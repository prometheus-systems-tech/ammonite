/*
 * SPDX-FileCopyrightText: 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 * SPDX-FileCopyrightText: 2020 Sam Thursfield <sam@afuera.me.uk>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#define G_LOG_DOMAIN "ammonite-tag-manager"

#include "ammonite-tag-manager.h"
#include "ammonite-file.h"
#include "ammonite-file-undo-operations.h"
#include "ammonite-file-undo-manager.h"

#include <glib/gi18n.h>
#include <tinysparql.h>

#include "config.h"

struct _AmmoniteTagManager
{
    GObject object;

    gboolean database_ok;
    TrackerSparqlConnection *db;
    TrackerNotifier *notifier;

    TrackerSparqlStatement *query_starred_files;
    TrackerSparqlStatement *query_file_is_starred;

    GHashTable *starred_file_uris;
    GFile *home;

    GList *pending_changed_files;

    GCancellable *cancellable;
};

G_DEFINE_FINAL_TYPE (AmmoniteTagManager, ammonite_tag_manager, G_TYPE_OBJECT);

static AmmoniteTagManager *tag_manager = NULL;

/* See ammonite_tag_manager_new_dummy() documentation for details. */
static gboolean make_dummy_instance = FALSE;

typedef struct
{
    AmmoniteTagManager *tag_manager;
    GTask *task;
    GList *selection;
    gboolean star;
} UpdateData;

enum
{
    STARRED_CHANGED,
    LAST_SIGNAL
};

#define QUERY_STARRED_FILES \
        "SELECT ?file " \
        "{ " \
        "    ?file a ammonite:File ; " \
        "        ammonite:starred true . " \
        "}"

#define QUERY_FILE_IS_STARRED \
        "ASK " \
        "{ " \
        "    ~file a ammonite:File ; " \
        "        ammonite:starred true . " \
        "}"

static guint signals[LAST_SIGNAL];

static void
inform_no_localsearch_connection_once (void)
{
    static gboolean done = FALSE;

    if (G_UNLIKELY (!done))
    {
        done = TRUE;

        g_message ("No Localsearch connection");
    }
}

static void
start_query_or_update (TrackerSparqlConnection *db,
                       GString                 *query,
                       GAsyncReadyCallback      callback,
                       gpointer                 user_data,
                       gboolean                 is_query,
                       GCancellable            *cancellable)
{
    if (!db)
    {
        inform_no_localsearch_connection_once ();
        return;
    }

    if (is_query)
    {
        tracker_sparql_connection_query_async (db,
                                               query->str,
                                               cancellable,
                                               callback,
                                               user_data);
    }
    else
    {
        tracker_sparql_connection_update_async (db,
                                                query->str,
                                                cancellable,
                                                callback,
                                                user_data);
    }
}

static void
on_update_callback (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    TrackerSparqlConnection *db;
    GError *error;
    UpdateData *data;

    data = user_data;

    error = NULL;

    db = TRACKER_SPARQL_CONNECTION (object);

    tracker_sparql_connection_update_finish (db, result, &error);

    if (error == NULL)
    {
        if (!ammonite_file_undo_manager_is_operating ())
        {
            AmmoniteFileUndoInfo *undo_info;

            undo_info = ammonite_file_undo_info_starred_new (data->selection, data->star);
            ammonite_file_undo_manager_set_action (undo_info);

            g_object_unref (undo_info);
        }

        g_task_return_boolean (data->task, TRUE);
        g_object_unref (data->task);
    }
    else if (error && error->code == G_IO_ERROR_CANCELLED)
    {
        g_error_free (error);
    }
    else
    {
        g_warning ("error updating tags: %s", error->message);
        g_task_return_error (data->task, error);
        g_object_unref (data->task);
    }

    ammonite_file_list_free (data->selection);
    g_free (data);
}

/**
 * ammonite_tag_manager_get_starred_files:
 * @self: The tag manager singleton
 *
 * Returns: (element-type gchar*) (transfer full): A list of the starred AmmoniteFile.
 */
GList *
ammonite_tag_manager_get_starred_files (AmmoniteTagManager *self)
{
    GHashTableIter starred_iter;
    gchar *starred_uri;
    GList *starred_files = NULL;

    g_hash_table_iter_init (&starred_iter, self->starred_file_uris);
    while (g_hash_table_iter_next (&starred_iter, (gpointer *) &starred_uri, NULL))
    {
        g_autoptr (GFile) location = g_file_new_for_uri (starred_uri);
        AmmoniteFile *file = ammonite_file_get (location);

        /* Skip files outside $HOME, because we don't support starring these yet.
         * See comment on ammonite_tag_manager_can_star_contents() */
        if (g_file_has_prefix (location, self->home))
        {
            starred_files = g_list_prepend (starred_files, file);
        }
    }

    return starred_files;
}

