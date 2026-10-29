/*
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#include "ammonite-directory-private.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ammonite-directory-notify.h"
#include "ammonite-enums.h"
#include "ammonite-file-private.h"
#include "ammonite-global-preferences.h"
#include "ammonite-hash-queue.h"
#include "ammonite-monitor.h"
#include "ammonite-scheme.h"
#include "ammonite-vfs-directory.h"
#include "ammonite-vfs-file.h"

enum
{
    FILES_ADDED,
    FILES_CHANGED,
    DONE_LOADING,
    LOAD_ERROR,
    LAST_SIGNAL
};

enum
{
    PROP_LOCATION = 1,
    NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL];
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GHashTable *directories;

static AmmoniteDirectory *ammonite_directory_new (GFile *location);
static void               set_directory_location (AmmoniteDirectory *directory,
                                                  GFile             *location);

G_DEFINE_TYPE_WITH_PRIVATE (AmmoniteDirectory, ammonite_directory, G_TYPE_OBJECT);

static gboolean
real_contains_file (AmmoniteDirectory *self,
                    AmmoniteFile      *file)
{
    AmmoniteDirectory *directory;

    directory = ammonite_file_get_directory (file);

    return directory == self;
}

static gboolean
real_are_all_files_seen (AmmoniteDirectory *directory)
{
    return directory->details->directory_loaded;
}

static gboolean
real_is_not_empty (AmmoniteDirectory *directory)
{
    return directory->details->file_list != NULL;
}

static gboolean
is_not_tentative (AmmoniteFile *file,
                  gpointer      callback_data)
{
    /* Files that are not added yet will later be sent with the files_added
     * signal, and a user doing get_file_list + files_added monitoring will
     * then see the file twice */
    return file->details->got_file_info && file->details->is_added;
}

static GList *
real_get_file_list (AmmoniteDirectory *directory)
{
    AmmoniteFileList *file_list_copy = ammonite_file_list_copy (directory->details->file_list);

    return ammonite_file_list_filter (file_list_copy,
                                      is_not_tentative,
                                      NULL);
}

static gboolean
real_is_editable (AmmoniteDirectory *directory)
{
    return TRUE;
}

AmmoniteFile *
ammonite_directory_new_as_vfs_file (AmmoniteDirectory *directory)
{
    return g_object_new (AMMONITE_TYPE_VFS_FILE, "directory", directory, NULL);
}

static gboolean
real_handles_location (GFile *location)
{
    /* This class is the fallback on handling any location */
    return TRUE;
}

static void
ammonite_directory_finalize (GObject *object)
{
    AmmoniteDirectory *directory;

    directory = AMMONITE_DIRECTORY (object);

    g_hash_table_remove (directories, directory->details->location);

    ammonite_directory_cancel (directory);
    g_warn_if_fail (directory->details->count_in_progress == NULL);

    if (g_hash_table_size (directory->details->monitor_table) != 0)
    {
        GHashTableIter iter;
        gpointer value;

        g_warning ("destroying a AmmoniteDirectory while it's being monitored");

        g_hash_table_iter_init (&iter, directory->details->monitor_table);
        while (g_hash_table_iter_next (&iter, NULL, &value))
        {
            GList *list = value;
            g_list_free_full (list, g_free);
        }
        g_hash_table_remove_all (directory->details->monitor_table);
    }
    g_hash_table_destroy (directory->details->monitor_table);

    if (directory->details->monitor != NULL)
    {
        ammonite_monitor_cancel (directory->details->monitor);
    }

    if (directory->details->dequeue_pending_idle_id != 0)
    {
        g_source_remove (directory->details->dequeue_pending_idle_id);
    }

    if (directory->details->call_ready_idle_id != 0)
    {
        g_source_remove (directory->details->call_ready_idle_id);
    }

    if (directory->details->location)
    {
        g_object_unref (directory->details->location);
    }

    g_warn_if_fail (directory->details->file_list == NULL);
    g_hash_table_destroy (directory->details->file_hash);

    ammonite_hash_queue_destroy (directory->details->high_priority_queue);
    ammonite_hash_queue_destroy (directory->details->low_priority_queue);
    ammonite_hash_queue_destroy (directory->details->extension_queue);
    g_clear_pointer (&directory->details->call_when_ready_hash.unsatisfied, g_hash_table_unref);
    g_clear_pointer (&directory->details->call_when_ready_hash.ready, g_hash_table_unref);
    g_clear_list (&directory->details->files_changed_while_adding, g_object_unref);
    g_warn_if_fail (directory->details->directory_load_in_progress == NULL);
    g_warn_if_fail (directory->details->dequeue_pending_idle_id == 0);
    g_list_free_full (directory->details->pending_file_info, g_object_unref);

    G_OBJECT_CLASS (ammonite_directory_parent_class)->finalize (object);
}

