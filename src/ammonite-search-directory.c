/*
 * SPDX-FileCopyrightText: 2005 Novell, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 */

#include "ammonite-search-directory.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <string.h>

#include "ammonite-directory-private.h"
#include "ammonite-file-private.h"
#include "ammonite-file-utilities.h"
#include "ammonite-file.h"
#include "ammonite-query.h"
#include "ammonite-scheme.h"
#include "ammonite-search-directory-file.h"
#include "ammonite-search-engine.h"
#include "ammonite-search-hit.h"

struct _AmmoniteSearchDirectory
{
    AmmoniteDirectory parent_instance;

    AmmoniteQuery *query;

    AmmoniteSearchEngine *engine;

    gboolean search_running;
    /* When the search directory is stopped or cancelled, we might wait
     * until all data and signals from previous search are stopped and removed
     * from the search engine. While this situation happens we don't want to connect
     * clients to our signals, and we will wait until the search data and signals
     * are valid and ready.
     * The worst thing that can happens if we don't do this is that new clients
     * might get the information of old searches if they are waiting_for_file_list.
     * But that shouldn't be a big deal since old clients have the old information.
     * But anyway it's currently unused for this case since the only client is
     * ammonite-view and is not waiting_for_file_list :) .
     *
     * The other use case is for letting clients know if information of the directory
     * is outdated or not valid. This might happens for automatic
     * scheduled timeouts. */
    gboolean search_ready_and_valid;

    GHashTable *files_hash;

    GList *monitor_list;
    GList *callback_list;
    GList *pending_callback_list;

    AmmoniteDirectory *base_model;
};

typedef struct
{
    gboolean monitor_hidden_files;
    AmmoniteAttributes monitor_attributes;

    gconstpointer client;
} SearchMonitor;

typedef struct
{
    AmmoniteSearchDirectory *search_directory;

    AmmoniteDirectoryCallback callback;
    gpointer callback_data;

    AmmoniteAttributes wait_for_attributes;
    GList *file_list;
    GHashTable *non_ready_hash;
} SearchCallback;

enum
{
    PROP_0,
    PROP_QUERY,
    NUM_PROPERTIES
};

G_DEFINE_FINAL_TYPE_WITH_CODE (AmmoniteSearchDirectory, ammonite_search_directory, AMMONITE_TYPE_DIRECTORY,
                               ammonite_ensure_extension_points ();
                               g_io_extension_point_implement (AMMONITE_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                               g_define_type_id,
                                                               AMMONITE_SEARCH_DIRECTORY_PROVIDER_NAME,
                                                               0))

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void search_callback_file_ready_callback (AmmoniteFile *file,
                                                 gpointer      data);
static void file_changed (AmmoniteFile            *file,
                          AmmoniteSearchDirectory *self);

static void
reset_file_list (AmmoniteSearchDirectory *self)
{
    GHashTableIter hash_iter;
    AmmoniteFile *file;

    g_hash_table_iter_init (&hash_iter, self->files_hash);

    /* Remove file connections */
    while (g_hash_table_iter_next (&hash_iter, (gpointer *) &file, NULL))
    {
        /* Disconnect change handler */
        g_signal_handlers_disconnect_by_func (file, file_changed, self);

        /* Remove monitors */
        for (GList *monitor_list = self->monitor_list;
             monitor_list != NULL;
             monitor_list = monitor_list->next)
        {
            SearchMonitor *monitor = monitor_list->data;

            ammonite_file_monitor_remove (file, monitor);
        }
    }

    g_hash_table_remove_all (self->files_hash);
}