void
ammonite_tag_manager_announce_starred_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
    GtkAccessible *accessible = user_data;
    gtk_accessible_announce (accessible, _("starred"),
                             GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
}

void
ammonite_tag_manager_announce_unstarred_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
    GtkAccessible *accessible = user_data;
    gtk_accessible_announce (accessible, _("unstarred"),
                             GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
}

static void
on_get_starred_files_cursor_callback (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    g_autoptr (GError) error = NULL;
    const gchar *url;
    gboolean success;
    AmmoniteTagManager *self;
    AmmoniteFile *file;

    cursor = TRACKER_SPARQL_CURSOR (object);

    self = AMMONITE_TAG_MANAGER (user_data);

    success = tracker_sparql_cursor_next_finish (cursor, result, &error);

    if (!success)
    {
        if (error != NULL)
        {
            g_warning ("Error on getting all tags cursor callback: %s", error->message);
        }

        if (self->pending_changed_files != NULL)
        {
            g_signal_emit_by_name (self, "starred-changed", self->pending_changed_files);
            g_clear_pointer (&self->pending_changed_files, ammonite_file_list_free);
        }

        g_clear_object (&cursor);
        return;
    }

    url = tracker_sparql_cursor_get_string (cursor, 0, NULL);

    g_hash_table_add (self->starred_file_uris, g_strdup (url));

    file = ammonite_file_get_by_uri (url);

    if (file)
    {
        self->pending_changed_files = g_list_prepend (self->pending_changed_files, file);
    }
    else
    {
        g_debug ("File %s is starred but not found", url);
    }

    tracker_sparql_cursor_next_async (cursor,
                                      self->cancellable,
                                      on_get_starred_files_cursor_callback,
                                      self);
}

static void
on_get_starred_files_query_callback (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    g_autoptr (GError) error = NULL;
    TrackerSparqlStatement *statement;
    AmmoniteTagManager *self;

    self = AMMONITE_TAG_MANAGER (user_data);
    statement = TRACKER_SPARQL_STATEMENT (object);

    cursor = tracker_sparql_statement_execute_finish (statement,
                                                      result,
                                                      &error);

    if (error != NULL)
    {
        if (error->code != G_IO_ERROR_CANCELLED)
        {
            g_warning ("Error on getting starred files: %s", error->message);
        }
    }
    else
    {
        tracker_sparql_cursor_next_async (cursor,
                                          self->cancellable,
                                          on_get_starred_files_cursor_callback,
                                          user_data);
    }
}

static void
ammonite_tag_manager_query_starred_files (AmmoniteTagManager *self,
                                          GCancellable       *cancellable)
{
    if (!self->database_ok)
    {
        inform_no_localsearch_connection_once ();
        return;
    }

    tracker_sparql_statement_execute_async (self->query_starred_files,
                                            cancellable,
                                            on_get_starred_files_query_callback,
                                            self);
}

static GString *
ammonite_tag_manager_delete_tag (AmmoniteTagManager *self,
                                 GList              *selection)
{
    GString *query;
    AmmoniteFile *file;
    GList *l;

    query = g_string_new ("DELETE DATA {");

    for (l = selection; l != NULL; l = l->next)
    {
        g_autofree gchar *uri = NULL;

        file = l->data;

        uri = ammonite_file_get_uri (file);
        g_string_append_printf (query,
                                "    <%s> a ammonite:File ; "
                                "        ammonite:starred true . ",
                                uri);
    }

    g_string_append (query, "}");

    return query;
}

static GString *
ammonite_tag_manager_insert_tag (AmmoniteTagManager *self,
                                 GList              *selection)
{
    GString *query;
    AmmoniteFile *file;
    GList *l;

    query = g_string_new ("INSERT DATA {");

    for (l = selection; l != NULL; l = l->next)
    {
        g_autofree gchar *uri = NULL;

        file = l->data;

        uri = ammonite_file_get_uri (file);
        g_string_append_printf (query,
                                "    <%s> a ammonite:File ; "
                                "        ammonite:starred true . ",
                                uri);
    }

    g_string_append (query, "}");

    return query;
}