static void
ammonite_directory_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    AmmoniteDirectory *directory = AMMONITE_DIRECTORY (object);

    switch (property_id)
    {
        case PROP_LOCATION:
        {
            set_directory_location (directory, g_value_get_object (value));
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
ammonite_directory_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    AmmoniteDirectory *directory = AMMONITE_DIRECTORY (object);

    switch (property_id)
    {
        case PROP_LOCATION:
        {
            g_value_set_object (value, directory->details->location);
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
ammonite_directory_class_init (AmmoniteDirectoryClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    klass->contains_file = real_contains_file;
    klass->are_all_files_seen = real_are_all_files_seen;
    klass->is_not_empty = real_is_not_empty;
    klass->get_file_list = real_get_file_list;
    klass->is_editable = real_is_editable;
    klass->new_as_file = ammonite_directory_new_as_vfs_file;
    klass->handles_location = real_handles_location;

    object_class->finalize = ammonite_directory_finalize;
    object_class->set_property = ammonite_directory_set_property;
    object_class->get_property = ammonite_directory_get_property;

    signals[FILES_ADDED] =
        g_signal_new ("files-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AmmoniteDirectoryClass, files_added),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[FILES_CHANGED] =
        g_signal_new ("files-changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AmmoniteDirectoryClass, files_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[DONE_LOADING] =
        g_signal_new ("done-loading",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AmmoniteDirectoryClass, done_loading),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[LOAD_ERROR] =
        g_signal_new ("load-error",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AmmoniteDirectoryClass, load_error),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);

    properties[PROP_LOCATION] =
        g_param_spec_object ("location",
                             "The location",
                             "The location of this directory",
                             G_TYPE_FILE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
ammonite_directory_init (AmmoniteDirectory *directory)
{
    directory->details = ammonite_directory_get_instance_private (directory);
    directory->details->file_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                           g_free, NULL);
    directory->details->high_priority_queue = ammonite_hash_queue_new (g_direct_hash, g_direct_equal, g_object_unref, NULL);
    directory->details->low_priority_queue = ammonite_hash_queue_new (g_direct_hash, g_direct_equal, g_object_unref, NULL);
    directory->details->extension_queue = ammonite_hash_queue_new (g_direct_hash, g_direct_equal, g_object_unref, NULL);
    directory->details->call_when_ready_hash.unsatisfied = g_hash_table_new (NULL, NULL);
    directory->details->call_when_ready_hash.ready = g_hash_table_new (NULL, NULL);
    directory->details->monitor_table = g_hash_table_new (NULL, NULL);
}

AmmoniteDirectory *
ammonite_directory_ref (AmmoniteDirectory *directory)
{
    if (directory == NULL)
    {
        return directory;
    }

    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), NULL);

    g_object_ref (directory);
    return directory;
}

void
ammonite_directory_unref (AmmoniteDirectory *directory)
{
    if (directory == NULL)
    {
        return;
    }

    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));

    g_object_unref (directory);
}

static void
filtering_changed_callback (gpointer callback_data)
{
    g_autolist (AmmoniteDirectory) dirs = g_hash_table_get_values (directories);
    g_list_foreach (dirs, (GFunc) g_object_ref, NULL);

    /* Preference about which items to show has changed, so we
     * can't trust any of our precomputed directory counts.
     */
    for (AmmoniteDirectoryList *l = dirs; l != NULL; l = l->next)
    {
        AmmoniteDirectory *directory = l->data;

        ammonite_directory_invalidate_count (directory);
    }
}

void
emit_change_signals_for_all_files (AmmoniteDirectory *directory)
{
    g_autolist (AmmoniteFile) files = NULL;

    files = ammonite_file_list_copy (directory->details->file_list);
    if (directory->details->as_file != NULL)
    {
        files = g_list_prepend (files, g_object_ref (directory->details->as_file));
    }

    ammonite_directory_emit_change_signals (directory, files);
}

void
emit_change_signals_for_all_files_in_all_directories (void)
{
    if (directories == NULL)
    {
        return;
    }

    g_autolist (AmmoniteDirectory) dirs = g_hash_table_get_values (directories);
    g_list_foreach (dirs, (GFunc) g_object_ref, NULL);

    for (AmmoniteDirectoryList *l = dirs; l != NULL; l = l->next)
    {
        AmmoniteDirectory *directory = l->data;
        emit_change_signals_for_all_files (directory);
    }
}

static void
async_data_preference_changed_callback (gpointer callback_data)
{
    g_autolist (AmmoniteDirectory) dirs = g_hash_table_get_values (directories);

    g_list_foreach (dirs, (GFunc) ammonite_directory_ref, NULL);

    /* Preference involving fetched async data has changed, so
     * we have to kick off refetching all async data, and tell
     * each file that it (might have) changed.
     */
    for (AmmoniteDirectoryList *l = dirs; l != NULL; l = l->next)
    {
        AmmoniteDirectory *directory = l->data;

        ammonite_directory_async_state_changed (directory);
        emit_change_signals_for_all_files (directory);
    }
}

static void
ensure_directories_hash_table (void)
{
    if (G_LIKELY (directories != NULL))
    {
        return;
    }

    /* Create a hash table to reuse existing directory objects */
    directories = g_hash_table_new (g_file_hash, (GCompareFunc) g_file_equal);

    ammonite_global_preferences_init ();

    g_signal_connect_swapped (gtk_filechooser_preferences,
                              "changed::" AMMONITE_PREFERENCES_SHOW_HIDDEN_FILES,
                              G_CALLBACK (filtering_changed_callback),
                              NULL);
    g_signal_connect_swapped (ammonite_preferences,
                              "changed::" AMMONITE_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
                              G_CALLBACK (async_data_preference_changed_callback),
                              NULL);
}

/**
 * lookup_existing:
 * @location: (nullable): location for which to lookup existing #AmmoniteDirectory
 */
static AmmoniteDirectory *
lookup_existing (GFile *location)
{
    if (location == NULL)
    {
        return NULL;
    }

    ensure_directories_hash_table ();

    return g_hash_table_lookup (directories, location);
}

/**
 * ammonite_directory_get_by_uri:
 * @uri: URI of directory to get.
 *
 * Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
AmmoniteDirectory *
ammonite_directory_get (GFile *location)
{
    if (location == NULL)
    {
        return NULL;
    }

    AmmoniteDirectory *directory = lookup_existing (location);

    if (directory != NULL)
    {
        return ammonite_directory_ref (directory);
    }

    /* Create a new directory object instead. */
    directory = ammonite_directory_new (location);

    /* Put it in the hash table. */
    g_hash_table_insert (directories, directory->details->location, directory);

    return directory;
}

AmmoniteDirectory *
ammonite_directory_get_existing (GFile *location)
{
    AmmoniteDirectory *directory = lookup_existing (location);

    return (directory != NULL) ? ammonite_directory_ref (directory) : NULL;
}


AmmoniteDirectory *
ammonite_directory_get_by_uri (const char *uri)
{
    AmmoniteDirectory *directory;
    GFile *location;

    if (uri == NULL)
    {
        return NULL;
    }

    location = g_file_new_for_uri (uri);

    directory = ammonite_directory_get (location);
    g_object_unref (location);
    return directory;
}

AmmoniteDirectory *
ammonite_directory_get_for_file (AmmoniteFile *file)
{
    char *uri;
    AmmoniteDirectory *directory;

    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    uri = ammonite_file_get_uri (file);
    directory = ammonite_directory_get_by_uri (uri);
    g_free (uri);
    return directory;
}

/* Returns a reffed AmmoniteFile object for this directory.
 */
AmmoniteFile *
ammonite_directory_get_corresponding_file (AmmoniteDirectory *directory)
{
    AmmoniteFile *file;

    file = ammonite_directory_get_existing_corresponding_file (directory);
    if (file == NULL)
    {
        g_autoptr (GFile) location = ammonite_directory_get_location (directory);
        file = ammonite_file_get (location);
    }

    return file;
}

/* Returns a reffed AmmoniteFile object for this directory, but only if the
 * AmmoniteFile object has already been created.
 */
AmmoniteFile *
ammonite_directory_get_existing_corresponding_file (AmmoniteDirectory *directory)
{
    AmmoniteFile *file;

    file = directory->details->as_file;
    if (file != NULL)
    {
        ammonite_file_ref (file);
        return file;
    }

    g_autoptr (GFile) location = ammonite_directory_get_location (directory);
    file = ammonite_file_get_existing (location);

    return file;
}

/* ammonite_directory_get_name_for_self_as_new_file:
 *
 * Get a name to display for the file representing this
 * directory. This is called only when there's no VFS
 * directory for this AmmoniteDirectory.
 */
char *
ammonite_directory_get_name_for_self_as_new_file (AmmoniteDirectory *directory)
{
    g_autofree char *directory_uri = NULL;
    g_autofree char *scheme = NULL;
    g_autofree char *hostname = NULL;

    directory_uri = ammonite_directory_get_uri (directory);

    g_uri_split (directory_uri, 0, &scheme, NULL, &hostname, NULL, NULL, NULL, NULL, NULL);
    if (hostname == NULL)
    {
        return g_steal_pointer (&directory_uri);
    }
    else if (scheme == NULL)
    {
        return g_steal_pointer (&hostname);
    }

    /* Translators: this is of the format "hostname (uri-scheme)" */
    return g_strdup_printf (_("%s (%s)"), hostname, scheme);
}

char *
ammonite_directory_get_uri (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), NULL);

    return g_file_get_uri (directory->details->location);
}