static gboolean
is_monitoring_hidden_files (AmmoniteSearchDirectory *self)
{
    for (GList *l = self->monitor_list; l != NULL; l = l->next)
    {
        SearchMonitor *monitor = l->data;

        if (monitor->monitor_hidden_files)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void
start_search (AmmoniteSearchDirectory *self)
{
    if (self->query == NULL ||
        self->search_running ||
        (self->monitor_list == NULL && self->pending_callback_list == NULL))
    {
        return;
    }

    /* We need to start the search engine */
    self->search_running = TRUE;
    self->search_ready_and_valid = FALSE;

    ammonite_query_set_show_hidden_files (self->query,
                                          is_monitoring_hidden_files (self));

    reset_file_list (self);
    ammonite_search_engine_start (self->engine, self->query);
}

static void
stop_search (AmmoniteSearchDirectory *self)
{
    if (!self->search_running)
    {
        return;
    }

    self->search_running = FALSE;
    ammonite_search_engine_stop (self->engine);

    reset_file_list (self);
}

static void
file_changed (AmmoniteFile            *file,
              AmmoniteSearchDirectory *self)
{
    ammonite_directory_emit_files_changed (AMMONITE_DIRECTORY (self),
                                           &(AmmoniteFileList){ .data = file });
}

static void
search_monitor_add (AmmoniteDirectory         *directory,
                    gconstpointer              client,
                    gboolean                   monitor_hidden_files,
                    AmmoniteAttributes         attributes,
                    AmmoniteDirectoryCallback  callback,
                    gpointer                   callback_data)
{
    g_autoptr (GList) files_list = NULL;
    GList *list;
    SearchMonitor *monitor;
    AmmoniteSearchDirectory *self;
    AmmoniteFile *file;

    self = AMMONITE_SEARCH_DIRECTORY (directory);

    monitor = g_new0 (SearchMonitor, 1);
    monitor->monitor_hidden_files = monitor_hidden_files;
    monitor->monitor_attributes = attributes;
    monitor->client = client;

    self->monitor_list = g_list_prepend (self->monitor_list, monitor);
    files_list = g_hash_table_get_keys (self->files_hash);

    if (callback != NULL)
    {
        (*callback)(directory, files_list, callback_data);
    }

    for (list = files_list; list != NULL; list = list->next)
    {
        file = list->data;

        /* Add monitors */
        ammonite_file_monitor_add (file, monitor, attributes);
    }

    start_search (self);
}

static void
search_monitor_remove_file_monitors (SearchMonitor           *monitor,
                                     AmmoniteSearchDirectory *self)
{
    g_autoptr (GList) files_list = g_hash_table_get_keys (self->files_hash);

    for (GList *list = files_list; list != NULL; list = list->next)
    {
        AmmoniteFile *file = list->data;

        ammonite_file_monitor_remove (file, monitor);
    }
}

static void
search_monitor_destroy (SearchMonitor           *monitor,
                        AmmoniteSearchDirectory *self)
{
    search_monitor_remove_file_monitors (monitor, self);

    g_free (monitor);
}

static void
search_monitor_remove (AmmoniteDirectory *directory,
                       gconstpointer      client)
{
    AmmoniteSearchDirectory *self;
    SearchMonitor *monitor;
    GList *list;

    self = AMMONITE_SEARCH_DIRECTORY (directory);

    for (list = self->monitor_list; list != NULL; list = list->next)
    {
        monitor = list->data;

        if (monitor->client == client)
        {
            self->monitor_list = g_list_delete_link (self->monitor_list, list);

            search_monitor_destroy (monitor, self);

            break;
        }
    }

    if (!self->monitor_list)
    {
        stop_search (self);
    }
}

static void
cancel_call_when_ready (gpointer key,
                        gpointer value,
                        gpointer user_data)
{
    SearchCallback *search_callback;
    AmmoniteFile *file;

    file = key;
    search_callback = user_data;

    ammonite_file_cancel_call_when_ready (file, search_callback_file_ready_callback,
                                          search_callback);
}

static void
search_callback_destroy (SearchCallback *search_callback)
{
    if (search_callback->non_ready_hash)
    {
        g_hash_table_foreach (search_callback->non_ready_hash, cancel_call_when_ready, search_callback);
        g_hash_table_destroy (search_callback->non_ready_hash);
    }

    ammonite_file_list_free (search_callback->file_list);

    g_free (search_callback);
}

static void
search_callback_invoke_and_destroy (SearchCallback *search_callback)
{
    search_callback->callback (AMMONITE_DIRECTORY (search_callback->search_directory),
                               search_callback->file_list,
                               search_callback->callback_data);

    search_callback->search_directory->callback_list =
        g_list_remove (search_callback->search_directory->callback_list, search_callback);

    search_callback_destroy (search_callback);
}

static void
search_callback_file_ready_callback (AmmoniteFile *file,
                                     gpointer      data)
{
    SearchCallback *search_callback = data;

    g_hash_table_remove (search_callback->non_ready_hash, file);

    if (g_hash_table_size (search_callback->non_ready_hash) == 0)
    {
        search_callback_invoke_and_destroy (search_callback);
    }
}

static void
search_callback_add_file_callbacks (SearchCallback *callback)
{
    for (GList *l = callback->file_list; l != NULL; l = l->next)
    {
        AmmoniteFile *file = l->data;

        ammonite_file_call_when_ready (file,
                                       callback->wait_for_attributes,
                                       search_callback_file_ready_callback,
                                       callback);
    }
}

static SearchCallback *
search_callback_find (AmmoniteSearchDirectory   *self,
                      AmmoniteDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    SearchCallback *search_callback;
    GList *list;

    for (list = self->callback_list; list != NULL; list = list->next)
    {
        search_callback = list->data;

        if (search_callback->callback == callback &&
            search_callback->callback_data == callback_data)
        {
            return search_callback;
        }
    }

    return NULL;
}

static SearchCallback *
search_callback_find_pending (AmmoniteSearchDirectory   *self,
                              AmmoniteDirectoryCallback  callback,
                              gpointer                   callback_data)
{
    SearchCallback *search_callback;
    GList *list;

    for (list = self->pending_callback_list; list != NULL; list = list->next)
    {
        search_callback = list->data;

        if (search_callback->callback == callback &&
            search_callback->callback_data == callback_data)
        {
            return search_callback;
        }
    }

    return NULL;
}

static GHashTable *
file_list_to_hash_table (GList *file_list)
{
    GList *list;
    GHashTable *table;

    if (!file_list)
    {
        return NULL;
    }

    table = g_hash_table_new (NULL, NULL);

    for (list = file_list; list != NULL; list = list->next)
    {
        g_hash_table_insert (table, list->data, list->data);
    }

    return table;
}

static void
search_call_when_ready (AmmoniteDirectory         *directory,
                        AmmoniteAttributes         attributes,
                        AmmoniteDirectoryCallback  callback,
                        gpointer                   callback_data)
{
    AmmoniteSearchDirectory *self;
    SearchCallback *search_callback;

    self = AMMONITE_SEARCH_DIRECTORY (directory);

    search_callback = search_callback_find (self, callback, callback_data);
    if (search_callback == NULL)
    {
        search_callback = search_callback_find_pending (self, callback, callback_data);
    }

    if (search_callback)
    {
        g_warning ("tried to add a new callback while an old one was pending");
        return;
    }

    search_callback = g_new0 (SearchCallback, 1);
    search_callback->search_directory = self;
    search_callback->callback = callback;
    search_callback->callback_data = callback_data;
    search_callback->wait_for_attributes = attributes;

    if (IS_ATTRIBUTE_SET (attributes, AMMONITE_ATTRIBUTE_FILE_LIST) &&
        !self->search_ready_and_valid)
    {
        /* Add it to the pending callback list, which will be
         * processed when the directory has valid data from the new
         * search and all data and signals from previous search is removed. */
        self->pending_callback_list =
            g_list_prepend (self->pending_callback_list, search_callback);

        /* We might need to start the search engine */
        start_search (self);
    }
    else
    {
        g_autoptr (GList) files_list = g_hash_table_get_keys (self->files_hash);

        search_callback->file_list = ammonite_file_list_copy (files_list);
        search_callback->non_ready_hash = file_list_to_hash_table (files_list);

        if (!search_callback->non_ready_hash)
        {
            /* If there are no ready files, we invoke the callback
             *  with an empty list.
             */
            search_callback_invoke_and_destroy (search_callback);
        }
        else
        {
            self->callback_list = g_list_prepend (self->callback_list, search_callback);
            search_callback_add_file_callbacks (search_callback);
        }
    }
}

static void
search_cancel_callback (AmmoniteDirectory         *directory,
                        AmmoniteDirectoryCallback  callback,
                        gpointer                   callback_data)
{
    AmmoniteSearchDirectory *self = AMMONITE_SEARCH_DIRECTORY (directory);
    SearchCallback *search_callback;

    if ((search_callback = search_callback_find (self, callback, callback_data)) != NULL)
    {
        self->callback_list = g_list_remove (self->callback_list, search_callback);

        search_callback_destroy (search_callback);
    }
    /* Check for a pending callback */
    else if ((search_callback = search_callback_find_pending (self, callback, callback_data)) != NULL)
    {
        self->pending_callback_list = g_list_remove (self->pending_callback_list, search_callback);

        search_callback_destroy (search_callback);
    }

    if (!self->callback_list && !self->pending_callback_list)
    {
        stop_search (self);
    }
}

static void
search_callback_add_pending_file_callbacks (SearchCallback *callback)
{
    g_autoptr (GList) files_list = g_hash_table_get_keys (callback->search_directory->files_hash);

    callback->file_list = ammonite_file_list_copy (files_list);
    callback->non_ready_hash = file_list_to_hash_table (files_list);

    search_callback_add_file_callbacks (callback);
}

static void
search_directory_add_pending_files_callbacks (AmmoniteSearchDirectory *self)
{
    /* Add all file callbacks */
    g_list_foreach (self->pending_callback_list,
                    (GFunc) search_callback_add_pending_file_callbacks, NULL);
    self->callback_list = g_list_concat (self->callback_list,
                                         self->pending_callback_list);

    g_list_free (self->pending_callback_list);
    self->pending_callback_list = NULL;
}

static void
on_search_directory_search_ready_and_valid (AmmoniteSearchDirectory *self)
{
    search_directory_add_pending_files_callbacks (self);
    self->search_ready_and_valid = TRUE;
}

static void
search_engine_hits_added (AmmoniteSearchEngine    *engine,
                          GPtrArray               *hits,
                          AmmoniteSearchDirectory *self)
{
    g_autoptr (GList) file_list = NULL;
    AmmoniteFile *file;
    g_autoptr (GDateTime) now = g_date_time_new_now_local ();
    SearchMonitor *monitor;
    GList *monitor_list;
    g_autoptr (GFile) query_location = ammonite_search_directory_get_search_location (self);

    for (guint i = 0; i < hits->len; i++)
    {
        AmmoniteSearchHit *hit = hits->pdata[i];
        const char *uri = ammonite_search_hit_get_uri (hit);
        AmmoniteFile *hit_file = ammonite_file_get_by_uri (uri);

        ammonite_search_hit_compute_scores (hit, now, query_location);
        ammonite_file_set_search_relevance (hit_file, ammonite_search_hit_get_relevance (hit));
        ammonite_file_set_search_fts_snippet (hit_file, ammonite_search_hit_get_fts_snippet (hit));

        for (monitor_list = self->monitor_list; monitor_list; monitor_list = monitor_list->next)
        {
            monitor = monitor_list->data;

            /* Add monitors */
            ammonite_file_monitor_add (hit_file, monitor, monitor->monitor_attributes);
        }

        g_signal_connect (hit_file, "changed", G_CALLBACK (file_changed), self);

        file_list = g_list_prepend (file_list, hit_file);
        g_hash_table_add (self->files_hash, g_steal_pointer (&hit_file));
    }

    ammonite_directory_emit_files_added (AMMONITE_DIRECTORY (self), file_list);

    file = ammonite_directory_get_corresponding_file (AMMONITE_DIRECTORY (self));
    ammonite_file_emit_changed (file);
    ammonite_file_unref (file);

    search_directory_add_pending_files_callbacks (self);
}

static void
search_engine_finished (AmmoniteSearchDirectory *self)
{
    /* This function does not get called when the search engine is restarted. */
    on_search_directory_search_ready_and_valid (self);
    ammonite_directory_emit_done_loading (AMMONITE_DIRECTORY (self));
}

static AmmoniteFile *
search_new_as_file (AmmoniteDirectory *directory)
{
    return g_object_new (AMMONITE_TYPE_SEARCH_DIRECTORY_FILE, "directory", directory, NULL);
}

static void
search_force_reload (AmmoniteDirectory *directory)
{
    AmmoniteSearchDirectory *self;
    AmmoniteFile *file;

    self = AMMONITE_SEARCH_DIRECTORY (directory);

    if (!self->query)
    {
        return;
    }

    self->search_ready_and_valid = FALSE;

    /* Remove file monitors */
    reset_file_list (self);
    stop_search (self);

    file = ammonite_directory_get_corresponding_file (directory);
    ammonite_file_invalidate_all_attributes (file);
    ammonite_file_unref (file);
}

static gboolean
search_are_all_files_seen (AmmoniteDirectory *directory)
{
    AmmoniteSearchDirectory *self;

    self = AMMONITE_SEARCH_DIRECTORY (directory);

    return (!self->query ||
            self->search_ready_and_valid);
}

static gboolean
search_contains_file (AmmoniteDirectory *directory,
                      AmmoniteFile      *file)
{
    AmmoniteSearchDirectory *self;

    self = AMMONITE_SEARCH_DIRECTORY (directory);
    return (g_hash_table_lookup (self->files_hash, file) != NULL);
}

static GList *
search_get_file_list (AmmoniteDirectory *directory)
{
    AmmoniteSearchDirectory *self = AMMONITE_SEARCH_DIRECTORY (directory);

    g_hash_table_foreach (self->files_hash, (GHFunc) ammonite_file_ref, NULL);

    return g_hash_table_get_keys (self->files_hash);
}


static gboolean
search_is_editable (AmmoniteDirectory *directory)
{
    return FALSE;
}

static gboolean
real_handles_location (GFile *location)
{
    return g_file_has_uri_scheme (location, SCHEME_SEARCH);
}

static void
search_set_property (GObject      *object,
                     guint         property_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
    AmmoniteSearchDirectory *self = AMMONITE_SEARCH_DIRECTORY (object);

    switch (property_id)
    {
        case PROP_QUERY:
        {
            ammonite_search_directory_set_query (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
search_get_property (GObject    *object,
                     guint       property_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
    AmmoniteSearchDirectory *self = AMMONITE_SEARCH_DIRECTORY (object);

    switch (property_id)
    {
        case PROP_QUERY:
        {
            g_value_set_object (value, ammonite_search_directory_get_query (self));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
clear_base_model (AmmoniteSearchDirectory *self)
{
    if (self->base_model != NULL)
    {
        ammonite_directory_file_monitor_remove (self->base_model,
                                                &self->base_model);
        g_clear_object (&self->base_model);
    }
}

static void
search_connect_engine (AmmoniteSearchDirectory *self)
{
    g_signal_connect (self->engine, "hits-added",
                      G_CALLBACK (search_engine_hits_added),
                      self);
    g_signal_connect_swapped (self->engine, "search-finished",
                              G_CALLBACK (search_engine_finished),
                              self);
}

static void
search_disconnect_engine (AmmoniteSearchDirectory *self)
{
    g_signal_handlers_disconnect_by_func (self->engine,
                                          search_engine_hits_added,
                                          self);
    g_signal_handlers_disconnect_by_func (self->engine,
                                          search_engine_finished,
                                          self);
}

static void
search_dispose (GObject *object)
{
    AmmoniteSearchDirectory *self;
    GList *list;

    self = AMMONITE_SEARCH_DIRECTORY (object);

    clear_base_model (self);

    /* Remove search monitors */
    if (self->monitor_list)
    {
        for (list = self->monitor_list; list != NULL; list = list->next)
        {
            search_monitor_destroy ((SearchMonitor *) list->data, self);
        }

        g_list_free (self->monitor_list);
        self->monitor_list = NULL;
    }

    reset_file_list (self);

    if (self->callback_list)
    {
        /* Remove callbacks */
        g_list_foreach (self->callback_list,
                        (GFunc) search_callback_destroy, NULL);
        g_list_free (self->callback_list);
        self->callback_list = NULL;
    }

    if (self->pending_callback_list)
    {
        g_list_foreach (self->pending_callback_list,
                        (GFunc) search_callback_destroy, NULL);
        g_list_free (self->pending_callback_list);
        self->pending_callback_list = NULL;
    }

    g_clear_object (&self->query);
    stop_search (self);
    search_disconnect_engine (self);

    g_clear_object (&self->engine);

    G_OBJECT_CLASS (ammonite_search_directory_parent_class)->dispose (object);
}

static void
search_finalize (GObject *object)
{
    AmmoniteSearchDirectory *self;

    self = AMMONITE_SEARCH_DIRECTORY (object);

    g_hash_table_destroy (self->files_hash);

    G_OBJECT_CLASS (ammonite_search_directory_parent_class)->finalize (object);
}

static void
ammonite_search_directory_init (AmmoniteSearchDirectory *self)
{
    self->query = NULL;
    self->files_hash = g_hash_table_new_full (NULL, NULL,
                                              g_object_unref, NULL);

    self->engine = ammonite_search_engine_new (AMMONITE_SEARCH_TYPE_FOLDER);
    search_connect_engine (self);
}

static void
ammonite_search_directory_class_init (AmmoniteSearchDirectoryClass *class)
{
    AmmoniteDirectoryClass *directory_class = AMMONITE_DIRECTORY_CLASS (class);
    GObjectClass *oclass = G_OBJECT_CLASS (class);

    oclass->dispose = search_dispose;
    oclass->finalize = search_finalize;
    oclass->get_property = search_get_property;
    oclass->set_property = search_set_property;

    directory_class->new_as_file = search_new_as_file;

    directory_class->are_all_files_seen = search_are_all_files_seen;
    directory_class->contains_file = search_contains_file;
    directory_class->force_reload = search_force_reload;
    directory_class->call_when_ready = search_call_when_ready;
    directory_class->cancel_callback = search_cancel_callback;

    directory_class->file_monitor_add = search_monitor_add;
    directory_class->file_monitor_remove = search_monitor_remove;

    directory_class->get_file_list = search_get_file_list;
    directory_class->is_editable = search_is_editable;
    directory_class->handles_location = real_handles_location;

    properties[PROP_QUERY] =
        g_param_spec_object ("query", NULL, NULL,
                             AMMONITE_TYPE_QUERY,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
update_base_model (AmmoniteSearchDirectory *self)
{
    g_autoptr (GFile) query_location = ammonite_search_directory_get_search_location (self);
    g_autoptr (AmmoniteDirectory) base_model = ammonite_directory_get (query_location);

    if (self->base_model != base_model)
    {
        clear_base_model (self);
        self->base_model = g_steal_pointer (&base_model);

        if (self->base_model != NULL)
        {
            ammonite_directory_file_monitor_add (self->base_model, &self->base_model,
                                                 TRUE, AMMONITE_ATTRIBUTE_INFO,
                                                 NULL, NULL);
        }
    }

    AmmoniteSearchType search_type = (self->base_model != NULL)
                                     ? AMMONITE_SEARCH_TYPE_FOLDER
                                     : AMMONITE_SEARCH_TYPE_GLOBAL;

    ammonite_search_engine_set_search_type (self->engine, search_type);
}

char *
ammonite_search_directory_generate_new_uri (void)
{
    static int counter = 0;

    return g_strdup_printf (SCHEME_SEARCH "://%d/", counter++);
}

void
ammonite_search_directory_set_query (AmmoniteSearchDirectory *self,
                                     AmmoniteQuery           *query)
{
    g_autoptr (AmmoniteFile) file = NULL;

    if (g_set_object (&self->query, query))
    {
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_QUERY]);
    }

    /* Even if the query is the same, its parameters might have changed, so we
     * ensure the base model and the display name are up-to-date. */
    update_base_model (self);

    file = ammonite_directory_get_existing_corresponding_file (AMMONITE_DIRECTORY (self));
    if (file != NULL)
    {
        ammonite_search_directory_file_update_display_name (AMMONITE_SEARCH_DIRECTORY_FILE (file));
    }
}

AmmoniteQuery *
ammonite_search_directory_get_query (AmmoniteSearchDirectory *self)
{
    return self->query;
}

GFile *
ammonite_search_directory_get_search_location (AmmoniteSearchDirectory *self)
{
    if (self->query != NULL)
    {
        return ammonite_query_get_location (self->query);
    }
    else
    {
        return NULL;
    }
}