gboolean
ammonite_tag_manager_file_is_starred (AmmoniteTagManager *self,
                                      const gchar        *file_uri)
{
    return g_hash_table_contains (self->starred_file_uris, file_uri);
}

void
ammonite_tag_manager_star_files (AmmoniteTagManager  *self,
                                 GObject             *object,
                                 GList               *selection,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data,
                                 GCancellable        *cancellable)
{
    GString *query;
    GTask *task;
    UpdateData *update_data;

    if (g_getenv ("G_MESSAGES_DEBUG") != NULL)
    {
        g_debug ("Starring %i files", g_list_length (selection));
    }

    task = g_task_new (object, cancellable, callback, user_data);

    query = ammonite_tag_manager_insert_tag (self, selection);

    update_data = g_new0 (UpdateData, 1);
    update_data->task = task;
    update_data->tag_manager = self;
    update_data->selection = ammonite_file_list_copy (selection);
    update_data->star = TRUE;

    start_query_or_update (self->db,
                           query,
                           on_update_callback,
                           update_data,
                           FALSE,
                           cancellable);

    g_string_free (query, TRUE);
}

void
ammonite_tag_manager_unstar_files (AmmoniteTagManager  *self,
                                   GObject             *object,
                                   GList               *selection,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data,
                                   GCancellable        *cancellable)
{
    GString *query;
    GTask *task;
    UpdateData *update_data;

    if (g_getenv ("G_MESSAGES_DEBUG") != NULL)
    {
        g_debug ("Unstarring %i files", g_list_length (selection));
    }

    task = g_task_new (object, cancellable, callback, user_data);

    query = ammonite_tag_manager_delete_tag (self, selection);

    update_data = g_new0 (UpdateData, 1);
    update_data->task = task;
    update_data->tag_manager = self;
    update_data->selection = ammonite_file_list_copy (selection);
    update_data->star = FALSE;

    start_query_or_update (self->db,
                           query,
                           on_update_callback,
                           update_data,
                           FALSE,
                           cancellable);

    g_string_free (query, TRUE);
}

static void
on_tracker_notifier_events (TrackerNotifier *notifier,
                            gchar           *service,
                            gchar           *graph,
                            GPtrArray       *events,
                            gpointer         user_data)
{
    TrackerNotifierEvent *event;
    AmmoniteTagManager *self;
    const gchar *file_url;
    GError *error = NULL;
    TrackerSparqlCursor *cursor;
    gboolean query_has_results = FALSE;
    gboolean starred;
    GList *changed_files;
    AmmoniteFile *changed_file;

    self = AMMONITE_TAG_MANAGER (user_data);

    for (guint i = 0; i < events->len; i++)
    {
        event = g_ptr_array_index (events, i);

        file_url = tracker_notifier_event_get_urn (event);
        changed_file = NULL;

        g_debug ("Got event for file %s", file_url);

        tracker_sparql_statement_bind_string (self->query_file_is_starred, "file", file_url);
        cursor = tracker_sparql_statement_execute (self->query_file_is_starred,
                                                   NULL,
                                                   &error);

        if (cursor)
        {
            query_has_results = tracker_sparql_cursor_next (cursor, NULL, &error);
        }

        if (error || !cursor || !query_has_results)
        {
            g_warning ("Couldn't query the starred files database: '%s'", error ? error->message : "(null error)");
            g_clear_error (&error);
            return;
        }

        starred = tracker_sparql_cursor_get_boolean (cursor, 0);
        if (starred)
        {
            gboolean inserted = g_hash_table_add (self->starred_file_uris, g_strdup (file_url));

            if (inserted)
            {
                g_debug ("Added %s to starred files list", file_url);
                changed_file = ammonite_file_get_existing_by_uri (file_url);

                if (changed_file != NULL)
                {
                    g_object_notify (G_OBJECT (changed_file), "a11y-name");
                }
            }
        }
        else
        {
            gboolean removed = g_hash_table_remove (self->starred_file_uris, file_url);

            if (removed)
            {
                g_debug ("Removed %s from starred files list", file_url);
                changed_file = ammonite_file_get_existing_by_uri (file_url);

                if (changed_file != NULL)
                {
                    g_object_notify (G_OBJECT (changed_file), "a11y-name");
                }
            }
        }

        if (changed_file)
        {
            changed_files = g_list_prepend (NULL, changed_file);

            g_signal_emit_by_name (self, "starred-changed", changed_files);

            ammonite_file_list_free (changed_files);
        }

        g_object_unref (cursor);
    }
}