GFile *
ammonite_directory_get_location (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), NULL);

    return g_object_ref (directory->details->location);
}

AmmoniteFile *
ammonite_directory_new_as_file (AmmoniteDirectory *directory)
{
    AmmoniteDirectoryClass *dir_class = AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory));

    return dir_class->new_as_file (directory);
}

static GList *
ammonite_directory_provider_get_all (void)
{
    GIOExtensionPoint *extension_point;
    GList *extensions;

    extension_point = g_io_extension_point_lookup (AMMONITE_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME);
    if (extension_point == NULL)
    {
        g_warning ("Directory provider extension point not registered. Did you call ammonite_ensure_extension_points()?");
    }
    extensions = g_io_extension_point_get_extensions (extension_point);

    return extensions;
}

static AmmoniteDirectory *
ammonite_directory_new (GFile *location)
{
    GList *extensions = ammonite_directory_provider_get_all ();

    for (GList *l = extensions; l != NULL; l = l->next)
    {
        GIOExtension *gio_extension = l->data;
        AmmoniteDirectoryClass *current_provider_class =
            AMMONITE_DIRECTORY_CLASS (g_io_extension_ref_class (gio_extension));

        if (current_provider_class->handles_location (location))
        {
            return g_object_new (g_io_extension_get_type (gio_extension),
                                 "location", location,
                                 NULL);
        }
    }

    /* This class is the fallback for any location */
    return g_object_new (AMMONITE_TYPE_VFS_DIRECTORY,
                         "location", location,
                         NULL);
}

/**
 * ammonite_directory_is_local_or_fuse:
 *
 * @directory: a #AmmoniteDirectory
 *
 * Checks whether this directory contains files with local paths. Usually, this
 * means the local path can be obtained by calling g_file_get_path(). As an
 * exception, the local URI for files in recent:// can only be obtained from the
 * G_FILE_ATTRIBUTE_STANDARD_TARGET_URI attribute.
 *
 * Returns: %TRUE if a local path is known to be obtainable for all files in
 *          this directory. Otherwise, %FALSE.
 */
