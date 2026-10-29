/*
 * SPDX-FileCopyrightText: 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ammonite-starred-directory.h"

#include "ammonite-directory-private.h"
#include "ammonite-file-utilities.h"
#include "ammonite-internal-place-file.h"
#include "ammonite-tag-manager.h"
#include "ammonite-scheme.h"

struct _AmmoniteFavoriteDirectory
{
    AmmoniteDirectory parent_slot;

    GList *files;

    GList *monitor_list;
    GList *callback_list;
    GList *pending_callback_list;
};

typedef struct
{
    gboolean monitor_hidden_files;
    AmmoniteAttributes monitor_attributes;

    gconstpointer client;
} FavoriteMonitor;

typedef struct
{
    AmmoniteFavoriteDirectory *starred_directory;

    AmmoniteDirectoryCallback callback;
    gpointer callback_data;

    GList *file_list;
} FavoriteCallback;

G_DEFINE_FINAL_TYPE_WITH_CODE (AmmoniteFavoriteDirectory, ammonite_starred_directory, AMMONITE_TYPE_DIRECTORY,
                               ammonite_ensure_extension_points ();
                               g_io_extension_point_implement (AMMONITE_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                               g_define_type_id,
                                                               AMMONITE_STARRED_DIRECTORY_PROVIDER_NAME,
                                                               0))

static void
file_changed (AmmoniteFile              *file,
              AmmoniteFavoriteDirectory *starred)
{
    ammonite_directory_emit_files_changed (AMMONITE_DIRECTORY (starred),
                                           &(AmmoniteFileList) { .data = file });
}

static void
disconnect_and_unmonitor_file (AmmoniteFile              *file,
                               AmmoniteFavoriteDirectory *self)
{
    /* Disconnect change handler */
    g_signal_handlers_disconnect_by_func (file, file_changed, self);

    /* Remove monitors */
    for (GList *m = self->monitor_list; m != NULL; m = m->next)
    {
        ammonite_file_monitor_remove (file, m->data);
    }
}

static void
ammonite_starred_directory_update_files (AmmoniteFavoriteDirectory *self,
                                         GList                     *changed_files)
{
    AmmoniteTagManager *tag_manager = ammonite_tag_manager_get ();
    GList *monitor_list;
    FavoriteMonitor *monitor;
    g_autoptr (GHashTable) uri_table = NULL;
    GList *next;
    g_autolist (AmmoniteFile) files_added = NULL;
    g_autolist (AmmoniteFile) files_removed = NULL;

    uri_table = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       (GDestroyNotify) g_free,
                                       NULL);

    for (GList *l = self->files; l != NULL; l = l->next)
    {
        g_hash_table_add (uri_table, ammonite_file_get_uri (AMMONITE_FILE (l->data)));
    }

    for (GList *l = changed_files; l != NULL; l = next)
    {
        AmmoniteFile *file = l->data;
        g_autofree char *uri = ammonite_file_get_uri (file);

        next = l->next;
        if (g_hash_table_contains (uri_table, uri) &&
            !ammonite_tag_manager_file_is_starred (tag_manager, uri))
        {
            disconnect_and_unmonitor_file (file, self);
            self->files = g_list_remove (self->files, file);
            files_removed = g_list_prepend (files_removed, g_steal_pointer (&file));
        }
        else if (!g_hash_table_contains (uri_table, uri) &&
                 ammonite_tag_manager_file_is_starred (tag_manager, uri))
        {
            for (monitor_list = self->monitor_list; monitor_list; monitor_list = monitor_list->next)
            {
                monitor = monitor_list->data;

                /* Add monitors */
                ammonite_file_monitor_add (file, monitor, monitor->monitor_attributes);
            }

            g_signal_connect (file, "changed", G_CALLBACK (file_changed), self);

            files_added = g_list_prepend (files_added, ammonite_file_ref (file));
            self->files = g_list_prepend (self->files, ammonite_file_ref (file));
        }
        else
        {
            /* This may happen if we have moved the file and updated the starred
             * database for the new URI. In that case, the AmmoniteFile instance
             * has already updated its own URI. There's no need to re-add it. */
        }
    }

    if (files_added)
    {
        ammonite_directory_emit_files_added (AMMONITE_DIRECTORY (self), files_added);
    }

    if (files_removed)
    {
        ammonite_directory_emit_files_changed (AMMONITE_DIRECTORY (self), files_removed);
    }
}

static void
on_starred_files_changed (AmmoniteTagManager *tag_manager,
                          GList              *changed_files,
                          gpointer            user_data)
{
    AmmoniteFavoriteDirectory *self;

    self = AMMONITE_STARRED_DIRECTORY (user_data);

    ammonite_starred_directory_update_files (self, changed_files);
}