static void
ammonite_tag_manager_finalize (GObject *object)
{
    AmmoniteTagManager *self;

    self = AMMONITE_TAG_MANAGER (object);

    if (self->notifier != NULL)
    {
        g_signal_handlers_disconnect_by_func (self->notifier,
                                              G_CALLBACK (on_tracker_notifier_events),
                                              self);
    }

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_object (&self->notifier);
    g_clear_object (&self->db);
    g_clear_object (&self->query_file_is_starred);
    g_clear_object (&self->query_starred_files);
    g_clear_pointer (&self->pending_changed_files, ammonite_file_list_free);

    g_hash_table_destroy (self->starred_file_uris);
    g_clear_object (&self->home);

    G_OBJECT_CLASS (ammonite_tag_manager_parent_class)->finalize (object);
}

static void
ammonite_tag_manager_class_init (AmmoniteTagManagerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ammonite_tag_manager_finalize;

    signals[STARRED_CHANGED] = g_signal_new ("starred-changed",
                                             AMMONITE_TYPE_TAG_MANAGER,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL,
                                             NULL,
                                             g_cclosure_marshal_VOID__POINTER,
                                             G_TYPE_NONE,
                                             1,
                                             G_TYPE_POINTER);
}

/**
 * ammonite_tag_manager_new:
 *
 * Returns: (transfer full): the #AmmoniteTagManager singleton object.
 */
AmmoniteTagManager *
ammonite_tag_manager_new (void)
{
    if (tag_manager != NULL)
    {
        return g_object_ref (tag_manager);
    }

    tag_manager = g_object_new (AMMONITE_TYPE_TAG_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (tag_manager), (gpointer) & tag_manager);

    return tag_manager;
}

/**
 * ammonite_tag_manager_new_dummy:
 *
 * Creates a dummy tag manager without database.
 *
 * Useful only for tests where the tag manager is needed but not being tested
 * and we don't want to fail the tests due to irrelevant D-Bus failures.
 *
 * Returns: (transfer full): the #AmmoniteTagManager singleton object.
 */
AmmoniteTagManager *
ammonite_tag_manager_new_dummy (void)
{
    make_dummy_instance = TRUE;
    return ammonite_tag_manager_new ();
}

/**
 * ammonite_tag_manager_get:
 *
 * Returns: (transfer none): the #AmmoniteTagManager singleton object.
 */
AmmoniteTagManager *
ammonite_tag_manager_get (void)
{
    return tag_manager;
}

static gboolean
setup_database (AmmoniteTagManager  *self,
                GCancellable        *cancellable,
                GError             **error)
{
    const gchar *datadir;
    g_autofree gchar *store_path = NULL;
    g_autofree gchar *ontology_path = NULL;
    g_autoptr (GFile) store = NULL;
    g_autoptr (GFile) ontology = NULL;

    /* Open private database to store ammonite:starred property. */

    datadir = AMMONITE_DATADIR;

    store_path = g_build_filename (g_get_user_data_dir (), "ammonite", "tags", NULL);
    ontology_path = g_build_filename (datadir, "ontology", NULL);

    store = g_file_new_for_path (store_path);
    ontology = g_file_new_for_path (ontology_path);

    self->db = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
                                              store,
                                              ontology,
                                              cancellable,
                                              error);

    if (*error)
    {
        return FALSE;
    }

    /* Prepare reusable queries. */
    self->query_file_is_starred = tracker_sparql_connection_query_statement (self->db,
                                                                             QUERY_FILE_IS_STARRED,
                                                                             cancellable,
                                                                             error);

    if (*error)
    {
        return FALSE;
    }

    self->query_starred_files = tracker_sparql_connection_query_statement (self->db,
                                                                           QUERY_STARRED_FILES,
                                                                           cancellable,
                                                                           error);

    if (*error)
    {
        return FALSE;
    }

    return TRUE;
}