gboolean
ammonite_directory_is_local_or_fuse (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), FALSE);
    g_return_val_if_fail (directory->details->location, FALSE);


    if (ammonite_directory_is_in_recent (directory)
        || g_file_is_native (directory->details->location))
    {
        /* Native files have a local path by definition. The files in recent:/
         * have a local URI stored in the standard::target-uri attribute. */
        return TRUE;
    }
    else
    {
        /* Non-native files may have local paths in FUSE mounts. The only way to
         * know if that's the case is to test if GIO reports a path.
         */
        return (g_file_peek_path (directory->details->location) != NULL);
    }
}

gboolean
ammonite_directory_is_in_trash (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), FALSE);

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, SCHEME_TRASH);
}

gboolean
ammonite_directory_is_in_recent (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), FALSE);

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, SCHEME_RECENT);
}

gboolean
ammonite_directory_is_in_starred (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), FALSE);

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, SCHEME_STARRED);
}

gboolean
ammonite_directory_is_in_admin (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), FALSE);

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, SCHEME_ADMIN);
}

gboolean
ammonite_directory_are_all_files_seen (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), FALSE);

    return AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->are_all_files_seen (directory);
}

static void
add_to_hash_table (AmmoniteDirectory *directory,
                   AmmoniteFile      *file,
                   GList             *node)
{
    const char *name = ammonite_file_get_name (file);

    g_return_if_fail (name != NULL);
    g_return_if_fail (node != NULL);
    g_return_if_fail (g_hash_table_lookup (directory->details->file_hash,
                                           name) == NULL);

    g_hash_table_insert (directory->details->file_hash, g_strdup (name), node);
}

static GList *
extract_from_hash_table (AmmoniteDirectory *directory,
                         AmmoniteFile      *file)
{
    const char *name = ammonite_file_get_name (file);
    GList *node;

    if (name == NULL)
    {
        return NULL;
    }

    /* Find the list node in the hash table. */
    node = g_hash_table_lookup (directory->details->file_hash, name);
    g_hash_table_remove (directory->details->file_hash, name);

    return node;
}

void
ammonite_directory_add_file (AmmoniteDirectory *directory,
                             AmmoniteFile      *file)
{
    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));
    g_return_if_fail (AMMONITE_IS_FILE (file));

    /* Add to list. */
    GList *node = g_list_prepend (directory->details->file_list, file);
    directory->details->file_list = node;

    /* Add to hash table. */
    add_to_hash_table (directory, file, node);

    directory->details->confirmed_file_count++;

    gboolean add_to_work_queue = FALSE;
    if (ammonite_directory_is_file_list_monitored (directory))
    {
        /* Ref if we are monitoring, since monitoring owns the file list. */
        ammonite_file_ref (file);
        add_to_work_queue = TRUE;
    }
    else if (ammonite_directory_has_request_for_file (directory, file))
    {
        /* We're waiting for the file in a call_when_ready. Make sure
         *  we add the file to the work queue so that said waiter won't
         *  wait forever for e.g. all files in the directory to be done */
        add_to_work_queue = TRUE;
    }

    if (add_to_work_queue)
    {
        ammonite_directory_add_file_to_work_queue (directory, file);
    }
}

void
ammonite_directory_remove_file (AmmoniteDirectory *directory,
                                AmmoniteFile      *file)
{
    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));
    g_return_if_fail (AMMONITE_IS_FILE (file));

    /* Find the list node in the hash table. */
    GList *node = extract_from_hash_table (directory, file);
    g_return_if_fail (node != NULL);
    g_return_if_fail (node->data == file);

    /* Remove the item from the list. */
    directory->details->file_list = g_list_remove_link
                                        (directory->details->file_list, node);
    g_list_free_1 (node);

    ammonite_directory_remove_file_from_work_queue (directory, file);

    if (!file->details->unconfirmed)
    {
        directory->details->confirmed_file_count--;
    }

    /* Unref if we are monitoring. */
    if (ammonite_directory_is_file_list_monitored (directory))
    {
        ammonite_file_unref (file);
    }
}

GList *
ammonite_directory_begin_file_name_change (AmmoniteDirectory *directory,
                                           AmmoniteFile      *file)
{
    /* Find the list node in the hash table. */
    return extract_from_hash_table (directory, file);
}

void
ammonite_directory_end_file_name_change (AmmoniteDirectory *directory,
                                         AmmoniteFile      *file,
                                         GList             *node)
{
    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));
    g_return_if_fail (AMMONITE_IS_FILE (file));

    /* Add the list node to the hash table. */
    if (node != NULL)
    {
        add_to_hash_table (directory, file, node);
    }
}

AmmoniteFile *
ammonite_directory_find_file_by_name (AmmoniteDirectory *directory,
                                      const char        *name)
{
    GList *node;

    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    node = g_hash_table_lookup (directory->details->file_hash,
                                name);
    return node == NULL ? NULL : AMMONITE_FILE (node->data);
}

void
ammonite_directory_emit_files_added (AmmoniteDirectory *directory,
                                     GList             *added_files)
{
    if (added_files != NULL)
    {
        g_signal_emit (directory,
                       signals[FILES_ADDED], 0,
                       added_files);
    }
}

void
ammonite_directory_emit_files_changed (AmmoniteDirectory *directory,
                                       GList             *changed_files)
{
    if (changed_files != NULL)
    {
        g_signal_emit (directory,
                       signals[FILES_CHANGED], 0,
                       changed_files);
    }
}