static AmmoniteFile *
real_new_as_file (AmmoniteDirectory *directory)
{
    return g_object_new (AMMONITE_TYPE_INTERNAL_PLACE_FILE, "directory", directory, NULL);
}

static gboolean
real_contains_file (AmmoniteDirectory *directory,
                    AmmoniteFile      *file)
{
    g_autofree gchar *uri = NULL;

    uri = ammonite_file_get_uri (file);

    return ammonite_tag_manager_file_is_starred (ammonite_tag_manager_get (), uri);
}

static gboolean
real_is_editable (AmmoniteDirectory *directory)
{
    return FALSE;
}

static void
real_call_when_ready (AmmoniteDirectory         *directory,
                      AmmoniteAttributes         attributes,
                      AmmoniteDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    GList *file_list;
    AmmoniteFavoriteDirectory *starred;

    starred = AMMONITE_STARRED_DIRECTORY (directory);

    file_list = ammonite_file_list_copy (starred->files);

    callback (AMMONITE_DIRECTORY (directory),
              file_list,
              callback_data);

    ammonite_file_list_free (file_list);
}

static gboolean
real_are_all_files_seen (AmmoniteDirectory *directory)
{
    return TRUE;
}

static void
real_file_monitor_add (AmmoniteDirectory         *directory,
                       gconstpointer              client,
                       gboolean                   monitor_hidden_files,
                       AmmoniteAttributes         attributes,
                       AmmoniteDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    GList *list;
    FavoriteMonitor *monitor;
    AmmoniteFavoriteDirectory *starred;
    AmmoniteFile *file;

    starred = AMMONITE_STARRED_DIRECTORY (directory);

    monitor = g_new0 (FavoriteMonitor, 1);
    monitor->monitor_hidden_files = monitor_hidden_files;
    monitor->monitor_attributes = attributes;
    monitor->client = client;

    starred->monitor_list = g_list_prepend (starred->monitor_list, monitor);

    if (callback != NULL)
    {
        (*callback)(directory, starred->files, callback_data);
    }

    for (list = starred->files; list != NULL; list = list->next)
    {
        file = list->data;

        /* Add monitors */
        ammonite_file_monitor_add (file, monitor, attributes);
    }
}

static void
starred_monitor_destroy (FavoriteMonitor           *monitor,
                         AmmoniteFavoriteDirectory *starred)
{
    GList *l;
    AmmoniteFile *file;

    for (l = starred->files; l != NULL; l = l->next)
    {
        file = l->data;

        ammonite_file_monitor_remove (file, monitor);
    }

    g_free (monitor);
}

static void
real_monitor_remove (AmmoniteDirectory *directory,
                     gconstpointer      client)
{
    AmmoniteFavoriteDirectory *starred;
    FavoriteMonitor *monitor;
    GList *list;

    starred = AMMONITE_STARRED_DIRECTORY (directory);

    for (list = starred->monitor_list; list != NULL; list = list->next)
    {
        monitor = list->data;

        if (monitor->client != client)
        {
            continue;
        }

        starred->monitor_list = g_list_delete_link (starred->monitor_list, list);

        starred_monitor_destroy (monitor, starred);

        break;
    }
}

static gboolean
real_handles_location (GFile *location)
{
    return g_file_has_uri_scheme (location, SCHEME_STARRED);
}

static FavoriteCallback *
starred_callback_find_pending (AmmoniteFavoriteDirectory *starred,
                               AmmoniteDirectoryCallback  callback,
                               gpointer                   callback_data)
{
    FavoriteCallback *starred_callback;
    GList *list;

    for (list = starred->pending_callback_list; list != NULL; list = list->next)
    {
        starred_callback = list->data;

        if (starred_callback->callback == callback &&
            starred_callback->callback_data == callback_data)
        {
            return starred_callback;
        }
    }

    return NULL;
}

static FavoriteCallback *
starred_callback_find (AmmoniteFavoriteDirectory *starred,
                       AmmoniteDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    FavoriteCallback *starred_callback;
    GList *list;

    for (list = starred->callback_list; list != NULL; list = list->next)
    {
        starred_callback = list->data;

        if (starred_callback->callback == callback &&
            starred_callback->callback_data == callback_data)
        {
            return starred_callback;
        }
    }

    return NULL;
}

static void
starred_callback_destroy (FavoriteCallback *starred_callback)
{
    ammonite_file_list_free (starred_callback->file_list);

    g_free (starred_callback);
}

static void
real_cancel_callback (AmmoniteDirectory         *directory,
                      AmmoniteDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    AmmoniteFavoriteDirectory *starred;
    FavoriteCallback *starred_callback;

    starred = AMMONITE_STARRED_DIRECTORY (directory);
    starred_callback = starred_callback_find (starred, callback, callback_data);

    if (starred_callback)
    {
        starred->callback_list = g_list_remove (starred->callback_list, starred_callback);

        starred_callback_destroy (starred_callback);

        return;
    }

    /* Check for a pending callback */
    starred_callback = starred_callback_find_pending (starred, callback, callback_data);

    if (starred_callback)
    {
        starred->pending_callback_list = g_list_remove (starred->pending_callback_list, starred_callback);

        starred_callback_destroy (starred_callback);
    }
}