static void
ammonite_tag_manager_init (AmmoniteTagManager *self)
{
    g_autoptr (GError) error = NULL;

    self->starred_file_uris = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     (GDestroyNotify) g_free,
                                                     /* values are keys */
                                                     NULL);
    self->home = g_file_new_for_path (g_get_home_dir ());

    if (make_dummy_instance)
    {
        /* Skip database initiation for ammonite_tag_manager_new_dummy(). */
        return;
    }

    self->cancellable = g_cancellable_new ();
    self->database_ok = setup_database (self, self->cancellable, &error);
    if (error)
    {
        g_warning ("Unable to initialize tag manager: %s", error->message);
        return;
    }

    self->notifier = tracker_sparql_connection_create_notifier (self->db);

    ammonite_tag_manager_query_starred_files (self, self->cancellable);

    g_signal_connect (self->notifier,
                      "events",
                      G_CALLBACK (on_tracker_notifier_events),
                      self);
}

gboolean
ammonite_tag_manager_can_star_contents (AmmoniteTagManager *self,
                                        GFile              *directory)
{
    /* We only allow files to be starred inside the home directory for now.
     * This avoids the starred files database growing too big.
     */
    return g_file_has_prefix (directory, self->home) || g_file_equal (directory, self->home);
}

gboolean
ammonite_tag_manager_can_star_location (AmmoniteTagManager *self,
                                        GFile              *directory)
{
    /* We only allow files to be starred inside the home directory for now.
     * This avoids the starred files database growing too big.
     */
    return g_file_has_prefix (directory, self->home);
}

static void
update_moved_uris_callback (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
    g_autoptr (GError) error = NULL;

    tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (object),
                                             result,
                                             &error);

    if (error != NULL && error->code != G_IO_ERROR_CANCELLED)
    {
        g_warning ("Error updating moved uris: %s", error->message);
    }
}

/**
 * ammonite_tag_manager_update_moved_uris:
 * @self: The tag manager singleton
 * @src: The original location as a #GFile
 * @dest: The new location as a #GFile
 *
 * Checks whether the rename/move operation (@src to @dest) has modified
 * the URIs of any starred files, and updates the database accordingly.
 */
void
ammonite_tag_manager_update_moved_uris (AmmoniteTagManager *self,
                                        GFile              *src,
                                        GFile              *dest)
{
    GHashTableIter starred_iter;
    gchar *starred_uri;
    g_autoptr (GPtrArray) old_uris = NULL;
    g_autoptr (GPtrArray) new_uris = NULL;
    g_autoptr (GString) query = NULL;

    if (!self->database_ok)
    {
        inform_no_localsearch_connection_once ();
        return;
    }

    old_uris = g_ptr_array_new ();
    new_uris = g_ptr_array_new_with_free_func (g_free);

    g_hash_table_iter_init (&starred_iter, self->starred_file_uris);
    while (g_hash_table_iter_next (&starred_iter, (gpointer *) &starred_uri, NULL))
    {
        g_autoptr (GFile) starred_location = NULL;
        g_autofree gchar *relative_path = NULL;

        starred_location = g_file_new_for_uri (starred_uri);

        if (g_file_equal (starred_location, src))
        {
            /* The moved file/folder is starred */
            g_ptr_array_add (old_uris, starred_uri);
            g_ptr_array_add (new_uris, g_file_get_uri (dest));
            continue;
        }

        relative_path = g_file_get_relative_path (src, starred_location);
        if (relative_path != NULL)
        {
            /* The starred file/folder is descendant of the moved/renamed directory */
            g_autoptr (GFile) new_location = NULL;

            new_location = g_file_resolve_relative_path (dest, relative_path);

            g_ptr_array_add (old_uris, starred_uri);
            g_ptr_array_add (new_uris, g_file_get_uri (new_location));
        }
    }

    if (new_uris->len == 0)
    {
        /* No starred files are affected by this move/rename */
        return;
    }

    g_debug ("Updating moved URI for %i starred files", new_uris->len);

    query = g_string_new ("DELETE DATA {");

    for (guint i = 0; i < old_uris->len; i++)
    {
        gchar *old_uri = g_ptr_array_index (old_uris, i);
        g_string_append_printf (query,
                                "    <%s> a ammonite:File ; "
                                "        ammonite:starred true . ",
                                old_uri);
    }

    g_string_append (query, "} ; INSERT DATA {");

    for (guint i = 0; i < new_uris->len; i++)
    {
        gchar *new_uri = g_ptr_array_index (new_uris, i);
        g_string_append_printf (query,
                                "    <%s> a ammonite:File ; "
                                "        ammonite:starred true . ",
                                new_uri);
    }

    g_string_append (query, "}");

    tracker_sparql_connection_update_async (self->db,
                                            query->str,
                                            self->cancellable,
                                            update_moved_uris_callback,
                                            NULL);
}