void
ammonite_directory_emit_change_signals (AmmoniteDirectory *directory,
                                        AmmoniteFileList  *changed_files)
{
    for (AmmoniteFileList *p = changed_files; p != NULL; p = p->next)
    {
        ammonite_file_emit_changed (p->data);
    }
    ammonite_directory_emit_files_changed (directory, changed_files);
}

void
ammonite_directory_emit_done_loading (AmmoniteDirectory *directory)
{
    g_signal_emit (directory,
                   signals[DONE_LOADING], 0);
}

void
ammonite_directory_emit_load_error (AmmoniteDirectory *directory,
                                    GError            *error)
{
    g_signal_emit (directory,
                   signals[LOAD_ERROR], 0,
                   error);
}

/* Return a directory object for this one's parent. */
static AmmoniteDirectory *
get_parent_directory (GFile *location)
{
    AmmoniteDirectory *directory;
    GFile *parent;

    parent = g_file_get_parent (location);
    if (parent)
    {
        directory = ammonite_directory_get (parent);
        g_object_unref (parent);
        return directory;
    }
    return NULL;
}

static void
hash_table_list_insert (GHashTable    *table,
                        gconstpointer  key,
                        gpointer       data)
{
    GList *list = g_hash_table_lookup (table, key);

    if (list != NULL)
    {
        list = g_list_insert (list, data, 1);
    }
    else
    {
        g_hash_table_insert (table, (gpointer) key, g_list_prepend (NULL, data));
    }
}

static void
add_to_directory_work_queue (AmmoniteDirectory *directory,
                             GList             *file_list)
{
    for (GList *node = file_list; node != NULL; node = node->next)
    {
        AmmoniteFile *file = node->data;
        ammonite_directory_add_file_to_work_queue (directory, file);
    }
}

static void
notify_directory_changes (AmmoniteDirectory *directory,
                          GList             *file_list)
{
    ammonite_directory_async_state_changed (directory);
    ammonite_directory_emit_change_signals (directory, file_list);
}

static void
hash_table_add_uncontained (GHashTable        *hash_table,
                            AmmoniteDirectory *directory)
{
    if (directory != NULL && !g_hash_table_contains (hash_table, directory))
    {
        g_hash_table_add (hash_table, ammonite_directory_ref (directory));
    }
}

static void
file_list_free_full (GList *list)
{
    g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

void
ammonite_directory_notify_files_added (GList *files)
{
    /* Make a list of added files in each directory. */
    g_autoptr (GHashTable) added_lists =
        g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) file_list_free_full);

    /* Make a list of parent directories that will need their counts updated. */
    g_autoptr (GHashTable) parent_directories =
        g_hash_table_new_full (NULL, NULL, (GDestroyNotify) ammonite_directory_unref, NULL);

    for (GList *p = files; p != NULL; p = p->next)
    {
        GFile *location = p->data;
        g_autoptr (GFile) parent = g_file_get_parent (location);
        AmmoniteDirectory *directory = lookup_existing (parent);

        if (directory == NULL)
        {
            /* In case the directory is not being
             * monitored, but the corresponding file is,
             * we must invalidate its item count.
             */

            if (parent == NULL)
            {
                continue;
            }

            g_autoptr (AmmoniteFile) file = ammonite_file_get_existing (parent);

            if (file != NULL)
            {
                ammonite_file_invalidate_count (file);
            }

            continue;
        }

        hash_table_add_uncontained (parent_directories, directory);

        /* If no one is monitoring files in the directory, nothing to do. */
        if (!ammonite_directory_is_file_list_monitored (directory))
        {
            continue;
        }

        g_autoptr (AmmoniteFile) file = ammonite_file_get_existing (location);
        /* We check is_added here, because the file could have been added
         * to the directory by a ammonite_file_get() but not gotten
         * files_added emitted
         */
        if (file && file->details->is_added)
        {
            /* A file already exists, it was probably renamed.
             * If it was renamed this could be ignored, but
             * queue a change just in case */
            ammonite_file_changed (file);
        }
        else
        {
            hash_table_list_insert (added_lists, directory, g_object_ref (location));
        }
    }

    /* Now get file info for the new files. This creates AmmoniteFile
     * objects for the new files, and sends out a files_added signal.
     */
    g_hash_table_foreach (added_lists, (GHFunc) ammonite_directory_get_info_for_new_files, NULL);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, (GHFunc) ammonite_directory_invalidate_count, NULL);
}

void
ammonite_directory_notify_files_changed (GList *files)
{
    /* Make a list of changed files in each directory. */
    g_autoptr (GHashTable) changed_lists =
        g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) ammonite_file_list_free);

    /* Go through all the notifications. */
    for (GList *node = files; node != NULL; node = node->next)
    {
        GFile *location = node->data;

        /* Find the file. */
        g_autoptr (AmmoniteFile) file = ammonite_file_get_existing (location);
        if (file != NULL)
        {
            AmmoniteDirectory *directory = ammonite_file_get_directory (file);

            /* Tell it to re-get info now, and later emit
             * a changed signal.
             */
            file->details->file_info_is_up_to_date = FALSE;
            ammonite_file_invalidate_extension_info_internal (file);

            hash_table_list_insert (changed_lists, directory, g_steal_pointer (&file));
        }
        else
        {
            g_autoptr (GFile) parent = g_file_get_parent (location);
            AmmoniteDirectory *dir = lookup_existing (parent);

            if (dir != NULL && dir->details->new_files_in_progress != NULL &&
                files != dir->details->files_changed_while_adding)
            {
                dir->details->files_changed_while_adding =
                    g_list_prepend (dir->details->files_changed_while_adding,
                                    g_object_ref (location));
            }
        }
    }

    /* Now send out the changed signals. */
    g_hash_table_foreach (changed_lists, (GHFunc) add_to_directory_work_queue, NULL);
    g_hash_table_foreach (changed_lists, (GHFunc) notify_directory_changes, NULL);
}