static GList *
real_get_file_list (AmmoniteDirectory *directory)
{
    AmmoniteFavoriteDirectory *starred;

    starred = AMMONITE_STARRED_DIRECTORY (directory);

    return ammonite_file_list_copy (starred->files);
}

static void
ammonite_starred_directory_set_files (AmmoniteFavoriteDirectory *self)
{
    g_autolist (AmmoniteFile) starred_files = NULL;
    GList *l;
    GList *file_list;
    FavoriteMonitor *monitor;
    GList *monitor_list;

    file_list = NULL;

    starred_files = ammonite_tag_manager_get_starred_files (ammonite_tag_manager_get ());

    for (l = starred_files; l != NULL; l = l->next)
    {
        AmmoniteFile *file = l->data;

        g_signal_connect (file, "changed", G_CALLBACK (file_changed), self);

        for (monitor_list = self->monitor_list; monitor_list; monitor_list = monitor_list->next)
        {
            monitor = monitor_list->data;

            /* Add monitors */
            ammonite_file_monitor_add (file, monitor, monitor->monitor_attributes);
        }

        file_list = g_list_prepend (file_list, ammonite_file_ref (file));
    }

    self->files = file_list;
}

static void
real_force_reload (AmmoniteDirectory *directory)
{
    AmmoniteFavoriteDirectory *self = AMMONITE_STARRED_DIRECTORY (directory);

    /* Unset current file list */
    g_list_foreach (self->files, (GFunc) disconnect_and_unmonitor_file, self);
    g_clear_list (&self->files, g_object_unref);

    /* Set a fresh file list  */
    ammonite_starred_directory_set_files (self);
}

static void
ammonite_starred_directory_finalize (GObject *object)
{
    AmmoniteFavoriteDirectory *self;

    self = AMMONITE_STARRED_DIRECTORY (object);

    g_signal_handlers_disconnect_by_func (ammonite_tag_manager_get (),
                                          on_starred_files_changed,
                                          self);

    ammonite_file_list_free (self->files);

    G_OBJECT_CLASS (ammonite_starred_directory_parent_class)->finalize (object);
}

static void
ammonite_starred_directory_dispose (GObject *object)
{
    AmmoniteFavoriteDirectory *starred;
    GList *l;

    starred = AMMONITE_STARRED_DIRECTORY (object);

    /* Remove file connections */
    g_list_foreach (starred->files, (GFunc) disconnect_and_unmonitor_file, starred);

    /* Remove search monitors */
    if (starred->monitor_list)
    {
        for (l = starred->monitor_list; l != NULL; l = l->next)
        {
            starred_monitor_destroy ((FavoriteMonitor *) l->data, starred);
        }

        g_list_free (starred->monitor_list);
        starred->monitor_list = NULL;
    }

    G_OBJECT_CLASS (ammonite_starred_directory_parent_class)->dispose (object);
}

static void
ammonite_starred_directory_class_init (AmmoniteFavoriteDirectoryClass *klass)
{
    GObjectClass *oclass;
    AmmoniteDirectoryClass *directory_class;

    oclass = G_OBJECT_CLASS (klass);
    directory_class = AMMONITE_DIRECTORY_CLASS (klass);

    oclass->finalize = ammonite_starred_directory_finalize;
    oclass->dispose = ammonite_starred_directory_dispose;

    directory_class->new_as_file = real_new_as_file;
    directory_class->handles_location = real_handles_location;
    directory_class->contains_file = real_contains_file;
    directory_class->is_editable = real_is_editable;
    directory_class->force_reload = real_force_reload;
    directory_class->call_when_ready = real_call_when_ready;
    directory_class->are_all_files_seen = real_are_all_files_seen;
    directory_class->file_monitor_add = real_file_monitor_add;
    directory_class->file_monitor_remove = real_monitor_remove;
    directory_class->cancel_callback = real_cancel_callback;
    directory_class->get_file_list = real_get_file_list;
}

AmmoniteFavoriteDirectory *
ammonite_starred_directory_new (void)
{
    AmmoniteFavoriteDirectory *self;

    self = g_object_new (AMMONITE_TYPE_STARRED_DIRECTORY, NULL);

    return self;
}

static void
ammonite_starred_directory_init (AmmoniteFavoriteDirectory *self)
{
    g_signal_connect (ammonite_tag_manager_get (),
                      "starred-changed",
                      (GCallback) on_starred_files_changed,
                      self);

    ammonite_starred_directory_set_files (self);
}