void
ammonite_directory_mark_files_unmounted (GList *files)
{
    for (GList *l = files; l != NULL; l = l->next)
    {
        GFile *location = l->data;
        g_autoptr (AmmoniteFile) file = ammonite_file_get_existing (location);
        if (file != NULL)
        {
            ammonite_file_mark_unmounted (file);
        }
    }
}

void
ammonite_directory_notify_files_removed (GList *files)
{
    /* Make a list of changed files in each directory. */
    g_autoptr (GHashTable) changed_lists =
        g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) ammonite_file_list_free);

    /* Make a list of parent directories that will need their counts updated. */
    g_autoptr (GHashTable) parent_directories =
        g_hash_table_new_full (NULL, NULL, (GDestroyNotify) ammonite_directory_unref, NULL);

    /* Go through all the notifications. */
    for (GList *p = files; p != NULL; p = p->next)
    {
        GFile *location = p->data;

        /* Update file count for parent directory if anyone might care. */
        g_autoptr (GFile) parent = g_file_get_parent (location);
        AmmoniteDirectory *parent_directory = lookup_existing (parent);

        hash_table_add_uncontained (parent_directories, parent_directory);

        /* Find the file. */
        g_autoptr (AmmoniteFile) file = ammonite_file_get_existing (location);
        if (file != NULL && !ammonite_file_rename_in_progress (file))
        {
            AmmoniteDirectory *directory = ammonite_file_get_directory (file);

            /* Mark it gone and prepare to send the changed signal. */
            ammonite_file_mark_gone (file);
            hash_table_list_insert (changed_lists, directory, g_steal_pointer (&file));
        }
    }

    /* Now send out the changed signals. */
    g_hash_table_foreach (changed_lists, (GHFunc) add_to_directory_work_queue, NULL);
    g_hash_table_foreach (changed_lists, (GHFunc) notify_directory_changes, NULL);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, (GHFunc) ammonite_directory_invalidate_count, NULL);
}

static void
set_directory_location (AmmoniteDirectory *directory,
                        GFile             *location)
{
    if (g_set_object (&directory->details->location, location))
    {
        g_object_notify_by_pspec (G_OBJECT (directory), properties[PROP_LOCATION]);
    }
}

static void
change_directory_location (AmmoniteDirectory *directory,
                           GFile             *new_location)
{
    /* I believe it's impossible for a self-owned file/directory
     * to be moved. But if that did somehow happen, this function
     * wouldn't do enough to handle it.
     */
    g_return_if_fail (directory->details->as_file == NULL);

    g_hash_table_remove (directories,
                         directory->details->location);

    set_directory_location (directory, new_location);

    g_hash_table_insert (directories,
                         directory->details->location,
                         directory);
}

typedef struct
{
    AmmoniteDirectory *directory;
    GFile *location;
} FileWithLocation;

static AmmoniteFileList *
ammonite_directory_moved_internal (GFile *old_location,
                                   GFile *new_location)
{
    GList *moved = NULL;

    /* The GHashTableIter gets invalidated on change_directory_location
     * calls, so gather all moved directories into a list first.
     */
    GHashTableIter directories_iter;
    g_hash_table_iter_init (&directories_iter, directories);
    gpointer value;

    while (g_hash_table_iter_next (&directories_iter, NULL, &value))
    {
        AmmoniteDirectory *directory = value;
        GFile *dir_location = directory->details->location;

        gboolean is_equal = g_file_equal (dir_location, old_location);
        if (!is_equal && !g_file_has_prefix (dir_location, old_location))
        {
            /* Directory is not affected by move */
            continue;
        }

        GFile *new_directory_location;

        if (is_equal)
        {
            new_directory_location = g_object_ref (new_location);
        }
        else
        {
            g_autofree char *relative_path =
                g_file_get_relative_path (old_location, dir_location);

            if (G_UNLIKELY (relative_path == NULL))
            {
                /* With the previous prefix check this shouldn't ever happen */
                g_autofree char *uri_old = g_file_get_uri (old_location);
                g_autofree char *uri_check = g_file_get_uri (dir_location);
                g_warning_once ("Failed to retrieve relative path between %s and %s",
                                uri_old,
                                uri_check);
                continue;
            }

            new_directory_location = g_file_resolve_relative_path (new_location, relative_path);
        }

        FileWithLocation *file_with_location = g_new0 (FileWithLocation, 1);
        file_with_location->directory = ammonite_directory_ref (directory);
        file_with_location->location = new_directory_location;
        moved = g_list_prepend (moved, file_with_location);
    }

    AmmoniteFileList *affected_files = NULL;

    for (GList *node = moved; node != NULL; node = g_list_delete_link (node, node))
    {
        g_autofree FileWithLocation *file_with_location = node->data;
        g_autoptr (AmmoniteDirectory) directory = file_with_location->directory;
        g_autoptr (GFile) new_directory_location = file_with_location->location;

        change_directory_location (directory, new_directory_location);

        /* Collect affected files. */
        if (directory->details->as_file != NULL)
        {
            affected_files = g_list_prepend (affected_files,
                                             ammonite_file_ref (directory->details->as_file));
        }
        affected_files = g_list_concat (affected_files,
                                        ammonite_file_list_copy (directory->details->file_list));
    }

    return affected_files;
}

void
ammonite_directory_moved (const char *old_uri,
                          const char *new_uri)
{
    g_autoptr (GHashTable) moved_files =
        g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) ammonite_file_list_free);
    g_autoptr (GFile) old_location = g_file_new_for_uri (old_uri);
    g_autoptr (GFile) new_location = g_file_new_for_uri (new_uri);

    g_autolist (AmmoniteFile) list =
        ammonite_directory_moved_internal (old_location, new_location);
    for (AmmoniteFileList *node = list; node != NULL; node = node->next)
    {
        AmmoniteFile *file = node->data;
        AmmoniteDirectory *directory = ammonite_file_get_directory (file);

        hash_table_list_insert (moved_files, directory, ammonite_file_ref (file));
    }

    g_hash_table_foreach (moved_files, (GHFunc) add_to_directory_work_queue, NULL);
    g_hash_table_foreach (moved_files, (GHFunc) notify_directory_changes, NULL);
}

void
ammonite_directory_notify_files_moved (GList *file_pairs)
{
    /* Make a list of added and changed files in each directory. */
    g_autoptr (GFileList) new_files_list = NULL;
    g_autoptr (GHashTable) added_lists =
        g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) file_list_free_full);
    g_autoptr (GHashTable) changed_lists =
        g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) file_list_free_full);

    /* Make a list of parent directories that will need their counts updated. */
    g_autoptr (GHashTable) parent_directories =
        g_hash_table_new_full (NULL, NULL, (GDestroyNotify) ammonite_directory_unref, NULL);

    AmmoniteAttributes cancel_attributes = ammonite_file_get_all_attributes ();

    for (GList *p = file_pairs; p != NULL; p = p->next)
    {
        GFilePair *pair = p->data;
        GFile *from_location = pair->from;
        GFile *to_location = pair->to;
        g_autoptr (AmmoniteFile) from_file = ammonite_file_get_existing (from_location);
        g_autoptr (AmmoniteFile) to_file = ammonite_file_get_existing (to_location);

        /* Handle overwriting a file. */
        if (to_file != NULL && from_file != NULL)
        {
            AmmoniteDirectory *directory;

            directory = ammonite_file_get_directory (to_file);

            /* Mark it gone and prepare to send the changed signal. */
            ammonite_file_mark_gone (to_file);
            hash_table_list_insert (changed_lists, directory, g_steal_pointer (&to_file));
            hash_table_add_uncontained (parent_directories, directory);
        }

        /* Update any directory objects that are affected. */
        for (GList *node = ammonite_directory_moved_internal (from_location, to_location);
             node != NULL;
             node = g_list_delete_link (node, node))
        {
            AmmoniteFile *affected_file = AMMONITE_FILE (node->data);
            AmmoniteDirectory *directory = ammonite_file_get_directory (affected_file);

            /* Ownership of file moves to changed_lists */
            hash_table_list_insert (changed_lists, directory, affected_file);
        }

        /* Move an existing file. */
        if (from_file == NULL)
        {
            /* Handle this as if it was a new file. */
            new_files_list = g_list_prepend (new_files_list,
                                             to_location);
        }
        else
        {
            AmmoniteDirectory *old_directory = ammonite_file_get_directory (from_file);

            /* Handle notification in the old directory. */
            hash_table_add_uncontained (parent_directories, old_directory);

            /* Cancel loading of attributes in the old directory */
            ammonite_directory_cancel_loading_attributes
                (old_directory, from_file, cancel_attributes);

            /* Locate the new directory. */
            g_autoptr (AmmoniteDirectory) new_directory = get_parent_directory (to_location);
            hash_table_add_uncontained (parent_directories, new_directory);

            /* Update the file's name and directory. */
            g_autofree char *name = g_file_get_basename (to_location);
            ammonite_file_update_name_and_directory (from_file, name, new_directory);

            /* Update file attributes */
            ammonite_file_invalidate_attributes (from_file, AMMONITE_ATTRIBUTE_INFO);

            if (old_directory != new_directory)
            {
                hash_table_list_insert (added_lists, new_directory, g_object_ref (from_file));
            }
            hash_table_list_insert (changed_lists, old_directory, g_steal_pointer (&from_file));
        }
    }

    /* Now send out the changed and added signals for existing file objects. */
    g_hash_table_foreach (changed_lists, (GHFunc) notify_directory_changes, NULL);
    g_hash_table_foreach (added_lists, (GHFunc) ammonite_directory_emit_files_added, NULL);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, (GHFunc) ammonite_directory_invalidate_count, NULL);

    /* Separate handling for brand new file objects. */
    ammonite_directory_notify_files_added (new_files_list);
}

gboolean
ammonite_directory_contains_file (AmmoniteDirectory *directory,
                                  AmmoniteFile      *file)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), FALSE);
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    if (ammonite_file_is_gone (file))
    {
        return FALSE;
    }

    return AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->contains_file (directory, file);
}

AmmoniteFile *
ammonite_directory_get_file_by_name (AmmoniteDirectory *directory,
                                     const gchar       *name)
{
    GList *files;
    GList *l;
    AmmoniteFile *result = NULL;

    files = ammonite_directory_get_file_list (directory);

    for (l = files; l != NULL; l = l->next)
    {
        if (ammonite_file_compare_display_name (l->data, name) == 0)
        {
            result = ammonite_file_ref (l->data);
            break;
        }
    }

    ammonite_file_list_free (files);

    return result;
}

void
ammonite_directory_call_when_ready (AmmoniteDirectory         *directory,
                                    AmmoniteAttributes         attributes,
                                    AmmoniteDirectoryCallback  callback,
                                    gpointer                   callback_data)
{
    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));
    g_return_if_fail (callback != NULL);

    AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->call_when_ready
        (directory, attributes,
        callback, callback_data);
}

void
ammonite_directory_cancel_callback (AmmoniteDirectory         *directory,
                                    AmmoniteDirectoryCallback  callback,
                                    gpointer                   callback_data)
{
    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));
    g_return_if_fail (callback != NULL);

    AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->cancel_callback
        (directory, callback, callback_data);
}

void
ammonite_directory_file_monitor_add (AmmoniteDirectory         *directory,
                                     gconstpointer              client,
                                     gboolean                   monitor_hidden_files,
                                     AmmoniteAttributes         attributes,
                                     AmmoniteDirectoryCallback  callback,
                                     gpointer                   callback_data)
{
    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));
    g_return_if_fail (client != NULL);

    AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->file_monitor_add
        (directory, client,
        monitor_hidden_files,
        attributes,
        callback, callback_data);
}

void
ammonite_directory_file_monitor_remove (AmmoniteDirectory *directory,
                                        gconstpointer      client)
{
    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));
    g_return_if_fail (client != NULL);

    AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->file_monitor_remove
        (directory, client);
}

void
ammonite_directory_force_reload (AmmoniteDirectory *directory)
{
    g_return_if_fail (AMMONITE_IS_DIRECTORY (directory));

    AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->force_reload (directory);
}

gboolean
ammonite_directory_is_not_empty (AmmoniteDirectory *directory)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), FALSE);

    return AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->is_not_empty (directory);
}

GList *
ammonite_directory_get_file_list (AmmoniteDirectory *directory)
{
    return AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->get_file_list (directory);
}

gboolean
ammonite_directory_is_editable (AmmoniteDirectory *directory)
{
    return AMMONITE_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->is_editable (directory);
}

GList *
ammonite_directory_match_pattern (AmmoniteDirectory *directory,
                                  const char        *pattern)
{
    GList *files, *l, *ret;
    GPatternSpec *spec;


    ret = NULL;
    spec = g_pattern_spec_new (pattern);

    files = ammonite_directory_get_file_list (directory);
    for (l = files; l; l = l->next)
    {
        AmmoniteFile *file = AMMONITE_FILE (l->data);
        const char *name = ammonite_file_get_display_name (file);

        if (g_pattern_spec_match_string (spec, name))
        {
            ret = g_list_prepend (ret, ammonite_file_ref (file));
        }
    }

    g_pattern_spec_free (spec);
    ammonite_file_list_free (files);

    return ret;
}

/**
 * ammonite_directory_list_ref
 *
 * Ref all the directories in a list.
 * @list: GList of directories.
 **/
GList *
ammonite_directory_list_ref (GList *list)
{
    g_list_foreach (list, (GFunc) ammonite_directory_ref, NULL);
    return list;
}

/**
 * ammonite_directory_list_unref
 *
 * Unref all the directories in a list.
 * @list: GList of directories.
 **/
void
ammonite_directory_list_unref (GList *list)
{
    g_list_foreach (list, (GFunc) ammonite_directory_unref, NULL);
}

/**
 * ammonite_directory_list_free
 *
 * Free a list of directories after unrefing them.
 * @list: GList of directories.
 **/
void
ammonite_directory_list_free (GList *list)
{
    ammonite_directory_list_unref (list);
    g_list_free (list);
}

/**
 * ammonite_directory_list_copy
 *
 * Copy the list of directories, making a new ref of each,
 * @list: GList of directories.
 **/
GList *
ammonite_directory_list_copy (GList *list)
{
    return g_list_copy (ammonite_directory_list_ref (list));
}

static int
compare_by_uri (AmmoniteDirectory *a,
                AmmoniteDirectory *b)
{
    char *uri_a, *uri_b;
    int res;

    uri_a = g_file_get_uri (a->details->location);
    uri_b = g_file_get_uri (b->details->location);

    res = strcmp (uri_a, uri_b);

    g_free (uri_a);
    g_free (uri_b);

    return res;
}

static int
compare_by_uri_cover (gconstpointer a,
                      gconstpointer b)
{
    return compare_by_uri (AMMONITE_DIRECTORY (a), AMMONITE_DIRECTORY (b));
}

/**
 * ammonite_directory_list_sort_by_uri
 *
 * Sort the list of directories by directory uri.
 * @list: GList of directories.
 **/
GList *
ammonite_directory_list_sort_by_uri (GList *list)
{
    return g_list_sort (list, compare_by_uri_cover);
}

/* Return the number of extant AmmoniteDirectories */
int
ammonite_directory_number_outstanding (void)
{
    return directories ? g_hash_table_size (directories) : 0;
}

void
ammonite_directory_dump (AmmoniteDirectory *directory)
{
    g_autofree gchar *uri = NULL;

    uri = g_file_get_uri (directory->details->location);
    g_print ("uri: %s\n", uri);
    g_print ("ref count: %d\n", G_OBJECT (directory)->ref_count);
}
