/*
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */
#define G_LOG_DOMAIN "ammonite-file"

#include "ammonite-file.h"

#ifndef AMMONITE_COMPILATION
#define AMMONITE_COMPILATION
#endif
#include <libammonite-extension/ammonite-extension-private.h>

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gnome-autoar/gnome-autoar.h>
#include <grp.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "ammonite-date-utilities.h"
#include "ammonite-directory-notify.h"
#include "ammonite-directory-private.h"
#include "ammonite-enums.h"
#include "ammonite-file-operations.h"
#include "ammonite-file-private.h"
#include "ammonite-file-undo-manager.h"
#include "ammonite-file-undo-operations.h"
#include "ammonite-file-utilities.h"
#include "ammonite-global-preferences.h"
#include "ammonite-icon-info.h"
#include "ammonite-metadata.h"
#include "ammonite-mime-actions.h"
#include "ammonite-module.h"
#include "ammonite-scheme.h"
#include "ammonite-signaller.h"
#include "ammonite-tag-manager.h"
#include "ammonite-thumbnails.h"
#include "ammonite-ui-utilities.h"

#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#endif

#define ICON_NAME_THUMBNAIL_LOADING   "image-loading"

/* Emblems sometimes displayed for AmmoniteFiles. */
#define AMMONITE_FILE_EMBLEM_NAME_SYMBOLIC_LINK "symbolic-link-symbolic"
#define AMMONITE_FILE_EMBLEM_NAME_CANT_READ "not-accessible-symbolic"
#define AMMONITE_FILE_EMBLEM_NAME_CANT_WRITE "readonly-symbolic"

#undef AMMONITE_FILE_DEBUG_REF
#undef AMMONITE_FILE_DEBUG_REF_VALGRIND

#ifdef AMMONITE_FILE_DEBUG_REF_VALGRIND
#include <valgrind/valgrind.h>
#define DEBUG_REF_PRINTF VALGRIND_PRINTF_BACKTRACE
#else
#define DEBUG_REF_PRINTF printf
#endif

#define MEGA_TO_BASE_RATE 1000000

/* Files that start with these characters sort after files that don't. */
#define SORT_LAST_CHAR1 '.'
#define SORT_LAST_CHAR2 '#'

#define METADATA_ID_IS_LIST_MASK (1U << 31)

typedef enum
{
    SHOW_HIDDEN = 1 << 0,
} FilterOptions;

typedef enum
{
    AMMONITE_DATE_FORMAT_REGULAR = 0,
    AMMONITE_DATE_FORMAT_FULL = 1,
} AmmoniteDateFormat;

typedef void (*ModifyListFunction) (GList       **list,
                                    AmmoniteFile *file);

enum
{
    CHANGED,
    UPDATED_DEEP_COUNT_IN_PROGRESS,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GHashTable *symbolic_links;

static guint64 cached_thumbnail_limit;
static AmmoniteSpeedTradeoffValue show_file_thumbs;

static AmmoniteSpeedTradeoffValue show_directory_item_count;

static GQuark attribute_name_q,
              attribute_size_q,
              attribute_type_q,
              attribute_detailed_type_q,
              attribute_modification_date_q,
              attribute_date_modified_q,
              attribute_date_modified_full_q,
              attribute_accessed_date_q,
              attribute_date_accessed_q,
              attribute_date_accessed_full_q,
              attribute_date_created_q,
              attribute_date_created_full_q,
              attribute_mime_type_q,
              attribute_size_detail_q,
              attribute_deep_size_q,
              attribute_deep_file_count_q,
              attribute_deep_directory_count_q,
              attribute_deep_total_count_q,
              attribute_search_relevance_q,
              attribute_trashed_on_q,
              attribute_trashed_on_full_q,
              attribute_trash_orig_path_q,
              attribute_recency_q,
              attribute_permissions_q,
              attribute_selinux_context_q,
              attribute_octal_permissions_q,
              attribute_owner_q,
              attribute_group_q,
              attribute_uri_q,
              attribute_where_q,
              attribute_link_target_q,
              attribute_volume_q,
              attribute_free_space_q,
              attribute_starred_q;

static void     ammonite_file_info_iface_init (AmmoniteFileInfoInterface *iface);
static char *ammonite_file_get_owner_as_string (AmmoniteFile *file,
                                                gboolean      include_real_name);
static char *ammonite_file_get_type_as_string (AmmoniteFile *file);
static const char *ammonite_file_get_type_as_string_no_extra_text (AmmoniteFile *file);
static char *ammonite_file_get_detailed_type_as_string (AmmoniteFile *file);
static gboolean update_info_and_name (AmmoniteFile *file,
                                      GFileInfo    *info);
static const char *ammonite_file_peek_display_name (AmmoniteFile *file);
static const char *ammonite_file_peek_display_name_collation_key (AmmoniteFile *file);
static void file_mount_unmounted (GMount  *mount,
                                  gpointer data);
static void metadata_hash_free (GHashTable *hash);

G_DEFINE_TYPE_WITH_CODE (AmmoniteFile, ammonite_file, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (AMMONITE_TYPE_FILE_INFO,
                                                ammonite_file_info_iface_init)
                         G_ADD_PRIVATE (AmmoniteFile));

enum
{
    PROP_0,
    PROP_DIRECTORY,
    PROP_DISPLAY_NAME,
    PROP_A11Y_NAME,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
ammonite_file_init (AmmoniteFile *file)
{
    file->details = ammonite_file_get_instance_private (file);

    ammonite_file_clear_info (file);
    ammonite_file_invalidate_extension_info_internal (file);

    file->details->free_space = -1;
}

static GObject *
ammonite_file_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_params)
{
    GObject *object;
    AmmoniteFile *file;

    object = (*G_OBJECT_CLASS (ammonite_file_parent_class)->constructor)(type,
                                                                         n_construct_properties,
                                                                         construct_params);

    file = AMMONITE_FILE (object);

    /* Set to default type after full construction */
    if (AMMONITE_FILE_GET_CLASS (file)->default_file_type != G_FILE_TYPE_UNKNOWN)
    {
        file->details->type = AMMONITE_FILE_GET_CLASS (file)->default_file_type;
    }

    return object;
}

gboolean
ammonite_file_set_display_name (AmmoniteFile *file,
                                const char   *display_name,
                                const char   *edit_name,
                                gboolean      custom)
{
    gboolean changed;

    if (custom && display_name == NULL)
    {
        /* We're re-setting a custom display name, invalidate it if
         *  we already set it so that the old one is re-read */
        if (file->details->got_custom_display_name)
        {
            file->details->got_custom_display_name = FALSE;
            ammonite_file_invalidate_attributes (file,
                                                 AMMONITE_ATTRIBUTE_INFO);
        }
        return FALSE;
    }

    if (display_name == NULL || *display_name == 0)
    {
        return FALSE;
    }

    if (!custom && file->details->got_custom_display_name)
    {
        return FALSE;
    }

    if (edit_name == NULL)
    {
        edit_name = display_name;
    }

    changed = FALSE;

    if (g_strcmp0 (file->details->display_name, display_name) != 0)
    {
        changed = TRUE;

        g_clear_pointer (&file->details->display_name, g_ref_string_release);

        if (g_strcmp0 (file->details->name, display_name) == 0)
        {
            file->details->display_name = g_ref_string_acquire (file->details->name);
        }
        else
        {
            file->details->display_name = g_ref_string_new (display_name);
        }

        g_free (file->details->display_name_collation_key);
        file->details->display_name_collation_key = g_utf8_collate_key_for_filename (display_name, -1);

        g_object_notify_by_pspec (G_OBJECT (file), properties[PROP_DISPLAY_NAME]);
        g_object_notify_by_pspec (G_OBJECT (file), properties[PROP_A11Y_NAME]);
    }

    if (g_strcmp0 (file->details->edit_name, edit_name) != 0)
    {
        changed = TRUE;

        g_clear_pointer (&file->details->edit_name, g_ref_string_release);
        if (g_strcmp0 (file->details->display_name, edit_name) == 0)
        {
            file->details->edit_name = g_ref_string_acquire (file->details->display_name);
        }
        else
        {
            file->details->edit_name = g_ref_string_new (edit_name);
        }
    }

    file->details->got_custom_display_name = custom;
    return changed;
}

static void
ammonite_file_clear_display_name (AmmoniteFile *file)
{
    g_clear_pointer (&file->details->display_name, g_ref_string_release);
    g_free (file->details->display_name_collation_key);
    file->details->display_name_collation_key = NULL;
    g_clear_pointer (&file->details->edit_name, g_ref_string_release);
}

static gboolean
foreach_metadata_free (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
    guint id;

    id = GPOINTER_TO_UINT (key);

    if (id & METADATA_ID_IS_LIST_MASK)
    {
        g_strfreev ((char **) value);
    }
    else
    {
        g_free ((char *) value);
    }
    return TRUE;
}


static void
metadata_hash_free (GHashTable *hash)
{
    g_hash_table_foreach_remove (hash,
                                 foreach_metadata_free,
                                 NULL);
    g_hash_table_destroy (hash);
}

static gboolean
metadata_hash_equal (GHashTable *hash1,
                     GHashTable *hash2)
{
    GHashTableIter iter;
    gpointer key1, value1, value2;
    guint id;

    if (hash1 == NULL && hash2 == NULL)
    {
        return TRUE;
    }

    if (hash1 == NULL || hash2 == NULL)
    {
        return FALSE;
    }

    if (g_hash_table_size (hash1) !=
        g_hash_table_size (hash2))
    {
        return FALSE;
    }

    g_hash_table_iter_init (&iter, hash1);
    while (g_hash_table_iter_next (&iter, &key1, &value1))
    {
        value2 = g_hash_table_lookup (hash2, key1);
        if (value2 == NULL)
        {
            return FALSE;
        }
        id = GPOINTER_TO_UINT (key1);
        if (id & METADATA_ID_IS_LIST_MASK)
        {
            if (!g_strv_equal (value1, value2))
            {
                return FALSE;
            }
        }
        else
        {
            if (strcmp ((char *) value1, (char *) value2) != 0)
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static void
clear_metadata (AmmoniteFile *file)
{
    if (file->details->metadata)
    {
        metadata_hash_free (file->details->metadata);
        file->details->metadata = NULL;
    }
}

static GHashTable *
get_metadata_from_info (GFileInfo *info)
{
    const guint metadata_key_len = strlen ("metadata::");
    g_auto (GStrv) attrs = g_file_info_list_attributes (info, "metadata");
    GHashTable *metadata = g_hash_table_new (NULL, NULL);

    for (guint i = 0; attrs[i] != NULL; i++)
    {
        guint id = ammonite_metadata_get_id (attrs[i] + metadata_key_len);

        if (id == 0)
        {
            continue;
        }

        GFileAttributeType type;
        gpointer value;

        if (!g_file_info_get_attribute_data (info, attrs[i],
                                             &type, &value, NULL))
        {
            continue;
        }

        if (type == G_FILE_ATTRIBUTE_TYPE_STRING)
        {
            g_hash_table_insert (metadata, GUINT_TO_POINTER (id),
                                 g_strdup ((char *) value));
        }
        else if (type == G_FILE_ATTRIBUTE_TYPE_STRINGV)
        {
            id |= METADATA_ID_IS_LIST_MASK;
            g_hash_table_insert (metadata, GUINT_TO_POINTER (id),
                                 g_strdupv ((char **) value));
        }
    }

    return metadata;
}

gboolean
ammonite_file_update_metadata_from_info (AmmoniteFile *file,
                                         GFileInfo    *info)
{
    gboolean changed = FALSE;

    if (g_file_info_has_namespace (info, "metadata"))
    {
        GHashTable *metadata;

        metadata = get_metadata_from_info (info);
        if (!metadata_hash_equal (metadata,
                                  file->details->metadata))
        {
            changed = TRUE;
            clear_metadata (file);
            file->details->metadata = metadata;
        }
        else
        {
            metadata_hash_free (metadata);
        }
    }
    else if (file->details->metadata)
    {
        changed = TRUE;
        clear_metadata (file);
    }
    return changed;
}

void
ammonite_file_clear_info (AmmoniteFile *file)
{
    file->details->got_file_info = FALSE;
    if (file->details->get_info_error)
    {
        g_error_free (file->details->get_info_error);
        file->details->get_info_error = NULL;
    }
    /* Reset to default type, which might be other than unknown for
     *  special kinds of files like the desktop or a search directory */
    file->details->type = AMMONITE_FILE_GET_CLASS (file)->default_file_type;

    if (!file->details->got_custom_display_name)
    {
        ammonite_file_clear_display_name (file);
    }

    if (file->details->activation_uri != NULL)
    {
        g_free (file->details->activation_uri);
        file->details->activation_uri = NULL;
    }

    if (file->details->icon != NULL)
    {
        g_object_unref (file->details->icon);
        file->details->icon = NULL;
    }

    g_free (file->details->thumbnail_path);
    file->details->thumbnail_path = NULL;
    file->details->thumbnailing_failed = FALSE;

    file->details->is_symlink = FALSE;
    file->details->is_hidden = FALSE;
    file->details->is_mountpoint = FALSE;
    file->details->has_uid = FALSE;
    file->details->has_gid = FALSE;
    file->details->can_read = TRUE;
    file->details->can_write = TRUE;
    file->details->can_execute = TRUE;
    file->details->can_delete = TRUE;
    file->details->can_trash = TRUE;
    file->details->can_rename = TRUE;
    file->details->can_mount = FALSE;
    file->details->can_unmount = FALSE;
    file->details->can_eject = FALSE;
    file->details->can_start = FALSE;
    file->details->can_start_degraded = FALSE;
    file->details->can_stop = FALSE;
    file->details->start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;
    file->details->can_poll_for_media = FALSE;
    file->details->is_media_check_automatic = FALSE;
    file->details->has_preview_icon = FALSE;
    file->details->has_permissions = FALSE;
    file->details->permissions = 0;
    file->details->size = -1;
    file->details->sort_order = 0;
    file->details->mtime = 0;
    file->details->atime = 0;
    file->details->btime = 0;
    file->details->trash_time = 0;
    file->details->recency = 0;
    g_free (file->details->symlink_name);
    file->details->symlink_name = NULL;
    g_clear_pointer (&file->details->mime_type, g_ref_string_release);
    g_free (file->details->selinux_context);
    file->details->selinux_context = NULL;
    g_clear_pointer (&file->details->owner, g_ref_string_release);
    g_clear_pointer (&file->details->owner_real, g_ref_string_release);
    g_clear_pointer (&file->details->group, g_ref_string_release);

    g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);

    clear_metadata (file);
}

AmmoniteDirectory *
ammonite_file_get_directory (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    return file->details->directory;
}

void
ammonite_file_set_directory (AmmoniteFile      *file,
                             AmmoniteDirectory *directory)
{
    if (!g_set_object (&file->details->directory, directory))
    {
        return;
    }

    g_clear_pointer (&file->details->directory_name_collation_key, g_free);

    g_object_notify_by_pspec (G_OBJECT (file), properties[PROP_DIRECTORY]);
}

static inline const gchar *
ammonite_file_get_directory_name (AmmoniteFile *file)
{
    if (G_UNLIKELY (file->details->directory_name_collation_key == NULL))
    {
        g_autofree char *parent_uri = ammonite_file_get_parent_uri (file);

        file->details->directory_name_collation_key = g_utf8_collate_key_for_filename (parent_uri, -1);
    }

    return file->details->directory_name_collation_key;
}

static AmmoniteFile *
ammonite_file_new_self_owned (AmmoniteDirectory *directory)
{
    g_autofree char *filename = ammonite_directory_get_name_for_self_as_new_file (directory);
    AmmoniteFile *file = ammonite_directory_new_as_file (directory);

    file->details->name = g_ref_string_new (filename);
    g_warn_if_fail (directory->details->as_file == NULL);
    directory->details->as_file = file;

#ifdef AMMONITE_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p ref'd", file);
#endif

    return file;
}

static AmmoniteFile *
ammonite_file_new_from_filename (AmmoniteDirectory *directory,
                                 const char        *filename)
{
    g_assert (filename != NULL);
    g_assert (filename[0] != '\0');

    /* Any not self-owned file must be a AmmoniteVfsFile */
    AmmoniteFile *file = ammonite_directory_new_as_vfs_file (directory);
    file->details->name = g_ref_string_new (filename);

#ifdef AMMONITE_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p ref'd", file);
#endif

    ammonite_directory_add_file (directory, file);

    return file;
}

static void
modify_link_hash_table (AmmoniteFile       *file,
                        ModifyListFunction  modify_function)
{
    char *target_uri;
    gboolean found;
    gpointer original_key;
    GList **list_ptr;

    /* Check if there is a symlink name. If none, we are OK. */
    if (file->details->symlink_name == NULL || !ammonite_file_is_symbolic_link (file))
    {
        return;
    }

    /* Create the hash table first time through. */
    if (symbolic_links == NULL)
    {
        symbolic_links = g_hash_table_new (g_str_hash, g_str_equal);
    }

    target_uri = ammonite_file_get_symbolic_link_target_uri (file);

    /* Find the old contents of the hash table. */
    found = g_hash_table_lookup_extended
                (symbolic_links, target_uri,
                &original_key, (gpointer *) &list_ptr);
    if (!found)
    {
        list_ptr = g_new0 (GList *, 1);
        original_key = g_strdup (target_uri);
        g_hash_table_insert (symbolic_links, original_key, list_ptr);
    }
    (*modify_function)(list_ptr, file);
    if (*list_ptr == NULL)
    {
        g_hash_table_remove (symbolic_links, target_uri);
        g_free (list_ptr);
        g_free (original_key);
    }
    g_free (target_uri);
}

static void
symbolic_link_weak_notify (gpointer  data,
                           GObject  *where_the_object_was)
{
    GList **list = data;
    /* This really shouldn't happen, but we're seeing some strange things in
     *  bug #358172 where the symlink hashtable isn't correctly updated. */
    *list = g_list_remove (*list, where_the_object_was);
}

static void
add_to_link_hash_table_list (GList        **list,
                             AmmoniteFile  *file)
{
    if (g_list_find (*list, file) != NULL)
    {
        g_warning ("Adding file to symlink_table multiple times. "
                   "Please add feedback of what you were doing at http://bugzilla.gnome.org/show_bug.cgi?id=358172\n");
        return;
    }
    g_object_weak_ref (G_OBJECT (file), symbolic_link_weak_notify, list);
    *list = g_list_prepend (*list, file);
}

static void
add_to_link_hash_table (AmmoniteFile *file)
{
    modify_link_hash_table (file, add_to_link_hash_table_list);
}

static void
remove_from_link_hash_table_list (GList        **list,
                                  AmmoniteFile  *file)
{
    if (g_list_find (*list, file) != NULL)
    {
        g_object_weak_unref (G_OBJECT (file), symbolic_link_weak_notify, list);
        *list = g_list_remove (*list, file);
    }
}

static void
remove_from_link_hash_table (AmmoniteFile *file)
{
    modify_link_hash_table (file, remove_from_link_hash_table_list);
}

AmmoniteFile *
ammonite_file_new_from_info (AmmoniteDirectory *directory,
                             GFileInfo         *info)
{
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (directory), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    AmmoniteFile *file = ammonite_directory_new_as_vfs_file (directory);

    update_info_and_name (file, info);

#ifdef AMMONITE_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p ref'd", file);
#endif

    return file;
}

static AmmoniteFile *
ammonite_file_get_internal (GFile    *location,
                            gboolean  create)
{
    g_autoptr (GFile) parent = g_file_get_parent (location);
    gboolean self_owned = (parent == NULL);
    GFile *dir_location = self_owned ? location : parent;

    /* Get object that represents the directory. */
    g_autoptr (AmmoniteDirectory) directory = (create)
                                              ? ammonite_directory_get (dir_location)
                                              : ammonite_directory_get_existing (dir_location);

    if (self_owned)
    {
        if (directory != NULL && directory->details->as_file != NULL)
        {
            return ammonite_file_ref (directory->details->as_file);
        }
        else if (create)
        {
            return ammonite_file_new_self_owned (directory);
        }
    }
    else
    {
        g_autofree char *basename = g_file_get_basename (location);

        if (directory != NULL)
        {
            /* Check if file is already known */
            AmmoniteFile *file = ammonite_directory_find_file_by_name (directory, basename);

            if (file != NULL)
            {
                return ammonite_file_ref (file);
            }
        }

        if (create)
        {
            return ammonite_file_new_from_filename (directory, basename);
        }
    }

    return NULL;
}

AmmoniteFile *
ammonite_file_get (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return ammonite_file_get_internal (location, TRUE);
}

AmmoniteFile *
ammonite_file_get_existing (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return ammonite_file_get_internal (location, FALSE);
}

AmmoniteFile *
ammonite_file_get_existing_by_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    location = g_file_new_for_uri (uri);

    return ammonite_file_get_existing (location);
}

AmmoniteFile *
ammonite_file_get_by_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    location = g_file_new_for_uri (uri);

    return ammonite_file_get (location);
}

gboolean
ammonite_file_is_self_owned (AmmoniteFile *file)
{
    return file->details->directory->details->as_file == file;
}

static void
ammonite_file_dispose (GObject *object)
{
    G_OBJECT_CLASS (ammonite_file_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    AmmoniteDirectory *directory;
    AmmoniteFile *file;

    file = AMMONITE_FILE (object);

    g_assert (file->details->operations_in_progress == NULL);

    ammonite_async_destroying_file (file);

    remove_from_link_hash_table (file);

    directory = file->details->directory;

    if (ammonite_file_is_self_owned (file))
    {
        directory->details->as_file = NULL;
    }
    else
    {
        if (!file->details->is_gone)
        {
            ammonite_directory_remove_file (directory, file);
        }
    }

    if (file->details->get_info_error)
    {
        g_error_free (file->details->get_info_error);
    }

    ammonite_directory_unref (directory);
    g_clear_pointer (&file->details->name, g_ref_string_release);
    g_clear_pointer (&file->details->display_name, g_ref_string_release);
    g_free (file->details->display_name_collation_key);
    g_free (file->details->directory_name_collation_key);
    g_clear_pointer (&file->details->edit_name, g_ref_string_release);
    if (file->details->icon)
    {
        g_object_unref (file->details->icon);
    }
    g_free (file->details->thumbnail_path);
    g_free (file->details->symlink_name);
    g_clear_pointer (&file->details->mime_type, g_ref_string_release);
    g_clear_pointer (&file->details->owner, g_ref_string_release);
    g_clear_pointer (&file->details->owner_real, g_ref_string_release);
    g_clear_pointer (&file->details->group, g_ref_string_release);
    g_free (file->details->selinux_context);
    g_free (file->details->activation_uri);

    if (file->details->thumbnail)
    {
        g_object_unref (file->details->thumbnail);
    }

    g_clear_object (&file->details->mount);

    g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);
    g_free (file->details->trash_orig_path);

    g_list_free_full (file->details->extension_emblems, g_free);
    g_list_free_full (file->details->pending_info_providers, g_object_unref);

    if (file->details->extension_attributes)
    {
        g_hash_table_destroy (file->details->extension_attributes);
    }

    if (file->details->metadata)
    {
        metadata_hash_free (file->details->metadata);
    }

    g_free (file->details->fts_snippet);

    G_OBJECT_CLASS (ammonite_file_parent_class)->finalize (object);
}

AmmoniteFile *
ammonite_file_ref (AmmoniteFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

#ifdef AMMONITE_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p ref'd", file);
#endif

    return g_object_ref (file);
}

void
ammonite_file_unref (AmmoniteFile *file)
{
    if (file == NULL)
    {
        return;
    }

    g_return_if_fail (AMMONITE_IS_FILE (file));

#ifdef AMMONITE_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p unref'd", file);
#endif

    g_object_unref (file);
}

/**
 * ammonite_file_get_parent_uri_for_display:
 *
 * Get the uri for the parent directory.
 *
 * @file: The file in question.
 *
 * Return value: A string representing the parent's location,
 * formatted for user display (including stripping "file://"
 * and adding trailing slash).
 * If the parent is NULL, returns the empty string.
 */
static char *
ammonite_file_get_parent_uri_for_display (AmmoniteFile *file)
{
    g_autoptr (GFile) parent = NULL;
    char *result;

    g_assert (AMMONITE_IS_FILE (file));

    parent = ammonite_file_get_parent_location (file);
    if (parent)
    {
        g_autofree gchar *parse_name = g_file_get_parse_name (parent);

        /* URI decode the string if it is a network connection*/
        if (g_uri_is_valid (parse_name, G_URI_FLAGS_NONE, NULL))
        {
            g_autofree gchar *temp = parse_name;
            parse_name = g_uri_unescape_string (temp, NULL);
        }

        /* Ensure a trailing slash to emphasize it is a directory */
        if (g_str_has_suffix (parse_name, G_DIR_SEPARATOR_S))
        {
            result = g_steal_pointer (&parse_name);
        }
        else
        {
            result = g_strconcat (parse_name, G_DIR_SEPARATOR_S, NULL);
        }
    }
    else
    {
        result = g_strdup ("");
    }

    return result;
}

/**
 * ammonite_file_can_read:
 *
 * Check whether the user is allowed to read the contents of this file.
 *
 * @file: The file to check.
 *
 * Return value: FALSE if the user is definitely not allowed to read
 * the contents of the file. If the user has read permission, or
 * the code can't tell whether the user has read permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
ammonite_file_can_read (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_read;
}

/**
 * ammonite_file_can_execute:
 *
 * Check whether the user is allowed to execute this file.
 *
 * @file: The file to check.
 *
 * Return value: FALSE if the user is definitely not allowed to execute
 * the file. If the user has execute permission, or
 * the code can't tell whether the user has execute permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
ammonite_file_can_execute (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_execute;
}

gboolean
ammonite_file_can_mount (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_mount;
}

gboolean
ammonite_file_can_unmount (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_unmount ||
           (file->details->mount != NULL &&
            g_mount_can_unmount (file->details->mount));
}

gboolean
ammonite_file_can_eject (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_eject ||
           (file->details->mount != NULL &&
            g_mount_can_eject (file->details->mount));
}

typedef gboolean (*DriveCheckFunc) (GDrive *);

static gboolean
can_mount_do (AmmoniteFile   *file,
              DriveCheckFunc  drive_check_func)
{
    if (file->details->mount != NULL)
    {
        g_autoptr (GDrive) drive = g_mount_get_drive (file->details->mount);

        if (drive != NULL)
        {
            return drive_check_func (drive);
        }
    }

    return FALSE;
}

gboolean
ammonite_file_can_start (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_start ||
           can_mount_do (file, g_drive_can_start);
}

gboolean
ammonite_file_can_start_degraded (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_start_degraded ||
           can_mount_do (file, g_drive_can_start_degraded);
}

gboolean
ammonite_file_can_poll_for_media (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_poll_for_media ||
           can_mount_do (file, g_drive_can_poll_for_media);
}

gboolean
ammonite_file_is_media_check_automatic (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->is_media_check_automatic ||
           can_mount_do (file, g_drive_is_media_check_automatic);
}


gboolean
ammonite_file_can_stop (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_stop ||
           can_mount_do (file, g_drive_can_stop);
}

GDriveStartStopType
ammonite_file_get_start_stop_type (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), G_DRIVE_START_STOP_TYPE_UNKNOWN);

    if (file->details->start_stop_type != G_DRIVE_START_STOP_TYPE_UNKNOWN)
    {
        return file->details->start_stop_type;
    }

    if (file->details->mount != NULL)
    {
        g_autoptr (GDrive) drive = g_mount_get_drive (file->details->mount);

        if (drive != NULL)
        {
            return g_drive_get_start_stop_type (drive);
        }
    }

    return G_DRIVE_START_STOP_TYPE_UNKNOWN;
}

void
ammonite_file_mount (AmmoniteFile                  *file,
                     GMountOperation               *mount_op,
                     GCancellable                  *cancellable,
                     AmmoniteFileOperationCallback  callback,
                     gpointer                       callback_data)
{
    GError *error;

    if (AMMONITE_FILE_GET_CLASS (file)->mount == NULL)
    {
        if (callback)
        {
            error = NULL;
            g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                 _("This file cannot be mounted"));
            callback (file, NULL, error, callback_data);
            g_error_free (error);
        }
    }
    else
    {
        AMMONITE_FILE_GET_CLASS (file)->mount (file, mount_op, cancellable, callback, callback_data);
    }
}

typedef struct
{
    AmmoniteFile *file;
    AmmoniteFileOperationCallback callback;
    gpointer callback_data;
} UnmountData;

static void
unmount_done (void *callback_data)
{
    UnmountData *data;

    data = (UnmountData *) callback_data;
    if (data->callback)
    {
        data->callback (data->file, NULL, NULL, data->callback_data);
    }
    ammonite_file_unref (data->file);
    g_free (data);
}

void
ammonite_file_unmount (AmmoniteFile                  *file,
                       GMountOperation               *mount_op,
                       GCancellable                  *cancellable,
                       AmmoniteFileOperationCallback  callback,
                       gpointer                       callback_data)
{
    GError *error;
    UnmountData *data;

    if (file->details->can_unmount)
    {
        if (AMMONITE_FILE_GET_CLASS (file)->unmount != NULL)
        {
            AMMONITE_FILE_GET_CLASS (file)->unmount (file, mount_op, cancellable, callback, callback_data);
        }
        else
        {
            if (callback)
            {
                error = NULL;
                g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("This file cannot be unmounted"));
                callback (file, NULL, error, callback_data);
                g_error_free (error);
            }
        }
    }
    else if (file->details->mount != NULL &&
             g_mount_can_unmount (file->details->mount))
    {
        GtkWindow *parent;

        parent = gtk_mount_operation_get_parent (GTK_MOUNT_OPERATION (mount_op));

        data = g_new0 (UnmountData, 1);
        data->file = ammonite_file_ref (file);
        data->callback = callback;
        data->callback_data = callback_data;
        ammonite_file_operations_unmount_mount_full (parent, file->details->mount, mount_op, FALSE, unmount_done, data);
    }
    else if (callback)
    {
        callback (file, NULL, NULL, callback_data);
    }
}

void
ammonite_file_eject (AmmoniteFile                  *file,
                     GMountOperation               *mount_op,
                     GCancellable                  *cancellable,
                     AmmoniteFileOperationCallback  callback,
                     gpointer                       callback_data)
{
    GError *error;
    UnmountData *data;

    if (file->details->can_eject)
    {
        if (AMMONITE_FILE_GET_CLASS (file)->eject != NULL)
        {
            AMMONITE_FILE_GET_CLASS (file)->eject (file, mount_op, cancellable, callback, callback_data);
        }
        else
        {
            if (callback)
            {
                error = NULL;
                g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("This file cannot be ejected"));
                callback (file, NULL, error, callback_data);
                g_error_free (error);
            }
        }
    }
    else if (file->details->mount != NULL &&
             g_mount_can_eject (file->details->mount))
    {
        GtkWindow *parent;

        parent = gtk_mount_operation_get_parent (GTK_MOUNT_OPERATION (mount_op));

        data = g_new0 (UnmountData, 1);
        data->file = ammonite_file_ref (file);
        data->callback = callback;
        data->callback_data = callback_data;
        ammonite_file_operations_unmount_mount_full (parent, file->details->mount, mount_op, TRUE, unmount_done, data);
    }
    else if (callback)
    {
        callback (file, NULL, NULL, callback_data);
    }
}

void
ammonite_file_start (AmmoniteFile                  *file,
                     GMountOperation               *start_op,
                     GCancellable                  *cancellable,
                     AmmoniteFileOperationCallback  callback,
                     gpointer                       callback_data)
{
    GError *error;

    if ((file->details->can_start || file->details->can_start_degraded) &&
        AMMONITE_FILE_GET_CLASS (file)->start != NULL)
    {
        AMMONITE_FILE_GET_CLASS (file)->start (file, start_op, cancellable, callback, callback_data);
    }
    else
    {
        if (callback)
        {
            error = NULL;
            g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                 _("This file cannot be started"));
            callback (file, NULL, error, callback_data);
            g_error_free (error);
        }
    }
}

static void
file_stop_callback (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    gboolean stopped;
    GError *error;

    op = callback_data;

    error = NULL;
    stopped = g_drive_stop_finish (G_DRIVE (source_object),
                                   res, &error);

    if (!stopped &&
        error->domain == G_IO_ERROR &&
        (error->code == G_IO_ERROR_FAILED_HANDLED ||
         error->code == G_IO_ERROR_CANCELLED))
    {
        g_error_free (error);
        error = NULL;
    }

    ammonite_file_operation_complete (op, NULL, error);
    if (error)
    {
        g_error_free (error);
    }
}

void
ammonite_file_stop (AmmoniteFile                  *file,
                    GMountOperation               *mount_op,
                    GCancellable                  *cancellable,
                    AmmoniteFileOperationCallback  callback,
                    gpointer                       callback_data)
{
    GError *error;

    if (AMMONITE_FILE_GET_CLASS (file)->stop != NULL)
    {
        if (file->details->can_stop)
        {
            AMMONITE_FILE_GET_CLASS (file)->stop (file, mount_op, cancellable, callback, callback_data);
        }
        else
        {
            if (callback)
            {
                error = NULL;
                g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("This file cannot be stopped"));
                callback (file, NULL, error, callback_data);
                g_error_free (error);
            }
        }
    }
    else
    {
        g_autoptr (GDrive) drive = NULL;

        if (file->details->mount != NULL)
        {
            drive = g_mount_get_drive (file->details->mount);
        }

        if (drive != NULL && g_drive_can_stop (drive))
        {
            AmmoniteFileOperation *op;

            op = ammonite_file_operation_new (file, callback, callback_data);
            if (cancellable)
            {
                g_set_object (&op->cancellable, cancellable);
            }

            g_drive_stop (drive,
                          G_MOUNT_UNMOUNT_NONE,
                          mount_op,
                          op->cancellable,
                          file_stop_callback,
                          op);
        }
        else
        {
            if (callback)
            {
                error = NULL;
                g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("This file cannot be stopped"));
                callback (file, NULL, error, callback_data);
                g_error_free (error);
            }
        }
    }
}

void
ammonite_file_poll_for_media (AmmoniteFile *file)
{
    if (file->details->can_poll_for_media)
    {
        if (AMMONITE_FILE_GET_CLASS (file)->stop != NULL)
        {
            AMMONITE_FILE_GET_CLASS (file)->poll_for_media (file);
        }
    }
    else if (file->details->mount != NULL)
    {
        g_autoptr (GDrive) drive = g_mount_get_drive (file->details->mount);

        if (drive != NULL)
        {
            g_drive_poll_for_media (drive,
                                    NULL,              /* cancellable */
                                    NULL,              /* GAsyncReadyCallback */
                                    NULL);             /* user_data */
        }
    }
}

/**
 * ammonite_file_can_rename:
 *
 * Check whether the user is allowed to change the name of the file.
 *
 * @file: The file to check.
 *
 * Return value: FALSE if the user is definitely not allowed to change
 * the name of the file. If the user is allowed to change the name, or
 * the code can't tell whether the user is allowed to change the name,
 * returns TRUE (so rename failures must always be handled).
 */
gboolean
ammonite_file_can_rename (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    /* Nonexistent files can't be renamed. */
    if (ammonite_file_is_gone (file))
    {
        return FALSE;
    }

    /* Self-owned files can't be renamed */
    if (ammonite_file_is_self_owned (file))
    {
        return FALSE;
    }

    if (ammonite_file_is_home (file))
    {
        return FALSE;
    }

    return file->details->can_rename;
}

gboolean
ammonite_file_can_delete (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    /* Nonexistent files can't be deleted. */
    if (ammonite_file_is_gone (file))
    {
        return FALSE;
    }

    /* Self-owned files can't be deleted */
    if (ammonite_file_is_self_owned (file))
    {
        return FALSE;
    }

    return file->details->can_delete;
}

gboolean
ammonite_file_can_trash (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    /* Nonexistent files can't be deleted. */
    if (ammonite_file_is_gone (file))
    {
        return FALSE;
    }

    /* Self-owned files can't be deleted */
    if (ammonite_file_is_self_owned (file))
    {
        return FALSE;
    }

    return file->details->can_trash;
}

gboolean
ammonite_file_opens_in_view (AmmoniteFile *file)
{
    return (ammonite_file_is_directory (file) ||
            ammonite_file_get_file_type (file) == G_FILE_TYPE_MOUNTABLE ||
            (ammonite_file_get_file_type (file) == G_FILE_TYPE_SHORTCUT &&
             g_strcmp0 (ammonite_file_get_mime_type (file), "inode/directory") == 0));
}

AmmoniteFileOperation *
ammonite_file_operation_new (AmmoniteFile                  *file,
                             AmmoniteFileOperationCallback  callback,
                             gpointer                       callback_data)
{
    AmmoniteFileOperation *op;

    op = g_new0 (AmmoniteFileOperation, 1);
    op->file = ammonite_file_ref (file);
    op->callback = callback;
    op->callback_data = callback_data;
    op->cancellable = g_cancellable_new ();

    op->file->details->operations_in_progress = g_list_prepend
                                                    (op->file->details->operations_in_progress, op);

    return op;
}

static void
ammonite_file_operation_remove (AmmoniteFileOperation *op)
{
    GList *l;
    AmmoniteFile *file;

    op->file->details->operations_in_progress = g_list_remove
                                                    (op->file->details->operations_in_progress, op);


    for (l = op->files; l != NULL; l = l->next)
    {
        file = AMMONITE_FILE (l->data);
        file->details->operations_in_progress = g_list_remove
                                                    (file->details->operations_in_progress, op);
    }
}

void
ammonite_file_operation_free (AmmoniteFileOperation *op)
{
    ammonite_file_operation_remove (op);

    g_clear_pointer (&op->file, ammonite_file_unref);
    g_clear_list (&op->files, (GDestroyNotify) ammonite_file_unref);

    g_object_unref (op->cancellable);
    if (op->free_data)
    {
        op->free_data (op->data);
    }

    if (op->undo_info != NULL)
    {
        ammonite_file_undo_manager_set_action (op->undo_info);
        g_object_unref (op->undo_info);
    }

    g_free (op);
}

void
ammonite_file_operation_complete (AmmoniteFileOperation *op,
                                  GFile                 *result_file,
                                  GError                *error)
{
    /* Claim that something changed even if the operation failed.
     * This makes it easier for some clients who see the "reverting"
     * as "changing back".
     */
    ammonite_file_operation_remove (op);

    if (op->files == NULL)
    {
        ammonite_file_changed (op->file);
    }

    if (op->callback)
    {
        (*op->callback)(op->file, result_file, error, op->callback_data);
    }

    if (error != NULL)
    {
        g_clear_object (&op->undo_info);
    }

    ammonite_file_operation_free (op);
}

void
ammonite_file_operation_cancel (AmmoniteFileOperation *op)
{
    /* Cancel the operation if it's still in progress. */
    g_cancellable_cancel (op->cancellable);
}

static void
rename_get_info_callback (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    AmmoniteDirectory *directory;
    g_autoptr (AmmoniteFile) existing_file = NULL;
    char *old_uri;
    char *new_uri;
    const char *new_name;
    GFileInfo *new_info;
    GError *error;

    op = callback_data;

    error = NULL;
    new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
    if (new_info != NULL)
    {
        g_autoptr (GFile) old_location = NULL;
        g_autoptr (GFile) new_location = NULL;

        directory = op->file->details->directory;

        new_name = g_file_info_get_name (new_info);

        /* If there was another file by the same name in this
         * directory and it is not the same file that we are
         * renaming, mark it gone.
         */
        existing_file = ammonite_directory_find_file_by_name (directory, new_name);
        if (existing_file != NULL)
        {
            g_object_ref (existing_file);

            if (existing_file != op->file)
            {
                ammonite_file_mark_gone (existing_file);
                ammonite_file_changed (existing_file);
            }
        }

        old_location = ammonite_file_get_location (op->file);
        old_uri = g_file_get_uri (old_location);

        update_info_and_name (op->file, new_info);

        new_location = ammonite_file_get_location (op->file);
        new_uri = g_file_get_uri (new_location);

        ammonite_directory_moved (old_uri, new_uri);
        ammonite_tag_manager_update_moved_uris (ammonite_tag_manager_get (),
                                                old_location,
                                                new_location);

        g_free (new_uri);
        g_free (old_uri);

        g_object_unref (new_info);
    }
    ammonite_file_operation_complete (op, NULL, error);
    if (error)
    {
        g_error_free (error);
    }
}

static void
rename_callback (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      callback_data)
{
    AmmoniteFileOperation *op = callback_data;
    g_autoptr (GFile) new_file = NULL;
    g_autoptr (GError) error = NULL;

    new_file = g_file_set_display_name_finish (G_FILE (source_object),
                                               res, &error);

    if (new_file != NULL)
    {
        if (op->undo_info != NULL)
        {
            ammonite_file_undo_info_rename_set_data_post (AMMONITE_FILE_UNDO_INFO_RENAME (op->undo_info),
                                                          new_file);
        }
        g_file_query_info_async (new_file,
                                 AMMONITE_FILE_DEFAULT_ATTRIBUTES,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 op->cancellable,
                                 rename_get_info_callback, op);
    }
    else
    {
        ammonite_file_operation_complete (op, NULL, error);
    }
}

static gboolean
name_is (AmmoniteFile *file,
         const char   *new_name)
{
    const char *old_name;
    old_name = file->details->name;
    return strcmp (new_name, old_name) == 0;
}

static const char *
ammonite_file_can_rename_file (AmmoniteFile                  *file,
                               const char                    *new_name,
                               AmmoniteFileOperationCallback  callback,
                               gpointer                       callback_data)
{
    GError *error;

    /* Return an error for incoming names containing path separators. */
    if (strstr (new_name, G_DIR_SEPARATOR_S) != NULL)
    {
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             _("Slashes are not allowed in filenames"));
        if (callback != NULL)
        {
            (*callback)(file, NULL, error, callback_data);
        }
        g_error_free (error);
        return NULL;
    }

    /* Can't rename a file that's already gone.
     * We need to check this here because there may be a new
     * file with the same name.
     */
    if (ammonite_file_rename_handle_file_gone (file, callback, callback_data))
    {
        return NULL;
    }

    /* Test the name-hasn't-changed case explicitly, for two reasons.
     * (1) rename returns an error if new & old are same.
     * (2) We don't want to send file-changed signal if nothing changed.
     */
    if (name_is (file, new_name))
    {
        if (callback != NULL)
        {
            (*callback)(file, NULL, NULL, callback_data);
        }
        return NULL;
    }

    /* Self-owned files can't be renamed. Test the name-not-actually-changing
     * case before this case.
     */
    if (ammonite_file_is_self_owned (file))
    {
        /* Claim that something changed even if the rename
         * failed. This makes it easier for some clients who
         * see the "reverting" to the old name as "changing
         * back".
         */
        ammonite_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             _("Toplevel files cannot be renamed"));

        if (callback != NULL)
        {
            (*callback)(file, NULL, error, callback_data);
        }
        g_error_free (error);

        return NULL;
    }

    return new_name;
}

void
ammonite_file_rename (AmmoniteFile                  *file,
                      const char                    *new_name,
                      AmmoniteFileOperationCallback  callback,
                      gpointer                       callback_data)
{
    AmmoniteFileOperation *op;
    const char *new_file_name;
    GFile *location;

    g_return_if_fail (AMMONITE_IS_FILE (file));
    g_return_if_fail (new_name != NULL);
    g_return_if_fail (callback != NULL);

    new_file_name = ammonite_file_can_rename_file (file,
                                                   new_name,
                                                   callback,
                                                   callback_data);

    if (new_file_name == NULL)
    {
        return;
    }

    /* Set up a renaming operation. */
    op = ammonite_file_operation_new (file, callback, callback_data);
    op->is_rename = TRUE;
    location = ammonite_file_get_location (file);

    /* Tell the undo manager a rename is taking place */
    if (!ammonite_file_undo_manager_is_operating ())
    {
        const char *old_name;
        op->undo_info = ammonite_file_undo_info_rename_new ();

        old_name = ammonite_file_get_display_name (file);
        ammonite_file_undo_info_rename_set_data_pre (AMMONITE_FILE_UNDO_INFO_RENAME (op->undo_info),
                                                     location, old_name, new_file_name);
    }

    /* Do the renaming. */
    g_file_set_display_name_async (location,
                                   new_file_name,
                                   G_PRIORITY_DEFAULT,
                                   op->cancellable,
                                   rename_callback,
                                   op);
    g_object_unref (location);
}

gboolean
ammonite_file_rename_handle_file_gone (AmmoniteFile                  *file,
                                       AmmoniteFileOperationCallback  callback,
                                       gpointer                       callback_data)
{
    GError *error;

    if (ammonite_file_is_gone (file))
    {
        /* Claim that something changed even if the rename
         * failed. This makes it easier for some clients who
         * see the "reverting" to the old name as "changing
         * back".
         */
        ammonite_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                             _("File not found"));
        if (callback)
        {
            (*callback)(file, NULL, error, callback_data);
        }
        g_error_free (error);
        return TRUE;
    }

    return FALSE;
}

typedef struct
{
    GPtrArray *chain_files;
    gboolean is_cycle;

    AmmoniteFileOperation *op;
    GAsyncQueue *rollback_queue;
} RenameChainData;

typedef struct
{
    GList *old_files;
    GList *new_files;
} TwoLists;

static void
rename_file_chain (gpointer data,
                   gpointer user_data)
{
    gboolean skip_files = FALSE;
    g_autofree RenameChainData *rename_chain = data;
    g_autoptr (GPtrArray) chain_files = rename_chain->chain_files;
    gsize len = chain_files->len;
    g_autoptr (GFile) snapped_file = NULL;
    AmmoniteFileOperation *op = rename_chain->op;
    GList *old_files = NULL, *new_files = NULL;
    GAsyncQueue *changed_files = user_data;

    if (rename_chain->is_cycle)
    {
        /* We have a cycle, break it with a temporary name. */
        g_autofree gchar *random_string = g_uuid_string_random ();

        snapped_file = g_object_ref (g_ptr_array_index (chain_files, len - 1));
        GFile *renamed_file = g_file_set_display_name (snapped_file,
                                                       random_string,
                                                       op->cancellable,
                                                       NULL);

        if (renamed_file != NULL)
        {
            g_ptr_array_remove_index_fast (chain_files, len - 1);
            g_ptr_array_add (chain_files, renamed_file);
        }
        else
        {
            /* Unlikely to be a name conflict, skip the entire chain. */
            g_autoptr (GFile) parent = g_file_get_parent (snapped_file);
            GFile *new_file = g_file_get_child (parent, random_string);
            TwoLists *rollback_files = g_new0 (TwoLists, 1);

            rollback_files->old_files = g_list_prepend (NULL, g_object_ref (snapped_file));
            rollback_files->new_files = g_list_prepend (NULL, new_file);
            g_async_queue_push (rename_chain->rollback_queue, rollback_files);

            skip_files = TRUE;
        }
    }

    /* Process the chain leading to this file */
    for (uint i = 0; i < len - 1; i++)
    {
        GFile *src = g_ptr_array_index (chain_files, i + 1);
        GFile *target = g_ptr_array_index (chain_files, i);

        if (skip_files)
        {
            guint remaining = len - 1 - i;

            g_atomic_int_add (&op->skipped_files, remaining);
            break;
        }

        g_autofree gchar *new_name = g_file_get_basename (target);
        g_autoptr (GError) error = NULL;

        /* Do the renaming. */
        g_autoptr (GFile) renamed_file = g_file_set_display_name (src,
                                                                  new_name,
                                                                  op->cancellable,
                                                                  &error);

        if (error != NULL)
        {
            g_autofree gchar *old_name = g_file_get_basename (src);
            g_warning ("Batch rename for file \"%s\" failed: %s",
                       old_name, error->message);

            /* We need to skip the rest of the files in the chain. */
            skip_files = TRUE;
            g_atomic_int_inc (&op->skipped_files);
        }
        else
        {
            g_atomic_int_inc (&op->renamed_files);
            old_files = g_list_prepend (old_files, g_object_ref (src));
            new_files = g_list_prepend (new_files, g_steal_pointer (&renamed_file));
        }
    }

    if (snapped_file != NULL)
    {
        /* Use the original snapped file to undo the operation correctly. */
        g_set_object (&old_files->data, snapped_file);
    }

    TwoLists *changed_files_lists = g_new0 (TwoLists, 1);

    changed_files_lists->old_files = old_files;
    changed_files_lists->new_files = new_files;
    g_async_queue_push (changed_files, changed_files_lists);
}

static void
real_batch_rename (GList                         *files,
                   GList                         *new_names,
                   AmmoniteFileOperationCallback  callback,
                   gpointer                       callback_data)
{
    GList *l1, *l2;
    g_autolist (GFile) old_files = NULL, new_files = NULL;
    AmmoniteFileOperation *op;
    gsize len = 0;
    /* `staged_targets` and `staged_files` are for the file => renamed file map
     * and a reverse map respectively. */
    g_autoptr (GHashTable) staged_targets = g_hash_table_new (g_file_hash, (GEqualFunc) g_file_equal);
    g_autoptr (GHashTable) staged_files = g_hash_table_new_full (g_file_hash,
                                                                 (GEqualFunc) g_file_equal,
                                                                 g_object_unref, g_object_unref);
    /* `stub_files` are for files that will not or cannot be renamed without
     * trying. */
    g_autoptr (GHashTable) stub_files = g_hash_table_new_full (g_file_hash,
                                                               (GEqualFunc) g_file_equal,
                                                               g_object_unref, NULL);
    GHashTableIter hash_iter;
    GFile *key, *value;
    g_autoptr (GAsyncQueue) changed_files = g_async_queue_new ();
    GThreadPool *thread_pool = NULL;
    g_autoptr (GAsyncQueue) rollback_files = g_async_queue_new ();
    g_autoptr (GError) skip_error = NULL;

    /* Set up a batch renaming operation.
     * FIXME: Use a variant that doesn't take a single file, or move to
     * file-operations.
     */
    op = ammonite_file_operation_new (files->data, callback, callback_data);
    op->files = ammonite_file_list_copy (files);
    op->renamed_files = 0;
    op->skipped_files = 0;

    /* Collect valid files in hashtables */
    for (l1 = files, l2 = new_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next)
    {
        AmmoniteFile *file = AMMONITE_FILE (l1->data);
        GString *new_name = l2->data;
        GFile *location = ammonite_file_get_location (file);
        const char *new_file_name = ammonite_file_can_rename_file (file, new_name->str,
                                                                   NULL, NULL);

        len++;

        if (new_file_name == NULL)
        {
            g_hash_table_add (stub_files, location);

            if (name_is (file, new_name->str))
            {
                /* We are ignoring this rename, and not even adding to the undo list. */
                op->renamed_files++;
            }
            else
            {
                op->skipped_files++;
            }

            continue;
        }

        g_autoptr (GFile) parent = ammonite_file_get_parent_location (file);
        g_autoptr (GFile) target = g_file_get_child (parent, new_file_name);

        if (g_hash_table_lookup (stub_files, target) != NULL)
        {
            /* file depends on a stub, so it becomes a stub itself */
            op->skipped_files++;
            g_hash_table_add (stub_files, location);

            continue;
        }

        file->details->operations_in_progress = g_list_prepend (file->details->operations_in_progress,
                                                                op);
        g_assert (g_hash_table_insert (staged_targets, location, target));
        g_assert (g_hash_table_insert (staged_files, g_steal_pointer (&target), location));
    }

    thread_pool = g_thread_pool_new ((GFunc) rename_file_chain, changed_files,
                                     MIN (32, len), FALSE, NULL);

    /* Discover and assemble individual dependency chains by walking the hash
     * table and push them into thread pools. */
    g_hash_table_iter_init (&hash_iter, staged_targets);
    while (g_hash_table_iter_next (&hash_iter, (gpointer *) &key, (gpointer *) &value))
    {
        GFile *chain_file = key, *iter_start_file = key;
        RenameChainData *rename_chain = g_new0 (RenameChainData, 1);
        rename_chain->chain_files = g_ptr_array_new_full (3, g_object_unref);
        rename_chain->op = op;
        rename_chain->rollback_queue = rollback_files;

        /* Iterate to find out if it's a chain or a cycle. */
        while (TRUE)
        {
            GFile *prev_chain_file = g_hash_table_lookup (staged_targets, chain_file);

            if (prev_chain_file == NULL)
            {
                /* Reached chain start */
                break;
            }

            if (g_file_equal (iter_start_file, prev_chain_file))
            {
                /* We found a cycle */
                rename_chain->is_cycle = TRUE;
                g_ptr_array_add (rename_chain->chain_files, g_object_ref (iter_start_file));

                break;
            }

            chain_file = prev_chain_file;
        }

        GFile *target = chain_file;

        iter_start_file = chain_file;

        /* Walk the reverse hash table to collect a list of the files to be
         * renamed in an ordered list, but terminate appropriately with a
         * cycle. */
        do
        {
            g_autoptr (GFile) src = NULL;
            GFile *new_target = NULL;

            g_ptr_array_add (rename_chain->chain_files, target);

            if (g_hash_table_steal_extended (staged_files, target, (gpointer *) &src, (gpointer *) &new_target))
            {
                g_hash_table_remove (staged_targets, new_target);

                if (src == target)
                {
                    src = NULL;
                }

                if (rename_chain->is_cycle && g_file_equal (new_target, iter_start_file))
                {
                    g_object_unref (new_target);
                    break;
                }
            }

            target = new_target;
        }
        while (target != NULL);

        g_thread_pool_push (thread_pool, rename_chain, NULL);
        g_hash_table_iter_init (&hash_iter, staged_targets);
    }

    /* Wait for threads to finish and collect the changes. */
    g_thread_pool_free (thread_pool, FALSE, TRUE);
    while (g_async_queue_length (changed_files) > 0)
    {
        g_autofree TwoLists *changed_files_lists = g_async_queue_pop (changed_files);

        old_files = g_list_concat (changed_files_lists->old_files, old_files);
        new_files = g_list_concat (changed_files_lists->new_files, new_files);
    }

    /* Attempt to rename back the files that were renamed to temporary names.
     * Don't handle any errors though, it's likely futile. */
    while (g_async_queue_length (rollback_files) > 0)
    {
        g_autofree TwoLists *rollback_file_lists = g_async_queue_pop (rollback_files);
        g_autoptr (GList) old_file_list = rollback_file_lists->old_files;
        g_autoptr (GList) new_file_list = rollback_file_lists->new_files;

        g_file_move (new_file_list->data, old_file_list->data,
                     G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);

        AmmoniteFile *old_file = ammonite_file_get_existing (old_file_list->data);
        AmmoniteFile *new_file = ammonite_file_get_existing (new_file_list->data);

        if (old_file != NULL)
        {
            ammonite_file_invalidate_attributes (old_file, AMMONITE_ATTRIBUTE_INFO);
        }
        if (new_file != NULL)
        {
            ammonite_file_invalidate_attributes (new_file, AMMONITE_ATTRIBUTE_INFO);
        }
    }

    /* Invalidate attributes of modified files in idle. */
    for (l1 = old_files, l2 = new_files; l1 != NULL; l1 = l1->next, l2 = l2->next)
    {
        AmmoniteFile *old_file = ammonite_file_get_existing (l1->data);
        AmmoniteFile *new_file = ammonite_file_get_existing (l2->data);

        if (old_file != NULL)
        {
            ammonite_file_invalidate_attributes (old_file, AMMONITE_ATTRIBUTE_INFO);
        }
        if (new_file != NULL)
        {
            ammonite_file_invalidate_attributes (new_file, AMMONITE_ATTRIBUTE_INFO);
        }
    }

    /* Tell the undo manager a batch rename is taking place if at least
     * a file has been renamed*/
    if (!ammonite_file_undo_manager_is_operating () && op->skipped_files != len)
    {
        op->undo_info = ammonite_file_undo_info_batch_rename_new (g_list_length (new_files));

        ammonite_file_undo_info_batch_rename_set_data_pre (AMMONITE_FILE_UNDO_INFO_BATCH_RENAME (op->undo_info),
                                                           g_steal_pointer (&old_files));

        ammonite_file_undo_info_batch_rename_set_data_post (AMMONITE_FILE_UNDO_INFO_BATCH_RENAME (op->undo_info),
                                                            g_steal_pointer (&new_files));

        ammonite_file_undo_manager_set_action (op->undo_info);
    }

    if (op->skipped_files > 1)
    {
        skip_error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                  "Skipped %d file(s)", op->skipped_files);
    }

    ammonite_file_operation_complete (op, NULL, skip_error);
}

void
ammonite_file_batch_rename (GList                         *files,
                            GList                         *new_names,
                            AmmoniteFileOperationCallback  callback,
                            gpointer                       callback_data)
{
    real_batch_rename (files,
                       new_names,
                       callback,
                       callback_data);
}

gboolean
ammonite_file_rename_in_progress (AmmoniteFile *file)
{
    GList *node;
    AmmoniteFileOperation *op;

    for (node = file->details->operations_in_progress; node != NULL; node = node->next)
    {
        op = node->data;
        if (op->is_rename)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void
ammonite_file_cancel (AmmoniteFile                  *file,
                      AmmoniteFileOperationCallback  callback,
                      gpointer                       callback_data)
{
    GList *node, *next;
    AmmoniteFileOperation *op;

    for (node = file->details->operations_in_progress; node != NULL; node = next)
    {
        next = node->next;
        op = node->data;

        g_assert (op->file == file);
        if (op->callback == callback && op->callback_data == callback_data)
        {
            ammonite_file_operation_cancel (op);
        }
    }
}

gboolean
ammonite_file_matches_uri (AmmoniteFile *file,
                           const char   *match_uri)
{
    GFile *match_file, *location;
    gboolean result;

    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);
    g_return_val_if_fail (match_uri != NULL, FALSE);

    location = ammonite_file_get_location (file);
    match_file = g_file_new_for_uri (match_uri);
    result = g_file_equal (location, match_file);
    g_object_unref (location);
    g_object_unref (match_file);

    return result;
}

int
ammonite_file_compare_location (AmmoniteFile *file_1,
                                AmmoniteFile *file_2)
{
    GFile *loc_a, *loc_b;
    gboolean res;

    loc_a = ammonite_file_get_location (file_1);
    loc_b = ammonite_file_get_location (file_2);

    res = !g_file_equal (loc_a, loc_b);

    g_object_unref (loc_a);
    g_object_unref (loc_b);

    return (gint) res;
}

/**
 * ammonite_file_has_local_path:
 *
 * @file: a #AmmoniteFile
 *
 * Checks whether this file has an obtainable local paths. Usually, this means
 * the local path can be obtained by calling g_file_get_path(); this includes
 * native and FUSE files. As an exception, the local URI for files in recent://
 * can only be obtained from the G_FILE_ATTRIBUTE_STANDARD_TARGET_URI attribute.
 *
 * Returns: %TRUE if a local path is known to be obtainable for this file.
 */
gboolean
ammonite_file_has_local_path (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return ammonite_directory_is_local_or_fuse (file->details->directory);
}

static void
update_link (AmmoniteFile *link_file,
             AmmoniteFile *target_file)
{
    g_assert (AMMONITE_IS_FILE (link_file));
    g_assert (AMMONITE_IS_FILE (target_file));

    /* FIXME bugzilla.gnome.org 42044: If we don't put any code
     * here then the hash table is a waste of time.
     */
}

static GList *
get_link_files (AmmoniteFile *target_file)
{
    char *uri;
    GList **link_files;

    if (symbolic_links == NULL)
    {
        link_files = NULL;
    }
    else
    {
        uri = ammonite_file_get_uri (target_file);
        link_files = g_hash_table_lookup (symbolic_links, uri);
        g_free (uri);
    }
    if (link_files)
    {
        return ammonite_file_list_copy (*link_files);
    }
    return NULL;
}

static void
update_links_if_target (AmmoniteFile *target_file)
{
    GList *link_files, *p;

    link_files = get_link_files (target_file);
    for (p = link_files; p != NULL; p = p->next)
    {
        update_link (AMMONITE_FILE (p->data), target_file);
    }
    ammonite_file_list_free (link_files);
}

static gboolean
update_info_internal (AmmoniteFile *file,
                      GFileInfo    *info,
                      gboolean      update_name)
{
    GList *node;
    gboolean changed;
    gboolean is_symlink, is_hidden, is_mountpoint;
    gboolean has_permissions;
    guint32 permissions;
    gboolean can_read, can_write, can_execute, can_delete, can_trash, can_rename, can_mount, can_unmount, can_eject;
    gboolean can_start, can_start_degraded, can_stop, can_poll_for_media, is_media_check_automatic;
    GDriveStartStopType start_stop_type;
    gboolean has_uid = FALSE;
    uid_t uid = 0;
    gboolean has_gid = FALSE;
    gid_t gid = 0;
    goffset size;
    int sort_order;
    time_t atime, mtime, btime;
    time_t trash_time;
    time_t recency;
    const char *time_string;
    const char *symlink_name, *mime_type, *selinux_context, *name;
    GFileType file_type;
    GIcon *icon;
    const char *filesystem_id;
    const char *trash_orig_path;
    const char *group, *owner, *owner_real;
    gboolean free_owner, free_group;
    const char *edit_name;

    if (file->details->is_gone)
    {
        return FALSE;
    }

    if (info == NULL)
    {
        ammonite_file_mark_gone (file);
        return TRUE;
    }

    file->details->file_info_is_up_to_date = TRUE;

    /* FIXME bugzilla.gnome.org 42044: Need to let links that
     * point to the old name know that the file has been renamed.
     */

    remove_from_link_hash_table (file);

    changed = FALSE;

    if (!file->details->got_file_info)
    {
        changed = TRUE;
    }
    file->details->got_file_info = TRUE;

    edit_name = g_file_info_get_attribute_string (info,
                                                  G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);
    changed |= ammonite_file_set_display_name (file,
                                               g_file_info_get_display_name (info),
                                               edit_name,
                                               FALSE);

    file_type = g_file_info_get_file_type (info);
    if (file->details->type != file_type)
    {
        changed = TRUE;
    }
    file->details->type = file_type;

    if (!ammonite_file_is_directory (file))
    {
        file->details->directory_count_is_up_to_date = TRUE;
        file->details->directory_count_failed = FALSE;
        file->details->got_directory_count = FALSE;
    }

    if (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL) ||
        file_type == G_FILE_TYPE_SHORTCUT ||
        file_type == G_FILE_TYPE_MOUNTABLE ||
        ammonite_file_is_in_recent (file))
    {
        if (g_set_str (&file->details->activation_uri,
                       g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI)))
        {
            changed = TRUE;
        }
    }

    is_symlink = g_file_info_get_attribute_boolean (info,
                                                    G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);
    if (file->details->is_symlink != is_symlink)
    {
        changed = TRUE;
    }
    file->details->is_symlink = is_symlink;

    is_hidden = g_file_info_get_attribute_boolean (info,
                                                   G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
                g_file_info_get_attribute_boolean (info,
                                                   G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP);
    if (file->details->is_hidden != is_hidden)
    {
        changed = TRUE;
    }
    file->details->is_hidden = is_hidden;

    is_mountpoint = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_UNIX_IS_MOUNTPOINT);
    if (file->details->is_mountpoint != is_mountpoint)
    {
        changed = TRUE;
    }
    file->details->is_mountpoint = is_mountpoint;

    has_permissions = g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE);
    permissions = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);
    if (file->details->has_permissions != has_permissions ||
        file->details->permissions != permissions)
    {
        changed = TRUE;
    }
    file->details->has_permissions = has_permissions;
    file->details->permissions = permissions;

    /* We default to TRUE for this if we can't know */
    can_read = TRUE;
    can_write = TRUE;
    can_execute = TRUE;
    can_delete = TRUE;
    can_rename = TRUE;
    can_trash = FALSE;
    can_mount = FALSE;
    can_unmount = FALSE;
    can_eject = FALSE;
    can_start = FALSE;
    can_start_degraded = FALSE;
    can_stop = FALSE;
    can_poll_for_media = FALSE;
    is_media_check_automatic = FALSE;
    start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
        can_read = g_file_info_get_attribute_boolean (info,
                                                      G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
    {
        can_write = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
    {
        can_execute = g_file_info_get_attribute_boolean (info,
                                                         G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE))
    {
        can_delete = g_file_info_get_attribute_boolean (info,
                                                        G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH))
    {
        can_trash = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME))
    {
        can_rename = g_file_info_get_attribute_boolean (info,
                                                        G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT))
    {
        can_mount = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT))
    {
        can_unmount = g_file_info_get_attribute_boolean (info,
                                                         G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT))
    {
        can_eject = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START))
    {
        can_start = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START_DEGRADED))
    {
        can_start_degraded = g_file_info_get_attribute_boolean (info,
                                                                G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START_DEGRADED);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_STOP))
    {
        can_stop = g_file_info_get_attribute_boolean (info,
                                                      G_FILE_ATTRIBUTE_MOUNTABLE_CAN_STOP);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_START_STOP_TYPE))
    {
        start_stop_type = g_file_info_get_attribute_uint32 (info,
                                                            G_FILE_ATTRIBUTE_MOUNTABLE_START_STOP_TYPE);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_POLL))
    {
        can_poll_for_media = g_file_info_get_attribute_boolean (info,
                                                                G_FILE_ATTRIBUTE_MOUNTABLE_CAN_POLL);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_IS_MEDIA_CHECK_AUTOMATIC))
    {
        is_media_check_automatic = g_file_info_get_attribute_boolean (info,
                                                                      G_FILE_ATTRIBUTE_MOUNTABLE_IS_MEDIA_CHECK_AUTOMATIC);
    }
    if (file->details->can_read != can_read ||
        file->details->can_write != can_write ||
        file->details->can_execute != can_execute ||
        file->details->can_delete != can_delete ||
        file->details->can_trash != can_trash ||
        file->details->can_rename != can_rename ||
        file->details->can_mount != can_mount ||
        file->details->can_unmount != can_unmount ||
        file->details->can_eject != can_eject ||
        file->details->can_start != can_start ||
        file->details->can_start_degraded != can_start_degraded ||
        file->details->can_stop != can_stop ||
        file->details->start_stop_type != start_stop_type ||
        file->details->can_poll_for_media != can_poll_for_media ||
        file->details->is_media_check_automatic != is_media_check_automatic)
    {
        changed = TRUE;
    }

    file->details->can_read = can_read;
    file->details->can_write = can_write;
    file->details->can_execute = can_execute;
    file->details->can_delete = can_delete;
    file->details->can_trash = can_trash;
    file->details->can_rename = can_rename;
    file->details->can_mount = can_mount;
    file->details->can_unmount = can_unmount;
    file->details->can_eject = can_eject;
    file->details->can_start = can_start;
    file->details->can_start_degraded = can_start_degraded;
    file->details->can_stop = can_stop;
    file->details->start_stop_type = start_stop_type;
    file->details->can_poll_for_media = can_poll_for_media;
    file->details->is_media_check_automatic = is_media_check_automatic;

    free_owner = FALSE;
    owner = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER);
    owner_real = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER_REAL);
    free_group = FALSE;
    group = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_GROUP);

    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_UID))
    {
        uid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID);
        has_uid = TRUE;
        if (owner == NULL)
        {
            free_owner = TRUE;
            owner = g_strdup_printf ("%d", uid);
        }
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_GID))
    {
        gid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID);
        has_gid = TRUE;
        if (group == NULL)
        {
            free_group = TRUE;
            group = g_strdup_printf ("%d", gid);
        }
    }
    if (file->details->has_uid != has_uid ||
        (file->details->has_uid && file->details->uid != uid) ||
        file->details->has_gid != has_gid ||
        (file->details->has_gid && file->details->gid != gid))
    {
        changed = TRUE;
    }
    file->details->has_uid = has_uid;
    file->details->uid = uid;
    file->details->has_gid = has_gid;
    file->details->gid = gid;

    if (g_strcmp0 (file->details->owner, owner) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->owner, g_ref_string_release);
        file->details->owner = g_ref_string_new_intern (owner);
    }

    if (g_strcmp0 (file->details->owner_real, owner_real) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->owner_real, g_ref_string_release);
        file->details->owner_real = g_ref_string_new_intern (owner_real);
    }

    if (g_strcmp0 (file->details->group, group) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->group, g_ref_string_release);
        file->details->group = g_ref_string_new_intern (group);
    }

    if (free_owner)
    {
        g_free ((char *) owner);
    }
    if (free_group)
    {
        g_free ((char *) group);
    }

    size = -1;
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
    {
        size = g_file_info_get_size (info);
    }
    if (file->details->size != size)
    {
        changed = TRUE;
    }
    file->details->size = size;

    sort_order = g_file_info_get_attribute_int32 (info,
                                                  G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER);
    if (file->details->sort_order != sort_order)
    {
        changed = TRUE;
    }
    file->details->sort_order = sort_order;

    atime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_ACCESS);
    mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    btime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_CREATED);
    if (file->details->atime != atime ||
        file->details->btime != btime)
    {
        changed = TRUE;
    }
    if (file->details->mtime != mtime)
    {
        file->details->thumbnail_info_is_up_to_date = FALSE;
        file->details->thumbnail_is_up_to_date = FALSE;

        changed = TRUE;
    }
    file->details->atime = atime;
    file->details->mtime = mtime;
    file->details->btime = btime;

    if (ammonite_file_has_thumbnail (file) &&
        file->details->thumbnail_mtime != 0 &&
        file->details->thumbnail_mtime != mtime)
    {
        file->details->thumbnail_info_is_up_to_date = FALSE;
        file->details->thumbnail_is_up_to_date = FALSE;
        changed = TRUE;
    }

    icon = g_file_info_get_icon (info);
    if (!g_icon_equal (icon, file->details->icon))
    {
        changed = TRUE;

        if (file->details->icon)
        {
            g_object_unref (file->details->icon);
        }
        file->details->icon = g_object_ref (icon);
    }

    symlink_name = g_file_info_get_attribute_byte_string (info,
                                                          G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);
    if (g_set_str (&file->details->symlink_name, symlink_name))
    {
        changed = TRUE;
    }

    mime_type = g_file_info_get_attribute_string (info,
                                                  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
    if (mime_type == NULL)
    {
        mime_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
    }
    if (g_strcmp0 (file->details->mime_type, mime_type) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->mime_type, g_ref_string_release);
        file->details->mime_type = g_ref_string_new_intern (mime_type);
    }

    selinux_context = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_SELINUX_CONTEXT);
    if (g_set_str (&file->details->selinux_context, selinux_context))
    {
        changed = TRUE;
    }

    filesystem_id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
    if (g_strcmp0 (file->details->filesystem_id, filesystem_id) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);
        file->details->filesystem_id = g_ref_string_new_intern (filesystem_id);
    }

    trash_time = 0;
    time_string = g_file_info_get_attribute_string (info, "trash::deletion-date");
    if (time_string != NULL)
    {
        g_autoptr (GTimeZone) tz = g_time_zone_new_local ();
        g_autoptr (GDateTime) date_time = g_date_time_new_from_iso8601 (time_string, tz);

        trash_time = date_time != NULL ? g_date_time_to_unix (date_time) : 0;
    }
    if (file->details->trash_time != trash_time)
    {
        changed = TRUE;
        file->details->trash_time = trash_time;
    }

    recency = g_file_info_get_attribute_int64 (info, G_FILE_ATTRIBUTE_RECENT_MODIFIED);
    if (file->details->recency != recency)
    {
        changed = TRUE;
        file->details->recency = recency;
    }

    trash_orig_path = g_file_info_get_attribute_byte_string (info, "trash::orig-path");
    if (g_set_str (&file->details->trash_orig_path, trash_orig_path))
    {
        changed = TRUE;
    }

    file->details->has_preview_icon =
        g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_PREVIEW_ICON);

    changed |=
        ammonite_file_update_metadata_from_info (file, info);

    if (update_name)
    {
        name = g_file_info_get_name (info);
        if (file->details->name == NULL ||
            strcmp (file->details->name, name) != 0)
        {
            changed = TRUE;

            node = ammonite_directory_begin_file_name_change
                       (file->details->directory, file);

            g_clear_pointer (&file->details->name, g_ref_string_release);
            if (g_strcmp0 (file->details->display_name, name) == 0)
            {
                file->details->name = g_ref_string_acquire (file->details->display_name);
            }
            else
            {
                file->details->name = g_ref_string_new (name);
            }

            if (!file->details->got_custom_display_name &&
                g_file_info_get_display_name (info) == NULL)
            {
                /* If the file info's display name is NULL,
                 * ammonite_file_set_display_name() did
                 * not unset the display name.
                 */
                ammonite_file_clear_display_name (file);
            }

            ammonite_directory_end_file_name_change
                (file->details->directory, file, node);
        }
    }

    if (changed)
    {
        add_to_link_hash_table (file);

        update_links_if_target (file);
    }

    return changed;
}

static gboolean
update_info_and_name (AmmoniteFile *file,
                      GFileInfo    *info)
{
    return update_info_internal (file, info, TRUE);
}

gboolean
ammonite_file_update_info (AmmoniteFile *file,
                           GFileInfo    *info)
{
    return update_info_internal (file, info, FALSE);
}

gboolean
ammonite_file_update_thumbnail_info (AmmoniteFile *file,
                                     GFileInfo    *info)
{
    gboolean changed = FALSE;

    if (!file->details->thumbnail_info_is_up_to_date)
    {
        file->details->thumbnail_info_is_up_to_date = TRUE;
        changed = TRUE;
    }

    const gchar *thumbnail_path = g_file_info_get_attribute_byte_string (info,
                                                                         G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
    if (g_set_str (&file->details->thumbnail_path, thumbnail_path))
    {
        changed = TRUE;
    }

    gboolean cache_is_valid = g_file_info_get_attribute_boolean (info,
                                                                 G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID);
    gboolean thumbnailing_failed = cache_is_valid &&
                                   g_file_info_get_attribute_boolean (info,
                                                                      G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
    if (file->details->thumbnailing_failed != thumbnailing_failed)
    {
        changed = TRUE;
        file->details->thumbnailing_failed = thumbnailing_failed;
    }

    return changed;
}

static gboolean
update_name_internal (AmmoniteFile *file,
                      const char   *name,
                      gboolean      in_directory)
{
    GList *node;

    g_assert (name != NULL);

    if (file->details->is_gone)
    {
        return FALSE;
    }

    if (name_is (file, name))
    {
        return FALSE;
    }

    node = NULL;
    if (in_directory)
    {
        node = ammonite_directory_begin_file_name_change
                   (file->details->directory, file);
    }

    g_clear_pointer (&file->details->name, g_ref_string_release);
    file->details->name = g_ref_string_new (name);

    if (!file->details->got_custom_display_name)
    {
        ammonite_file_clear_display_name (file);
    }

    if (in_directory)
    {
        ammonite_directory_end_file_name_change
            (file->details->directory, file, node);
    }

    return TRUE;
}

gboolean
ammonite_file_update_name_and_directory (AmmoniteFile      *file,
                                         const char        *name,
                                         AmmoniteDirectory *new_directory)
{
    AmmoniteDirectory *old_directory;
    FileMonitors *monitors;

    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (file->details->directory), FALSE);
    g_return_val_if_fail (!file->details->is_gone, FALSE);
    g_return_val_if_fail (!ammonite_file_is_self_owned (file), FALSE);
    g_return_val_if_fail (AMMONITE_IS_DIRECTORY (new_directory), FALSE);

    old_directory = file->details->directory;
    if (old_directory == new_directory)
    {
        if (name)
        {
            return update_name_internal (file, name, TRUE);
        }
        else
        {
            return FALSE;
        }
    }

    ammonite_file_ref (file);

    /* FIXME bugzilla.gnome.org 42044: Need to let links that
     * point to the old name know that the file has been moved.
     */

    remove_from_link_hash_table (file);

    monitors = ammonite_directory_remove_file_monitors (old_directory, file);
    ammonite_directory_remove_file (old_directory, file);

    ammonite_file_set_directory (file, new_directory);

    if (name)
    {
        update_name_internal (file, name, FALSE);
    }

    ammonite_directory_add_file (new_directory, file);
    ammonite_directory_add_file_monitors (new_directory, file, monitors);

    add_to_link_hash_table (file);

    update_links_if_target (file);

    ammonite_file_unref (file);

    return TRUE;
}

static Knowledge
get_item_count (AmmoniteFile *file,
                guint        *count)
{
    gboolean known, unreadable;

    known = ammonite_file_get_directory_item_count
                (file, count, &unreadable);
    if (!known)
    {
        return UNKNOWN;
    }
    if (unreadable)
    {
        return UNKNOWABLE;
    }
    return KNOWN;
}

static Knowledge
get_size (AmmoniteFile *file,
          goffset      *size)
{
    /* If we tried and failed, then treat it like there is no size
     * to know.
     */
    if (file->details->get_info_failed)
    {
        return UNKNOWABLE;
    }

    /* If the info is NULL that means we haven't even tried yet,
     * so it's just unknown, not unknowable.
     */
    if (!file->details->got_file_info)
    {
        return UNKNOWN;
    }

    /* If we got info with no size in it, it means there is no
     * such thing as a size as far as gnome-vfs is concerned,
     * so "unknowable".
     */
    if (file->details->size == -1)
    {
        return UNKNOWABLE;
    }

    /* We have a size! */
    *size = file->details->size;
    return KNOWN;
}

static Knowledge
get_time (AmmoniteFile     *file,
          time_t           *time_out,
          AmmoniteDateType  type)
{
    time_t time;

    /* If we tried and failed, then treat it like there is no size
     * to know.
     */
    if (file->details->get_info_failed)
    {
        return UNKNOWABLE;
    }

    /* If the info is NULL that means we haven't even tried yet,
     * so it's just unknown, not unknowable.
     */
    if (!file->details->got_file_info)
    {
        return UNKNOWN;
    }

    switch (type)
    {
        case AMMONITE_DATE_TYPE_MODIFIED:
        {
            time = file->details->mtime;
        }
        break;

        case AMMONITE_DATE_TYPE_ACCESSED:
        {
            time = file->details->atime;
        }
        break;

        case AMMONITE_DATE_TYPE_CREATED:
        {
            time = file->details->btime;
        }
        break;

        case AMMONITE_DATE_TYPE_TRASHED:
        {
            time = file->details->trash_time;
        }
        break;

        case AMMONITE_DATE_TYPE_RECENCY:
        {
            time = file->details->recency;
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }

    *time_out = time;

    /* If we got info with no modification time in it, it means
     * there is no such thing as a modification time as far as
     * gnome-vfs is concerned, so "unknowable".
     */
    if (time == 0)
    {
        return UNKNOWABLE;
    }
    return KNOWN;
}

static int
compare_directories_by_count (AmmoniteFile *file_1,
                              AmmoniteFile *file_2)
{
    /* Sort order:
     *   Directories with unknown # of items
     *   Directories with "unknowable" # of items
     *   Directories with 0 items
     *   Directories with n items
     */

    Knowledge count_known_1, count_known_2;
    guint count_1, count_2;

    count_known_1 = get_item_count (file_1, &count_1);
    count_known_2 = get_item_count (file_2, &count_2);

    if (count_known_1 > count_known_2)
    {
        return -1;
    }
    if (count_known_1 < count_known_2)
    {
        return +1;
    }

    /* count_known_1 and count_known_2 are equal now. Check if count
     * details are UNKNOWABLE or UNKNOWN.
     */
    if (count_known_1 == UNKNOWABLE || count_known_1 == UNKNOWN)
    {
        return 0;
    }

    if (count_1 < count_2)
    {
        return -1;
    }
    if (count_1 > count_2)
    {
        return +1;
    }

    return 0;
}

static int
compare_files_by_size (AmmoniteFile *file_1,
                       AmmoniteFile *file_2)
{
    /* Sort order:
     *   Files with unknown size.
     *   Files with "unknowable" size.
     *   Files with smaller sizes.
     *   Files with large sizes.
     */

    Knowledge size_known_1, size_known_2;
    goffset size_1 = 0, size_2 = 0;

    size_known_1 = get_size (file_1, &size_1);
    size_known_2 = get_size (file_2, &size_2);

    if (size_known_1 > size_known_2)
    {
        return -1;
    }
    if (size_known_1 < size_known_2)
    {
        return +1;
    }

    /* size_known_1 and size_known_2 are equal now. Check if size
     * details are UNKNOWABLE or UNKNOWN
     */
    if (size_known_1 == UNKNOWABLE || size_known_1 == UNKNOWN)
    {
        return 0;
    }

    if (size_1 < size_2)
    {
        return -1;
    }
    if (size_1 > size_2)
    {
        return +1;
    }

    return 0;
}

static int
compare_by_size (AmmoniteFile *file_1,
                 AmmoniteFile *file_2)
{
    /* Sort order:
     *   Directories with n items
     *   Directories with 0 items
     *   Directories with "unknowable" # of items
     *   Directories with unknown # of items
     *   Files with large sizes.
     *   Files with smaller sizes.
     *   Files with "unknowable" size.
     *   Files with unknown size.
     */

    gboolean is_directory_1, is_directory_2;

    is_directory_1 = ammonite_file_is_directory (file_1);
    is_directory_2 = ammonite_file_is_directory (file_2);

    if (is_directory_1 && !is_directory_2)
    {
        return -1;
    }
    if (is_directory_2 && !is_directory_1)
    {
        return +1;
    }

    if (is_directory_1)
    {
        return compare_directories_by_count (file_1, file_2);
    }
    else
    {
        return compare_files_by_size (file_1, file_2);
    }
}

static int
compare_by_display_name (AmmoniteFile *file_1,
                         AmmoniteFile *file_2)
{
    const char *name_1, *name_2;
    const char *key_1, *key_2;
    gboolean sort_last_1, sort_last_2;
    int compare;

    name_1 = ammonite_file_peek_display_name (file_1);
    name_2 = ammonite_file_peek_display_name (file_2);

    sort_last_1 = name_1[0] == SORT_LAST_CHAR1 || name_1[0] == SORT_LAST_CHAR2;
    sort_last_2 = name_2[0] == SORT_LAST_CHAR1 || name_2[0] == SORT_LAST_CHAR2;

    if (sort_last_1 && !sort_last_2)
    {
        compare = +1;
    }
    else if (!sort_last_1 && sort_last_2)
    {
        compare = -1;
    }
    else
    {
        key_1 = ammonite_file_peek_display_name_collation_key (file_1);
        key_2 = ammonite_file_peek_display_name_collation_key (file_2);
        compare = strcmp (key_1, key_2);
    }

    return compare;
}

static inline int
compare_by_name (AmmoniteFile *file_1,
                 AmmoniteFile *file_2)
{
    return g_strcmp0 (file_1->details->name,
                      file_2->details->name);
}

static int
compare_by_directory_name (AmmoniteFile *file_1,
                           AmmoniteFile *file_2)
{
    const gchar *directory_key1 = ammonite_file_get_directory_name (file_1);
    const gchar *directory_key2 = ammonite_file_get_directory_name (file_2);

    return strcmp (directory_key1, directory_key2);
}

static int
compare_by_type (AmmoniteFile *file_1,
                 AmmoniteFile *file_2)
{
    gboolean is_directory_1;
    gboolean is_directory_2;
    const char *type_string_1;
    const char *type_string_2;
    int result;

    /* Directories go first. Then, if mime types are identical,
     * don't bother getting strings (for speed). This assumes
     * that the string is dependent entirely on the mime type,
     * which is true now but might not be later.
     */
    is_directory_1 = ammonite_file_is_directory (file_1);
    is_directory_2 = ammonite_file_is_directory (file_2);

    if (is_directory_1 && is_directory_2)
    {
        return 0;
    }

    if (is_directory_1)
    {
        return -1;
    }

    if (is_directory_2)
    {
        return +1;
    }

    if (file_1->details->mime_type != NULL &&
        file_2->details->mime_type != NULL &&
        strcmp (file_1->details->mime_type,
                file_2->details->mime_type) == 0)
    {
        return 0;
    }

    type_string_1 = ammonite_file_get_type_as_string_no_extra_text (file_1);
    type_string_2 = ammonite_file_get_type_as_string_no_extra_text (file_2);

    if (type_string_1 == NULL || type_string_2 == NULL)
    {
        if (type_string_1 != NULL)
        {
            return -1;
        }

        if (type_string_2 != NULL)
        {
            return 1;
        }

        return 0;
    }

    result = g_utf8_collate (type_string_1, type_string_2);
    if (result == 0)
    {
        /* Among files of the same (generic) type, sort them by mime type. */
        result = g_utf8_collate (file_1->details->mime_type, file_2->details->mime_type);
    }

    return result;
}

static int
compare_by_starred (AmmoniteFile *file_1,
                    AmmoniteFile *file_2)
{
    AmmoniteTagManager *tag_manager = ammonite_tag_manager_get ();
    g_autofree gchar *uri_1 = NULL;
    g_autofree gchar *uri_2 = NULL;
    gboolean file_1_is_starred;
    gboolean file_2_is_starred;

    uri_1 = ammonite_file_get_activation_uri (file_1);
    uri_2 = ammonite_file_get_activation_uri (file_2);

    file_1_is_starred = ammonite_tag_manager_file_is_starred (tag_manager,
                                                              uri_1);
    file_2_is_starred = ammonite_tag_manager_file_is_starred (tag_manager,
                                                              uri_2);
    if (!!file_1_is_starred == !!file_2_is_starred)
    {
        return 0;
    }
    else if (file_1_is_starred && !file_2_is_starred)
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

static Knowledge
get_search_relevance (AmmoniteFile *file,
                      gdouble      *relevance_out)
{
    /* we're only called in search directories, and in that
     * case, the relevance is always known (or zero).
     */
    *relevance_out = file->details->search_relevance;
    return KNOWN;
}

static int
compare_by_search_relevance (AmmoniteFile *file_1,
                             AmmoniteFile *file_2)
{
    gdouble r_1, r_2;

    get_search_relevance (file_1, &r_1);
    get_search_relevance (file_2, &r_2);

    if (r_1 < r_2)
    {
        return -1;
    }
    if (r_1 > r_2)
    {
        return +1;
    }

    return 0;
}

static int
compare_by_time (AmmoniteFile     *file_1,
                 AmmoniteFile     *file_2,
                 AmmoniteDateType  type)
{
    /* Sort order:
     *   Files with unknown times.
     *   Files with "unknowable" times.
     *   Files with older times.
     *   Files with newer times.
     */

    Knowledge time_known_1, time_known_2;
    time_t time_1, time_2;

    time_1 = 0;
    time_2 = 0;

    time_known_1 = get_time (file_1, &time_1, type);
    time_known_2 = get_time (file_2, &time_2, type);

    if (time_known_1 > time_known_2)
    {
        return -1;
    }
    if (time_known_1 < time_known_2)
    {
        return +1;
    }

    /* Now time_known_1 is equal to time_known_2. Check whether
     * we failed to get modification times for files
     */
    if (time_known_1 == UNKNOWABLE || time_known_1 == UNKNOWN)
    {
        return 0;
    }

    if (time_1 < time_2)
    {
        return -1;
    }
    if (time_1 > time_2)
    {
        return +1;
    }

    return 0;
}

static int
compare_by_full_path (AmmoniteFile *file_1,
                      AmmoniteFile *file_2)
{
    int compare;

    compare = compare_by_directory_name (file_1, file_2);
    if (compare != 0)
    {
        return compare;
    }
    compare = compare_by_display_name (file_1, file_2);
    if (compare != 0)
    {
        return compare;
    }
    return compare_by_name (file_1, file_2);
}

static int
ammonite_file_compare_for_sort_internal (AmmoniteFile *file_1,
                                         AmmoniteFile *file_2,
                                         gboolean      directories_first,
                                         gboolean      reversed)
{
    gboolean is_directory_1, is_directory_2;

    if (directories_first)
    {
        is_directory_1 = ammonite_file_is_directory (file_1);
        is_directory_2 = ammonite_file_is_directory (file_2);

        if (is_directory_1 && !is_directory_2)
        {
            return -1;
        }

        if (is_directory_2 && !is_directory_1)
        {
            return +1;
        }
    }

    if (file_1->details->sort_order < file_2->details->sort_order)
    {
        return reversed ? 1 : -1;
    }
    else if (file_1->details->sort_order > file_2->details->sort_order)
    {
        return reversed ? -1 : 1;
    }

    return 0;
}

static AmmoniteDateType
sort_to_date_type (AmmoniteFileSortType sort_type)
{
    switch (sort_type)
    {
        case AMMONITE_FILE_SORT_BY_MTIME:
        {
            return AMMONITE_DATE_TYPE_MODIFIED;
        }

        case AMMONITE_FILE_SORT_BY_ATIME:
        {
            return AMMONITE_DATE_TYPE_ACCESSED;
        }

        case AMMONITE_FILE_SORT_BY_BTIME:
        {
            return AMMONITE_DATE_TYPE_CREATED;
        }

        case AMMONITE_FILE_SORT_BY_TRASHED_TIME:
        {
            return AMMONITE_DATE_TYPE_TRASHED;
        }

        case AMMONITE_FILE_SORT_BY_RECENCY:
        {
            return AMMONITE_DATE_TYPE_RECENCY;
        }

        default:
        {
            g_return_val_if_reached (AMMONITE_DATE_TYPE_MODIFIED);
        }
    }
}

/**
 * ammonite_file_compare_for_sort:
 * @file_1: A file object
 * @file_2: Another file object
 * @sort_type: Sort criterion
 * @directories_first: Put all directories before any non-directories
 * @reversed: Reverse the order of the items, except that
 * the directories_first flag is still respected.
 *
 * Return value: int < 0 if @file_1 should come before file_2 in a
 * sorted list; int > 0 if @file_2 should come before file_1 in a
 * sorted list; 0 if @file_1 and @file_2 are equal for this sort criterion. Note
 * that each named sort type may actually break ties several ways, with the name
 * of the sort criterion being the primary but not only differentiator.
 **/
int
ammonite_file_compare_for_sort (AmmoniteFile         *file_1,
                                AmmoniteFile         *file_2,
                                AmmoniteFileSortType  sort_type,
                                gboolean              directories_first,
                                gboolean              reversed)
{
    int result;

    if (file_1 == file_2)
    {
        return 0;
    }

    result = ammonite_file_compare_for_sort_internal (file_1, file_2, directories_first, reversed);

    if (result == 0)
    {
        switch (sort_type)
        {
            case AMMONITE_FILE_SORT_BY_DISPLAY_NAME:
            {
                result = compare_by_display_name (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_directory_name (file_1, file_2);
                }
                /* Some GVfs backends like google-drive allow multiple files
                 * with the same display name. */
                if (result == 0)
                {
                    result = compare_by_name (file_1, file_2);
                }
            }
            break;

            case AMMONITE_FILE_SORT_BY_SIZE:
            {
                /* Compare directory sizes ourselves, then if necessary
                 * use GnomeVFS to compare file sizes.
                 */
                result = compare_by_size (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case AMMONITE_FILE_SORT_BY_TYPE:
            {
                /* GnomeVFS doesn't know about our special text for certain
                 * mime types, so we handle the mime-type sorting ourselves.
                 */
                result = compare_by_type (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case AMMONITE_FILE_SORT_BY_STARRED:
            {
                result = compare_by_starred (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case AMMONITE_FILE_SORT_BY_MTIME:
            case AMMONITE_FILE_SORT_BY_ATIME:
            case AMMONITE_FILE_SORT_BY_BTIME:
            case AMMONITE_FILE_SORT_BY_TRASHED_TIME:
            case AMMONITE_FILE_SORT_BY_RECENCY:
            {
                AmmoniteDateType date_type = sort_to_date_type (sort_type);

                result = compare_by_time (file_1, file_2, date_type);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case AMMONITE_FILE_SORT_BY_SEARCH_RELEVANCE:
            {
                result = compare_by_search_relevance (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);

                    /* ensure alphabetical order for files of the same relevance
                     * grows in reverse (higher character = lower relevance) */
                    result = -result;
                }
            }
            break;

            default:
            {
                g_return_val_if_reached (0);
            }
        }

        if (reversed)
        {
            result = -result;
        }
    }

    return result;
}

static AmmoniteFileSortType
quark_to_sort_type (GQuark attribute)
{
    if (attribute == 0 || attribute == attribute_name_q)
    {
        return AMMONITE_FILE_SORT_BY_DISPLAY_NAME;
    }
    else if (attribute == attribute_size_q)
    {
        return AMMONITE_FILE_SORT_BY_SIZE;
    }
    else if (attribute == attribute_type_q)
    {
        return AMMONITE_FILE_SORT_BY_TYPE;
    }
    else if (attribute == attribute_starred_q)
    {
        return AMMONITE_FILE_SORT_BY_STARRED;
    }
    else if (attribute == attribute_modification_date_q ||
             attribute == attribute_date_modified_q ||
             attribute == attribute_date_modified_full_q)
    {
        return AMMONITE_FILE_SORT_BY_MTIME;
    }
    else if (attribute == attribute_accessed_date_q ||
             attribute == attribute_date_accessed_q ||
             attribute == attribute_date_accessed_full_q)
    {
        return AMMONITE_FILE_SORT_BY_ATIME;
    }
    else if (attribute == attribute_date_created_q || attribute == attribute_date_created_full_q)
    {
        return AMMONITE_FILE_SORT_BY_BTIME;
    }
    else if (attribute == attribute_trashed_on_q || attribute == attribute_trashed_on_full_q)
    {
        return AMMONITE_FILE_SORT_BY_TRASHED_TIME;
    }
    else if (attribute == attribute_search_relevance_q)
    {
        return AMMONITE_FILE_SORT_BY_SEARCH_RELEVANCE;
    }
    else if (attribute == attribute_recency_q)
    {
        return AMMONITE_FILE_SORT_BY_RECENCY;
    }
    else
    {
        return AMMONITE_FILE_SORT_BY_OTHER;
    }
}

int
ammonite_file_compare_for_sort_by_attribute_q   (AmmoniteFile *file_1,
                                                 AmmoniteFile *file_2,
                                                 GQuark        attribute,
                                                 gboolean      directories_first,
                                                 gboolean      reversed)
{
    int result;

    if (file_1 == file_2)
    {
        return 0;
    }

    /* Convert certain attributes into AmmoniteFileSortTypes and use
     * ammonite_file_compare_for_sort()
     */
    AmmoniteFileSortType sort_type = quark_to_sort_type (attribute);

    if (sort_type != AMMONITE_FILE_SORT_BY_OTHER)
    {
        return ammonite_file_compare_for_sort (file_1, file_2,
                                               sort_type,
                                               directories_first,
                                               reversed);
    }

    /* it is a normal attribute, compare by strings */

    result = ammonite_file_compare_for_sort_internal (file_1, file_2, directories_first, reversed);

    if (result == 0)
    {
        char *value_1;
        char *value_2;

        value_1 = ammonite_file_get_string_attribute_q (file_1,
                                                        attribute);
        value_2 = ammonite_file_get_string_attribute_q (file_2,
                                                        attribute);

        if (value_1 != NULL && value_2 != NULL)
        {
            result = strcmp (value_1, value_2);
        }

        g_free (value_1);
        g_free (value_2);

        if (reversed)
        {
            result = -result;
        }
    }

    return result;
}

int
ammonite_file_compare_for_sort_by_attribute     (AmmoniteFile *file_1,
                                                 AmmoniteFile *file_2,
                                                 const char   *attribute,
                                                 gboolean      directories_first,
                                                 gboolean      reversed)
{
    return ammonite_file_compare_for_sort_by_attribute_q (file_1, file_2,
                                                          g_quark_from_string (attribute),
                                                          directories_first,
                                                          reversed);
}


/**
 * ammonite_file_compare_display_name:
 * @file: A file object
 * @string: A string we are comparing it with
 *
 * Return value: result of a comparison of the file name and the given string.
 **/
int
ammonite_file_compare_display_name (AmmoniteFile *file,
                                    const char   *string)
{
    const char *name;
    int result;

    g_return_val_if_fail (string != NULL, -1);

    name = ammonite_file_peek_display_name (file);
    result = g_strcmp0 (name, string);
    return result;
}


gboolean
ammonite_file_is_hidden_file (AmmoniteFile *file)
{
    return file->details->is_hidden;
}

/**
 * ammonite_file_should_show:
 * @file: the file to check
 * @show_hidden: whether we want to show hidden files or not
 *
 * Determines if a #AmmoniteFile should be shown. Note that when browsing
 * a trash directory, this function will always return %TRUE.
 *
 * Returns: %TRUE if the file should be shown, %FALSE if it shouldn't.
 */
gboolean
ammonite_file_should_show (AmmoniteFile *file,
                           gboolean      show_hidden)
{
    /* Never hide any files in trash. */
    if (ammonite_file_is_in_trash (file))
    {
        return TRUE;
    }

    if (!show_hidden &&
        file->details->file_info_is_up_to_date &&
        ammonite_file_is_hidden_file (file))
    {
        return FALSE;
    }

    return TRUE;
}

gboolean
ammonite_file_is_home (AmmoniteFile *file)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    location = ammonite_directory_get_location (file->details->directory);
    if (location == NULL)
    {
        return FALSE;
    }

    return ammonite_is_home_directory_file (location, file->details->name);
}

gboolean
ammonite_file_is_in_search (AmmoniteFile *file)
{
    g_autoptr (GFile) location = ammonite_file_get_location (file);

    return g_file_has_uri_scheme (location, SCHEME_SEARCH);
}

/**
 * ammonite_file_list_filter
 * @file_list: (transfer full): A list of files to filter
 * @filter_function: Function that decides which items to keep in list
 * @user_data: Passed as is to @filter_function
 *
 * Removes and frees all files from the given list that don't pass the given
 * filter function.
 *
 * Returns: (transfer full): the filtered file list
 */
AmmoniteFileList *
ammonite_file_list_filter (AmmoniteFileList       *file_list,
                           AmmoniteFileFilterFunc  filter_function,
                           gpointer                user_data)
{
    GList *next = NULL;

    for (AmmoniteFileList *node = file_list; node != NULL; node = next)
    {
        AmmoniteFile *file = node->data;
        next = node->next;

        if (!filter_function (file, user_data))
        {
            ammonite_file_unref (file);
            file_list = g_list_delete_link (file_list, node);
        }
    }

    return file_list;
}

gboolean
ammonite_file_list_are_all_folders (const GList *files)
{
    const GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        if (!ammonite_file_is_directory (AMMONITE_FILE (l->data)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

AmmoniteFileList *
ammonite_file_list_from_uris (const GStrv uris)
{
    if (uris == NULL)
    {
        return NULL;
    }

    AmmoniteFileList *list = NULL;

    for (guint i = 0; uris[i] != NULL; i++)
    {
        list = g_list_prepend (list, ammonite_file_get_by_uri (uris[i]));
    }

    return g_list_reverse (list);
}

static const char *
ammonite_file_get_metadata_impl (AmmoniteFile *file,
                                 const char   *key)
{
    if (file->details->metadata == NULL)
    {
        return NULL;
    }

    guint id = ammonite_metadata_get_id (key);

    return g_hash_table_lookup (file->details->metadata, GUINT_TO_POINTER (id));
}

const char *
ammonite_file_get_metadata (AmmoniteFile *file,
                            const char   *key)
{
    g_return_val_if_fail (key != NULL && key[0] != '\0', NULL);
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    return ammonite_file_get_metadata_impl (file, key);
}

/**
 * ammonite_file_get_metadata_list:
 * @file: A #AmmoniteFile to get metadata from.
 * @key: A string representation of the metadata key (use macros when possible).
 *
 * Get the value of a metadata attribute which holds a list of strings.
 *
 * Returns: (transfer none): A zero-terminated array of strings.
 */
GStrv
ammonite_file_get_metadata_list (AmmoniteFile *file,
                                 const char   *key)
{
    guint id;

    g_return_val_if_fail (key != NULL, NULL);
    g_return_val_if_fail (key[0] != '\0', NULL);

    if (file == NULL ||
        file->details->metadata == NULL)
    {
        return NULL;
    }

    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    id = ammonite_metadata_get_id (key);
    id |= METADATA_ID_IS_LIST_MASK;

    return g_hash_table_lookup (file->details->metadata, GUINT_TO_POINTER (id));
}

/**
 * ammonite_file_set_metadata:
 * @file: A #AmmoniteFile to set metadata into.
 * @key: A string representation of the metadata key (use macros when possible).
 * @type: A #AmmoniteMetadataType for the following @value
 * @value: (transfer none): Value of type @type to set as metadata
 *
 * If supported by the file, set the value of a metadata attribute.
 */
void
ammonite_file_set_metadata (AmmoniteFile         *file,
                            const char           *key,
                            AmmoniteMetadataType  type,
                            gpointer              value)
{
    g_return_if_fail (AMMONITE_IS_FILE (file));
    g_return_if_fail (key != NULL);
    g_return_if_fail (key[0] != '\0');

    AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file))->set_metadata (file, key, type, value);
}

gboolean
ammonite_file_get_boolean_metadata (AmmoniteFile *file,
                                    const char   *key,
                                    gboolean      default_metadata)
{
    g_return_val_if_fail (key != NULL, default_metadata);
    g_return_val_if_fail (key[0] != '\0', default_metadata);
    g_return_val_if_fail (AMMONITE_IS_FILE (file), default_metadata);

    if (file == NULL)
    {
        return default_metadata;
    }

    const char *value = ammonite_file_get_metadata_impl (file, key);

    if (value == NULL)
    {
        return default_metadata;
    }
    else if (g_ascii_strcasecmp (value, "true") == 0)
    {
        return TRUE;
    }
    else if (g_ascii_strcasecmp (value, "false") == 0)
    {
        return FALSE;
    }
    else
    {
        g_warning ("Invalid boolean metadata value \"%s\" for key \"%s\", "
                   "using default value",
                   value, key);
        return default_metadata;
    }
}

void
ammonite_file_set_boolean_metadata (AmmoniteFile *file,
                                    const char   *key,
                                    gboolean      metadata)
{
    ammonite_file_set_metadata (file, key, AMMONITE_METADATA_STRING,
                                metadata ? "true" : "false");
}

static const char *
ammonite_file_peek_display_name_collation_key (AmmoniteFile *file)
{
    const char *res;

    res = file->details->display_name_collation_key;
    if (res == NULL)
    {
        res = "";
    }

    return res;
}

static const char *
ammonite_file_peek_display_name (AmmoniteFile *file)
{
    const char *name;
    char *escaped_name;

    /* FIXME: for some reason we can get a AmmoniteFile instance which is
     *        no longer valid or could be freed somewhere else in the same time.
     *        There's race condition somewhere. See bug 602500.
     */
    if (G_UNLIKELY (file == NULL || ammonite_file_is_gone (file)))
    {
        return "";
    }

    /* Default to display name based on filename if its not set yet */

    if (G_UNLIKELY (file->details->display_name == NULL))
    {
        name = file->details->name;
        if (g_utf8_validate (name, -1, NULL))
        {
            ammonite_file_set_display_name (file,
                                            name,
                                            NULL,
                                            FALSE);
        }
        else
        {
            escaped_name = g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
            ammonite_file_set_display_name (file,
                                            escaped_name,
                                            NULL,
                                            FALSE);
            g_free (escaped_name);
        }
    }

    return file->details->display_name ?
           file->details->display_name : "";
}

const char *
ammonite_file_get_display_name (AmmoniteFile *file)
{
    return ammonite_file_peek_display_name (file);
}

const char *
ammonite_file_get_edit_name (AmmoniteFile *file)
{
    return file->details->edit_name != NULL ?
           file->details->edit_name :
           "";
}

void
ammonite_file_monitor_add (AmmoniteFile       *file,
                           gconstpointer       client,
                           AmmoniteAttributes  attributes)
{
    g_return_if_fail (AMMONITE_IS_FILE (file));
    g_return_if_fail (client != NULL);

    AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file))->monitor_add (file, client, attributes);
}

void
ammonite_file_monitor_remove (AmmoniteFile  *file,
                              gconstpointer  client)
{
    g_return_if_fail (AMMONITE_IS_FILE (file));
    g_return_if_fail (client != NULL);

    AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file))->monitor_remove (file, client);
}

gboolean
ammonite_file_has_activation_uri (AmmoniteFile *file)
{
    return file->details->activation_uri != NULL;
}

GFile *
ammonite_file_get_activation_location (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    if (file->details->activation_uri != NULL)
    {
        return g_file_new_for_uri (file->details->activation_uri);
    }

    return ammonite_file_get_location (file);
}

static gboolean
is_uri_relative (const char *uri)
{
    char *scheme;
    gboolean ret;

    scheme = g_uri_parse_scheme (uri);
    ret = (scheme == NULL);
    g_free (scheme);
    return ret;
}

static char *
get_custom_icon_metadata_uri (AmmoniteFile *file)
{
    const char *uri = ammonite_file_get_metadata_impl (file, AMMONITE_METADATA_KEY_CUSTOM_ICON);

    if (uri != NULL &&
        ammonite_file_is_directory (file) &&
        is_uri_relative (uri))
    {
        g_autofree char *dir_uri = ammonite_file_get_uri (file);
        return g_build_filename (dir_uri, uri, NULL);
    }
    else
    {
        return g_strdup (uri);
    }
}

static const char *
get_custom_icon_metadata_name (AmmoniteFile *file)
{
    return ammonite_file_get_metadata_impl (file, AMMONITE_METADATA_KEY_CUSTOM_ICON_NAME);
}

static GIcon *
get_mount_icon (AmmoniteFile *file,
                gboolean      symbolic)
{
    g_autoptr (GMount) mount = ammonite_file_get_mount (file);

    if (mount != NULL)
    {
        return symbolic
               ? g_mount_get_symbolic_icon (mount)
               : g_mount_get_icon (mount);
    }
    else
    {
        g_autoptr (GFile) location = ammonite_file_get_location (file);

        /* Root directory doesn't have a GMount, but for UI purposes we want
         * it to be treated the same way. */
        if (ammonite_is_root_directory (location))
        {
            return symbolic
                   ? g_themed_icon_new_with_default_fallbacks ("drive-harddisk-symbolic")
                   : g_themed_icon_new_with_default_fallbacks ("drive-harddisk");
        }
    }

    return NULL;
}

static gboolean
has_custom_icon (AmmoniteFile *file)
{
    g_autofree gchar *custom_icon_uri = get_custom_icon_metadata_uri (file);

    return custom_icon_uri != NULL ||
           get_custom_icon_metadata_name (file) != NULL;
}

static GIcon *
get_custom_icon (AmmoniteFile *file)
{
    char *custom_icon_uri;
    GFile *icon_file;
    GIcon *icon;

    icon = NULL;

    /* Metadata takes precedence; first we look at the custom
     * icon URI, then at the custom icon name.
     */
    custom_icon_uri = get_custom_icon_metadata_uri (file);

    if (custom_icon_uri)
    {
        icon_file = g_file_new_for_uri (custom_icon_uri);
        icon = g_file_icon_new (icon_file);
        g_object_unref (icon_file);
        g_free (custom_icon_uri);
    }

    if (icon == NULL)
    {
        const char *custom_icon_name = get_custom_icon_metadata_name (file);

        if (custom_icon_name != NULL)
        {
            icon = g_themed_icon_new_with_default_fallbacks (custom_icon_name);
        }
    }

    return icon;
}

static GFilesystemPreviewType
get_filesystem_use_preview (AmmoniteFile *file,
                            AmmoniteFile *parent)
{
    if (file->details->filesystem_info_is_up_to_date)
    {
        return file->details->filesystem_use_preview;
    }
    else if (parent != NULL && parent->details->filesystem_info_is_up_to_date)
    {
        return parent->details->filesystem_use_preview;
    }
    else
    {
        return G_FILESYSTEM_PREVIEW_TYPE_IF_ALWAYS;
    }
}

static gboolean
get_filesystem_remote (AmmoniteFile *file,
                       AmmoniteFile *parent)
{
    if (file->details->filesystem_info_is_up_to_date)
    {
        return file->details->filesystem_remote;
    }

    g_autoptr (AmmoniteFile) tmp_parent = NULL;

    if (parent == NULL)
    {
        tmp_parent = ammonite_file_get_parent (file);
        parent = tmp_parent;
    }

    if (parent != NULL && parent->details->filesystem_info_is_up_to_date)
    {
        if (ammonite_file_is_regular_file (file))
        {
            file->details->filesystem_remote = parent->details->filesystem_remote;
            file->details->filesystem_readonly = parent->details->filesystem_readonly;
            file->details->filesystem_use_preview = parent->details->filesystem_use_preview;
            file->details->filesystem_info_is_up_to_date = TRUE;
        }

        return parent->details->filesystem_remote;
    }

    /* Pretend it's local */
    return FALSE;
}

static gboolean
get_speed_tradeoff_preference_for_file (AmmoniteFile               *file,
                                        AmmoniteSpeedTradeoffValue  value)
{
    if (value == AMMONITE_SPEED_TRADEOFF_NEVER)
    {
        return FALSE;
    }

    g_autoptr (AmmoniteFile) parent = file->details->filesystem_info_is_up_to_date ?
                                      NULL : ammonite_file_get_parent (file);
    GFilesystemPreviewType use_preview = get_filesystem_use_preview (file, parent);

    if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER)
    {
        /* file system says to never preview anything */
        return FALSE;
    }
    else if (value == AMMONITE_SPEED_TRADEOFF_ALWAYS)
    {
        /* we don't care whether it's local or not */
        return TRUE;
    }
    /* value == AMMONITE_SPEED_TRADEOFF_LOCAL_ONLY */
    else if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL)
    {
        /* file system says we should treat file as if it's local */
        return TRUE;
    }
    else
    {
        /* check whether file is not remote */
        return !get_filesystem_remote (file, parent);
    }
}

gboolean
ammonite_file_should_show_thumbnail (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    if (has_custom_icon (file))
    {
        return FALSE;
    }

    const char *mime_type = file->details->mime_type;

    if (mime_type == NULL)
    {
        mime_type = "application/octet-stream";
    }

    /* If the thumbnail has already been created, don't care about the size
     * of the original file.
     */
    if (!ammonite_file_has_thumbnail (file) &&
        ammonite_file_get_size (file) > cached_thumbnail_limit &&
        ammonite_thumbnail_is_mimetype_limited_by_size (mime_type))
    {
        return FALSE;
    }

    if (show_file_thumbs != AMMONITE_SPEED_TRADEOFF_NEVER &&
        file->details->has_preview_icon)
    {
        /* The thumbnail should be generated if the preview icon is available
         * regardless of the filesystem type (i.e. for MTP/GPhoto2 backends).
         */
        return TRUE;
    }

    return get_speed_tradeoff_preference_for_file (file, show_file_thumbs);
}

static GList *
strv_to_glist (GStrv strv)
{
    GList *list = NULL;

    for (guint i = 0; strv != NULL && strv[i] != NULL; i++)
    {
        list = g_list_prepend (list, strv[i]);
    }

    return list;
}

static GList *
get_automatic_emblem_keywords (AmmoniteFile *file)
{
    GList *keywords = NULL;

    if (ammonite_file_is_symbolic_link (file))
    {
        keywords = g_list_prepend (keywords, AMMONITE_FILE_EMBLEM_NAME_SYMBOLIC_LINK);
    }

    if (!ammonite_file_can_read (file))
    {
        keywords = g_list_prepend (keywords, AMMONITE_FILE_EMBLEM_NAME_CANT_READ);
    }
    /* Trash files are assumed to be read-only, so we want to ignore them here. */
    else if (!ammonite_file_can_write (file) && !ammonite_file_is_in_trash (file))
    {
        g_autoptr (AmmoniteFile) parent = ammonite_file_get_parent (file);

        if (parent == NULL || ammonite_file_can_write (parent))
        {
            keywords = g_list_prepend (keywords, AMMONITE_FILE_EMBLEM_NAME_CANT_WRITE);
        }
    }

    return keywords;
}

static GList *
get_extension_emblem_keywords (AmmoniteFile *file)
{
    const GStrv key_emblems = ammonite_file_get_metadata_list (file, AMMONITE_METADATA_KEY_EMBLEMS);
    GList *keywords = g_list_concat (g_list_copy (file->details->extension_emblems),
                                     strv_to_glist (key_emblems));

    if (keywords != NULL)
    {
        /* Sort and deduplicate keywords */
        keywords = g_list_sort (keywords, (GCompareFunc) g_utf8_collate);

        for (GList *l = keywords; l->next != NULL;)
        {
            if (strcmp ((const char *) l->data, (const char *) l->next->data) == 0)
            {
                keywords = g_list_delete_link (keywords, l->next);
            }
            else
            {
                l = l->next;
            }
        }
    }

    return keywords;
}

/**
 * ammonite_file_get_emblem_icons
 *
 * Return the list of names of emblems that this file should display,
 * in canonical order.
 * @file: (not nullable): AmmoniteFile representing the file in question.
 *
 * Returns: (transfer full) (element-type GIcon): A list of emblem names.
 *
 **/
GList *
ammonite_file_get_emblem_icons (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    GList *icons = NULL;
    GIcon *mount_icon = get_mount_icon (file, TRUE);
    g_autoptr (GList) keywords = g_list_concat (get_automatic_emblem_keywords (file),
                                                get_extension_emblem_keywords (file));

    for (GList *l = keywords; l != NULL; l = l->next)
    {
        char *keyword = l->data;
        g_autofree char *prefixed = g_strconcat ("emblem-", keyword, NULL);
        char *icon_names[2] = { prefixed, keyword };

        icons = g_list_prepend (icons, g_themed_icon_new_from_names (icon_names, 2));
    }

    if (mount_icon != NULL)
    {
        icons = g_list_prepend (icons, mount_icon);
    }

    return icons;
}

GIcon *
ammonite_file_get_gicon (AmmoniteFile          *file,
                         AmmoniteFileIconFlags  flags)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    GIcon *icon = get_custom_icon (file);

    if (icon != NULL)
    {
        return icon;
    }
    else if (flags & AMMONITE_FILE_ICON_FLAGS_USE_MOUNT_ICON &&
             (icon = get_mount_icon (file, FALSE)) != NULL)
    {
        return icon;
    }
    else if (file->details->icon != NULL)
    {
        return g_object_ref (file->details->icon);
    }

    return g_object_ref (ammonite_icon_info_get_default_file_icon ());
}

const char *
ammonite_file_get_thumbnail_path (AmmoniteFile *file)
{
    return file->details->thumbnail_path;
}

gboolean
ammonite_file_has_thumbnail (AmmoniteFile *file)
{
    return file->details->thumbnail_is_up_to_date &&
           file->details->thumbnail != NULL;
}

static GdkPaintable *
ammonite_file_get_thumbnail_icon (AmmoniteFile          *file,
                                  int                    size,
                                  int                    scale,
                                  AmmoniteFileIconFlags  flags)
{
    GdkPaintable *paintable = NULL;

    if (ammonite_file_has_thumbnail (file))
    {
        GdkTexture *texture = file->details->thumbnail;
        double width = gdk_texture_get_width (texture) / scale;
        double height = gdk_texture_get_height (texture) / scale;
        g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();
        GskRoundedRect rounded_rect;

        if (MAX (width, height) != size)
        {
            double scale_factor = size / MAX (width, height);

            width = round (width * scale_factor);
            height = round (height * scale_factor);
        }

        gsk_rounded_rect_init_from_rect (&rounded_rect,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height),
                                         2 /* radius*/);
        gtk_snapshot_push_rounded_clip (snapshot, &rounded_rect);

        gdk_paintable_snapshot (GDK_PAINTABLE (texture),
                                GDK_SNAPSHOT (snapshot),
                                width, height);

        if (size >= AMMONITE_GRID_ICON_SIZE_SMALL &&
            ammonite_mime_is_video (file->details->mime_type))
        {
            ammonite_ui_frame_video (snapshot, width, height);
        }

        gtk_snapshot_pop (snapshot); /* End rounded clip */

        if (g_getenv ("G_MESSAGES_DEBUG") != NULL)
        {
            g_debug ("Returning thumbnailed image, at size %d %d",
                     (int) (width), (int) (height));
        }

        paintable = gtk_snapshot_to_paintable (snapshot, NULL);
    }

    if (paintable != NULL)
    {
        return paintable;
    }

    if (ammonite_file_check_if_ready (file, AMMONITE_ATTRIBUTE_THUMBNAIL_INFO) &&
        !ammonite_file_check_if_ready (file, AMMONITE_ATTRIBUTE_THUMBNAIL_BUFFER))
    {
        g_autoptr (GIcon) gicon = g_themed_icon_new (ICON_NAME_THUMBNAIL_LOADING);

        paintable = ammonite_icon_info_lookup (gicon, size, scale);
    }

    return paintable;
}

GdkPaintable *
ammonite_file_get_icon_paintable (AmmoniteFile          *file,
                                  int                    size,
                                  int                    scale,
                                  AmmoniteFileIconFlags  flags)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    g_autoptr (GIcon) custom_gicon = get_custom_icon (file);

    if (custom_gicon != NULL)
    {
        return ammonite_icon_info_lookup (custom_gicon, size, scale);
    }

    GdkPaintable *paintable = NULL;

    if (g_getenv ("G_MESSAGES_DEBUG") != NULL)
    {
        g_debug ("Called file_get_icon(), at size %d", size);
    }

    if (flags & AMMONITE_FILE_ICON_FLAGS_USE_THUMBNAILS &&
        size >= AMMONITE_THUMBNAIL_MINIMUM_ICON_SIZE &&
        ammonite_file_should_show_thumbnail (file))
    {
        paintable = ammonite_file_get_thumbnail_icon (file, size, scale, flags);
    }

    if (paintable == NULL)
    {
        g_autoptr (GIcon) gicon = ammonite_file_get_gicon (file, flags);

        paintable = ammonite_icon_info_lookup (gicon, size, scale);
    }

    return paintable;
}

GDateTime *
ammonite_file_get_date (AmmoniteFile     *file,
                        AmmoniteDateType  date_type)
{
    if (file == NULL)
    {
        return NULL;
    }

    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    time_t file_time_raw = 0;

    switch (date_type)
    {
        case AMMONITE_DATE_TYPE_ACCESSED:
        {
            file_time_raw = ammonite_file_get_atime (file);
        }
        break;

        case AMMONITE_DATE_TYPE_MODIFIED:
        {
            file_time_raw = ammonite_file_get_mtime (file);
        }
        break;

        case AMMONITE_DATE_TYPE_CREATED:
        {
            file_time_raw = ammonite_file_get_btime (file);
        }
        break;

        case AMMONITE_DATE_TYPE_TRASHED:
        {
            file_time_raw = ammonite_file_get_trash_time (file);
        }
        break;

        case AMMONITE_DATE_TYPE_RECENCY:
        {
            file_time_raw = ammonite_file_get_recency (file);
        }
        break;

        default:
        {
            g_return_val_if_reached (NULL);
        }
    }

    /* Before we have info on a file, the date is unknown. */
    if (file_time_raw == 0)
    {
        return NULL;
    }

    return g_date_time_new_from_unix_local (file_time_raw);
}

static char *
ammonite_file_get_where_string (AmmoniteFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }

    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    g_autoptr (AmmoniteFile) real_file = NULL;

    if (ammonite_file_is_in_recent (file))
    {
        g_autoptr (GFile) activation_location = ammonite_file_get_activation_location (file);

        real_file = ammonite_file_get (activation_location);
    }
    else
    {
        real_file = g_object_ref (file);
    }

    return ammonite_file_get_parent_uri_for_display (real_file);
}

static char *
ammonite_file_get_trash_original_file_parent_as_string (AmmoniteFile *file)
{
    AmmoniteFile *orig_file;
    char *filename;

    filename = NULL;
    orig_file = ammonite_file_get_trash_original_file (file);
    if (orig_file != NULL)
    {
        filename = ammonite_file_get_parent_uri_for_display (orig_file);

        ammonite_file_unref (orig_file);
    }

    return filename;
}

/**
 * ammonite_file_get_date_as_string:
 *
 * Get a user-displayable string representing a file modification date.
 * The caller is responsible for g_free-ing this string.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_date_as_string (AmmoniteFile       *file,
                                  AmmoniteDateType    date_type,
                                  AmmoniteDateFormat  date_format)
{
    g_autoptr (GDateTime) file_date_time = ammonite_file_get_date (file, date_type);

    if (file_date_time == NULL)
    {
        return NULL;
    }

    return ammonite_date_to_str (file_date_time,
                                 date_format == AMMONITE_DATE_FORMAT_REGULAR);
}

gboolean
ammonite_file_should_show_directory_item_count (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    /* Don't count items in autofs directories to avoid triggering automount. */
    if (file->details->is_mountpoint)
    {
        g_autoptr (GFile) location = ammonite_file_get_location (file);

        if (ammonite_location_is_autofs_mountpoint (location))
        {
            return FALSE;
        }
    }

    return get_speed_tradeoff_preference_for_file (file, show_directory_item_count);
}

/**
 * ammonite_file_get_directory_item_count
 *
 * Get the number of items in a directory.
 * @file: AmmoniteFile representing a directory.
 * @count: Place to put count.
 * @count_unreadable: Set to TRUE (if non-NULL) if permissions prevent
 * the item count from being read on this directory. Otherwise set to FALSE.
 *
 * Returns: TRUE if count is available.
 *
 **/
gboolean
ammonite_file_get_directory_item_count (AmmoniteFile *file,
                                        guint        *count,
                                        gboolean     *count_unreadable)
{
    if (count != NULL)
    {
        *count = 0;
    }
    if (count_unreadable != NULL)
    {
        *count_unreadable = FALSE;
    }

    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    if (!ammonite_file_is_directory (file))
    {
        return FALSE;
    }

    if (!ammonite_file_should_show_directory_item_count (file))
    {
        return FALSE;
    }

    return AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_item_count
               (file, count, count_unreadable);
}

/**
 * ammonite_file_get_deep_counts
 *
 * Get the statistics about items inside a directory.
 * @file: AmmoniteFile representing a directory or file.
 * @directory_count: Place to put count of directories inside.
 * @files_count: Place to put count of files inside.
 * @unreadable_directory_count: Number of directories encountered
 * that were unreadable.
 * @total_size: Total size of all files and directories visited.
 * @force: Whether the deep counts should even be collected if
 * ammonite_file_should_show_directory_item_count returns FALSE
 * for this file.
 *
 * Returns: Status to indicate whether sizes are available.
 *
 **/
AmmoniteRequestStatus
ammonite_file_get_deep_counts (AmmoniteFile *file,
                               guint        *directory_count,
                               guint        *file_count,
                               guint        *unreadable_directory_count,
                               goffset      *total_size,
                               gboolean      force)
{
    if (directory_count != NULL)
    {
        *directory_count = 0;
    }
    if (file_count != NULL)
    {
        *file_count = 0;
    }
    if (unreadable_directory_count != NULL)
    {
        *unreadable_directory_count = 0;
    }
    if (total_size != NULL)
    {
        *total_size = 0;
    }

    g_return_val_if_fail (AMMONITE_IS_FILE (file), AMMONITE_REQUEST_DONE);

    if (!force && !ammonite_file_should_show_directory_item_count (file))
    {
        /* Set field so an existing value isn't treated as up-to-date
         * when preference changes later.
         */
        file->details->deep_counts_status = AMMONITE_REQUEST_NOT_STARTED;
        return file->details->deep_counts_status;
    }

    return AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_deep_counts
               (file, directory_count, file_count,
               unreadable_directory_count, total_size);
}

void
ammonite_file_recompute_deep_counts (AmmoniteFile *file)
{
    if (file->details->deep_counts_status != AMMONITE_REQUEST_IN_PROGRESS)
    {
        file->details->deep_counts_status = AMMONITE_REQUEST_NOT_STARTED;
        if (file->details->directory != NULL)
        {
            ammonite_directory_add_file_to_work_queue (file->details->directory, file);
            ammonite_directory_async_state_changed (file->details->directory);
        }
    }
}

gboolean
ammonite_file_can_get_size (AmmoniteFile *file)
{
    return file->details->size != -1;
}


/**
 * ammonite_file_get_size
 *
 * Get the file size.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Size in bytes.
 *
 **/
guint64
ammonite_file_get_size (AmmoniteFile *file)
{
    /* Before we have info on the file, we don't know the size. */
    if (file->details->size == -1)
    {
        return 0;
    }
    return file->details->size;
}

time_t
ammonite_file_get_mtime (AmmoniteFile *file)
{
    return file->details->mtime;
}

time_t
ammonite_file_get_atime (AmmoniteFile *file)
{
    return file->details->atime;
}

time_t
ammonite_file_get_btime (AmmoniteFile *file)
{
    return file->details->btime;
}

time_t
ammonite_file_get_recency (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), 0);

    return file->details->recency;
}

time_t
ammonite_file_get_trash_time (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), 0);

    return file->details->trash_time;
}

static void
set_attributes_get_info_callback (GObject      *source_object,
                                  GAsyncResult *res,
                                  gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    GFileInfo *new_info;
    GError *error;

    op = callback_data;

    error = NULL;
    new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
    if (new_info != NULL)
    {
        if (ammonite_file_update_info (op->file, new_info))
        {
            ammonite_file_changed (op->file);
        }
        g_object_unref (new_info);
    }
    ammonite_file_operation_complete (op, NULL, error);
    if (error)
    {
        g_error_free (error);
    }
}


static void
set_attributes_callback (GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    GError *error;
    gboolean res;

    op = callback_data;

    error = NULL;
    res = g_file_set_attributes_finish (G_FILE (source_object),
                                        result,
                                        NULL,
                                        &error);

    if (res)
    {
        g_file_query_info_async (G_FILE (source_object),
                                 AMMONITE_FILE_DEFAULT_ATTRIBUTES,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 op->cancellable,
                                 set_attributes_get_info_callback, op);
    }
    else
    {
        ammonite_file_operation_complete (op, NULL, error);
        g_error_free (error);
    }
}

void
ammonite_file_set_attributes (AmmoniteFile                  *file,
                              GFileInfo                     *attributes,
                              AmmoniteFileOperationCallback  callback,
                              gpointer                       callback_data)
{
    AmmoniteFileOperation *op;
    GFile *location;

    op = ammonite_file_operation_new (file, callback, callback_data);

    location = ammonite_file_get_location (file);
    g_file_set_attributes_async (location,
                                 attributes,
                                 0,
                                 G_PRIORITY_DEFAULT,
                                 op->cancellable,
                                 set_attributes_callback,
                                 op);
    g_object_unref (location);
}

void
ammonite_file_set_search_relevance (AmmoniteFile *file,
                                    gdouble       relevance)
{
    file->details->search_relevance = relevance;
}

void
ammonite_file_set_search_fts_snippet (AmmoniteFile *file,
                                      const gchar  *fts_snippet)
{
    g_set_str (&file->details->fts_snippet, fts_snippet);
}

const gchar *
ammonite_file_get_search_fts_snippet (AmmoniteFile *file)
{
    return file->details->fts_snippet;
}

/**
 * ammonite_file_can_get_permissions:
 *
 * Check whether the permissions for a file are determinable.
 * This might not be the case for files on non-UNIX file systems.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the permissions are valid.
 */
gboolean
ammonite_file_can_get_permissions (AmmoniteFile *file)
{
    return file->details->has_permissions;
}

/**
 * ammonite_file_can_set_permissions:
 *
 * Check whether the current user is allowed to change
 * the permissions of a file.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the current user can change the
 * permissions of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
ammonite_file_can_set_permissions (AmmoniteFile *file)
{
    g_autoptr (GFile) location = ammonite_file_get_location (file);

    if (file->details->has_uid &&
        g_file_is_native (location))
    {
        /* Owner is allowed to set permissions. */
        if (geteuid () == file->details->uid)
        {
            return TRUE;
        }

        /* Administrators are also allowed to set permissions. */
        if (ammonite_file_is_in_admin (file))
        {
            return TRUE;
        }

        /* Nobody else is allowed. */
        return FALSE;
    }

    /* pretend to have full chmod rights when no info is available, relevant when
     * the FS can't provide ownership info, for instance for FTP */
    return TRUE;
}

guint
ammonite_file_get_permissions (AmmoniteFile *file)
{
    g_return_val_if_fail (ammonite_file_can_get_permissions (file), 0);

    return file->details->permissions;
}

/**
 * ammonite_file_set_permissions:
 *
 * Change a file's permissions. This should only be called if
 * ammonite_file_can_set_permissions returned TRUE.
 *
 * @file: AmmoniteFile representing the file in question.
 * @new_permissions: New permissions value. This is the whole
 * set of permissions, not a delta.
 **/
void
ammonite_file_set_permissions (AmmoniteFile                  *file,
                               guint32                        new_permissions,
                               AmmoniteFileOperationCallback  callback,
                               gpointer                       callback_data)
{
    GFileInfo *info;
    GError *error;

    if (!ammonite_file_can_set_permissions (file))
    {
        /* Claim that something changed even if the permission change failed.
         * This makes it easier for some clients who see the "reverting"
         * to the old permissions as "changing back".
         */
        ammonite_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                             _("Not allowed to set permissions"));
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    /* Test the permissions-haven't-changed case explicitly
     * because we don't want to send the file-changed signal if
     * nothing changed.
     */
    if (new_permissions == file->details->permissions)
    {
        (*callback)(file, NULL, NULL, callback_data);
        return;
    }

    if (!ammonite_file_undo_manager_is_operating ())
    {
        AmmoniteFileUndoInfo *undo_info;
        g_autoptr (GFile) location = ammonite_file_get_location (file);

        undo_info = ammonite_file_undo_info_permissions_new (location,
                                                             file->details->permissions,
                                                             new_permissions);
        ammonite_file_undo_manager_set_action (undo_info);

        g_object_unref (undo_info);
    }

    info = g_file_info_new ();
    g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE, new_permissions);
    ammonite_file_set_attributes (file, info, callback, callback_data);

    g_object_unref (info);
}

/**
 * ammonite_file_can_get_selinux_context:
 *
 * Check whether the selinux context for a file are determinable.
 * This might not be the case for files on non-UNIX file systems,
 * files without a context or systems that don't support selinux.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the permissions are valid.
 */
gboolean
ammonite_file_can_get_selinux_context (AmmoniteFile *file)
{
    return file->details->selinux_context != NULL;
}


/**
 * ammonite_file_get_selinux_context:
 *
 * Get a user-displayable string representing a file's selinux
 * context
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
char *
ammonite_file_get_selinux_context (AmmoniteFile *file)
{
    char *translated;
    char *raw;

    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    if (!ammonite_file_can_get_selinux_context (file))
    {
        return NULL;
    }

    raw = file->details->selinux_context;

#ifdef HAVE_SELINUX
    if (selinux_raw_to_trans_context (raw, &translated) == 0)
    {
        char *tmp;
        tmp = g_strdup (translated);
        freecon (translated);
        translated = tmp;
    }
    else
#endif
    {
        translated = g_strdup (raw);
    }

    return translated;
}

static char *
get_real_name (const char *name,
               const char *gecos)
{
    char *part_before_comma, *capitalized_login_name, *real_name;
    const char *locale_string;
    g_auto (GStrv) geco_parts = NULL;

    if (gecos == NULL || gecos[0] == '\0')
    {
        return NULL;
    }

    geco_parts = g_strsplit (gecos, ",", 2);
    if (geco_parts == NULL)
    {
        return NULL;
    }

    locale_string = geco_parts[0];
    if (!g_utf8_validate (locale_string, -1, NULL))
    {
        part_before_comma = g_locale_to_utf8 (locale_string, -1, NULL, NULL, NULL);
    }
    else
    {
        part_before_comma = g_strdup (locale_string);
    }

    if (!g_utf8_validate (name, -1, NULL))
    {
        g_autofree gchar *login_name = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
        capitalized_login_name = ammonite_capitalize_str (login_name);
    }
    else
    {
        capitalized_login_name = ammonite_capitalize_str (name);
    }

    if (capitalized_login_name == NULL)
    {
        real_name = part_before_comma;
    }
    else
    {
        GString *real_name_str = g_string_new_take (g_steal_pointer (&part_before_comma));
        g_string_replace (real_name_str, "&", capitalized_login_name, 0);
        real_name = g_string_free_and_steal (real_name_str);
    }


    if (g_strcmp0 (real_name, NULL) == 0
        || g_strcmp0 (name, real_name) == 0
        || g_strcmp0 (capitalized_login_name, real_name) == 0)
    {
        g_free (real_name);
        real_name = NULL;
    }

    g_free (capitalized_login_name);

    return real_name;
}

static gboolean
get_group_id_from_group_name (const char *group_name,
                              uid_t      *gid)
{
    struct group *group;

    g_assert (gid != NULL);

    group = getgrnam (group_name);

    if (group == NULL)
    {
        return FALSE;
    }

    *gid = group->gr_gid;

    return TRUE;
}

static gboolean
get_ids_from_user_name (const char *user_name,
                        uid_t      *uid,
                        uid_t      *gid)
{
    struct passwd *password_info;

    g_assert (uid != NULL || gid != NULL);

    password_info = getpwnam (user_name);

    if (password_info == NULL)
    {
        return FALSE;
    }

    if (uid != NULL)
    {
        *uid = password_info->pw_uid;
    }

    if (gid != NULL)
    {
        *gid = password_info->pw_gid;
    }

    return TRUE;
}

static gboolean
get_user_id_from_user_name (const char *user_name,
                            uid_t      *id)
{
    return get_ids_from_user_name (user_name, id, NULL);
}

static gboolean
get_id_from_digit_string (const char *digit_string,
                          uid_t      *id)
{
    long scanned_id;
    char c;

    g_assert (id != NULL);

    /* Only accept string if it has one integer with nothing
     * afterwards.
     */
    if (sscanf (digit_string, "%ld%c", &scanned_id, &c) != 1)
    {
        return FALSE;
    }
    *id = scanned_id;
    return TRUE;
}

/**
 * ammonite_file_can_get_owner:
 *
 * Check whether the owner a file is determinable.
 * This might not be the case for files on non-UNIX file systems.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the owner is valid.
 */
gboolean
ammonite_file_can_get_owner (AmmoniteFile *file)
{
    /* Before we have info on a file, the owner is unknown. */
    return file->details->has_uid;
}

/**
 * ammonite_file_get_uid:
 *
 * Get the user id of the file's owner.
 *
 * @file: The file in question.
 *
 * Return value: (transfer none): the user id.
 */
uid_t
ammonite_file_get_uid (AmmoniteFile *file)
{
    return file->details->uid;
}

/**
 * ammonite_file_get_owner_name:
 *
 * Get the user name of the file's owner. If the owner has no
 * name, returns the userid as a string. The caller is responsible
 * for g_free-ing this string.
 *
 * @file: The file in question.
 *
 * Return value: A newly-allocated string.
 */
char *
ammonite_file_get_owner_name (AmmoniteFile *file)
{
    return ammonite_file_get_owner_as_string (file, FALSE);
}

/**
 * ammonite_file_can_set_owner:
 *
 * Check whether the current user is allowed to change
 * the owner of a file.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the current user can change the
 * owner of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
ammonite_file_can_set_owner (AmmoniteFile *file)
{
    /* Not allowed to set the owner if we can't
     * even read it. This can happen on non-UNIX file
     * systems.
     */
    if (!ammonite_file_can_get_owner (file))
    {
        return FALSE;
    }

    /* Owner can be changed by administrators, thanks to PolKit integration. */
    return ammonite_file_is_in_admin (file);
}

/**
 * ammonite_file_set_owner:
 *
 * Set the owner of a file. This will only have any effect if
 * ammonite_file_can_set_owner returns TRUE.
 *
 * @file: The file in question.
 * @user_name_or_id: The user name to set the owner to.
 * If the string does not match any user name, and the
 * string is an integer, the owner will be set to the
 * userid represented by that integer.
 * @callback: Function called when asynch owner change succeeds or fails.
 * @callback_data: Parameter passed back with callback function.
 */
void
ammonite_file_set_owner (AmmoniteFile                  *file,
                         const char                    *user_name_or_id,
                         AmmoniteFileOperationCallback  callback,
                         gpointer                       callback_data)
{
    GError *error;
    GFileInfo *info;
    uid_t new_id;

    if (!ammonite_file_can_set_owner (file))
    {
        /* Claim that something changed even if the permission
         * change failed. This makes it easier for some
         * clients who see the "reverting" to the old owner as
         * "changing back".
         */
        ammonite_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                             _("Not allowed to set owner"));
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    /* If no match treating user_name_or_id as name, try treating
     * it as id.
     */
    if (!get_user_id_from_user_name (user_name_or_id, &new_id)
        && !get_id_from_digit_string (user_name_or_id, &new_id))
    {
        /* Claim that something changed even if the permission
         * change failed. This makes it easier for some
         * clients who see the "reverting" to the old owner as
         * "changing back".
         */
        ammonite_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             _("Specified owner “%s” doesn’t exist"), user_name_or_id);
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    /* Test the owner-hasn't-changed case explicitly because we
     * don't want to send the file-changed signal if nothing
     * changed.
     */
    if (file->details->has_uid && new_id == file->details->uid)
    {
        (*callback)(file, NULL, NULL, callback_data);
        return;
    }

    if (!ammonite_file_undo_manager_is_operating ())
    {
        AmmoniteFileUndoInfo *undo_info;
        g_autoptr (GFile) location = ammonite_file_get_location (file);
        char *current_owner;

        current_owner = ammonite_file_get_owner_as_string (file, FALSE);

        undo_info = ammonite_file_undo_info_ownership_new (AMMONITE_FILE_UNDO_OP_CHANGE_OWNER,
                                                           location,
                                                           current_owner,
                                                           user_name_or_id);
        ammonite_file_undo_manager_set_action (undo_info);

        g_free (current_owner);
    }

    info = g_file_info_new ();
    g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID, new_id);
    ammonite_file_set_attributes (file, info, callback, callback_data);
    g_object_unref (info);
}

/**
 * ammonite_get_user_names:
 *
 * Get a list of user names. For users with a different associated
 * "real name", the real name follows the standard user name, separated
 * by a dash surrounded by spaces. The caller is responsible for freeing
 * this list and its contents.
 */
GList *
ammonite_get_user_names (void)
{
    GList *list;
    char *real_name, *name;
    struct passwd *user;

    list = NULL;

    setpwent ();

    while ((user = getpwent ()) != NULL)
    {
        real_name = get_real_name (user->pw_name, user->pw_gecos);
        if (real_name != NULL && !g_str_equal (real_name, ""))
        {
            name = g_strconcat (user->pw_name, " – ", real_name, NULL);
        }
        else
        {
            name = g_strdup (user->pw_name);
        }
        g_free (real_name);
        list = g_list_prepend (list, name);
    }

    endpwent ();

    return g_list_sort (list, (GCompareFunc) g_utf8_collate);
}

/**
 * ammonite_file_can_get_group:
 *
 * Check whether the group a file is determinable.
 * This might not be the case for files on non-UNIX file systems.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the group is valid.
 */
gboolean
ammonite_file_can_get_group (AmmoniteFile *file)
{
    /* Before we have info on a file, the group is unknown. */
    return file->details->has_gid;
}

/**
 * ammonite_file_get_gid:
 *
 * Get the group id of the file's group.
 *
 * @file: The file in question.
 *
 * Return value: (transfer none): the group id.
 */
gid_t
ammonite_file_get_gid (AmmoniteFile *file)
{
    return file->details->gid;
}

/**
 * ammonite_file_get_group_name:
 *
 * Get the name of the file's group. If the group has no
 * name, returns the groupid as a string.
 *
 * @file: The file in question.
 *
 * Returns: (transfer none): The group name for the file.
 **/
const char *
ammonite_file_get_group_name (AmmoniteFile *file)
{
    return file->details->group;
}

/**
 * ammonite_file_can_set_group:
 *
 * Check whether the current user is allowed to change
 * the group of a file.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the current user can change the
 * group of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
ammonite_file_can_set_group (AmmoniteFile *file)
{
    /* Not allowed to set the permissions if we can't
     * even read them. This can happen on non-UNIX file
     * systems.
     */
    if (!ammonite_file_can_get_group (file))
    {
        return FALSE;
    }

    /* Owner is allowed to set group (with restrictions). */
    if (file->details->has_uid && geteuid () == file->details->uid)
    {
        return TRUE;
    }

    /* Groups can be changed by administrators, thanks to PolKit integration. */
    if (ammonite_file_is_in_admin (file))
    {
        return TRUE;
    }

    /* Nobody else is allowed. */
    return FALSE;
}

static GList *
ammonite_get_group_names_for_user (void)
{
    GList *list;
    struct group *group;
    int count, i;
    gid_t gid_list[NGROUPS_MAX + 1];


    list = NULL;

    count = getgroups (NGROUPS_MAX + 1, gid_list);
    for (i = 0; i < count; i++)
    {
        group = getgrgid (gid_list[i]);
        if (group == NULL)
        {
            break;
        }

        list = g_list_prepend (list, g_strdup (group->gr_name));
    }

    return g_list_sort (list, (GCompareFunc) g_utf8_collate);
}

/**
 * ammonite_get_group_names:
 *
 * Get a list of all group names.
 */
GList *
ammonite_get_all_group_names (void)
{
    GList *list;
    struct group *group;

    list = NULL;

    setgrent ();

    while ((group = getgrent ()) != NULL)
    {
        list = g_list_prepend (list, g_strdup (group->gr_name));
    }

    endgrent ();

    return g_list_sort (list, (GCompareFunc) g_utf8_collate);
}

/**
 * ammonite_file_get_settable_group_names:
 *
 * Get a list of all group names that the current user
 * can set the group of a specific file to.
 *
 * @file: The AmmoniteFile in question.
 */
GList *
ammonite_file_get_settable_group_names (AmmoniteFile *file)
{
    GList *result = NULL;

    if (!ammonite_file_can_set_group (file))
    {
        return NULL;
    }

    if (ammonite_file_is_in_admin (file))
    {
        /* Administrators are allowed to set group to anything. */
        result = ammonite_get_all_group_names ();
    }
    else if (file->details->has_uid && geteuid () == file->details->uid)
    {
        /* Owner is allowed to set group to any that owner is member of. */
        result = ammonite_get_group_names_for_user ();
    }
    else
    {
        g_warning ("unhandled case in ammonite_get_settable_group_names");
    }

    return result;
}

/**
 * ammonite_file_set_group:
 *
 * Set the group of a file. This will only have any effect if
 * ammonite_file_can_set_group returns TRUE.
 *
 * @file: The file in question.
 * @group_name_or_id: The group name to set the owner to.
 * If the string does not match any group name, and the
 * string is an integer, the group will be set to the
 * group id represented by that integer.
 * @callback: Function called when asynch group change succeeds or fails.
 * @callback_data: Parameter passed back with callback function.
 */
void
ammonite_file_set_group (AmmoniteFile                  *file,
                         const char                    *group_name_or_id,
                         AmmoniteFileOperationCallback  callback,
                         gpointer                       callback_data)
{
    GError *error;
    GFileInfo *info;
    uid_t new_id;

    if (!ammonite_file_can_set_group (file))
    {
        /* Claim that something changed even if the group
         * change failed. This makes it easier for some
         * clients who see the "reverting" to the old group as
         * "changing back".
         */
        ammonite_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                             _("Not allowed to set group"));
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    /* If no match treating group_name_or_id as name, try treating
     * it as id.
     */
    if (!get_group_id_from_group_name (group_name_or_id, &new_id)
        && !get_id_from_digit_string (group_name_or_id, &new_id))
    {
        /* Claim that something changed even if the group
         * change failed. This makes it easier for some
         * clients who see the "reverting" to the old group as
         * "changing back".
         */
        ammonite_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             _("Specified group “%s” doesn’t exist"), group_name_or_id);
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    if (file->details->has_gid && new_id == file->details->gid)
    {
        (*callback)(file, NULL, NULL, callback_data);
        return;
    }

    if (!ammonite_file_undo_manager_is_operating ())
    {
        AmmoniteFileUndoInfo *undo_info;
        g_autoptr (GFile) location = ammonite_file_get_location (file);
        const char *current_group = ammonite_file_get_group_name (file);

        undo_info = ammonite_file_undo_info_ownership_new (AMMONITE_FILE_UNDO_OP_CHANGE_GROUP,
                                                           location,
                                                           current_group,
                                                           group_name_or_id);
        ammonite_file_undo_manager_set_action (undo_info);
    }

    info = g_file_info_new ();
    g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID, new_id);
    ammonite_file_set_attributes (file, info, callback, callback_data);
    g_object_unref (info);
}

/**
 * ammonite_file_get_octal_permissions_as_string:
 *
 * Get a user-displayable string representing a file's permissions
 * as an octal number. The caller
 * is responsible for g_free-ing this string.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_octal_permissions_as_string (AmmoniteFile *file)
{
    guint32 permissions;

    g_assert (AMMONITE_IS_FILE (file));

    if (!ammonite_file_can_get_permissions (file))
    {
        return NULL;
    }

    permissions = file->details->permissions;
    return g_strdup_printf ("%03o", permissions);
}

/**
 * ammonite_file_get_permissions_as_string:
 *
 * Get a user-displayable string representing a file's permissions. The caller
 * is responsible for g_free-ing this string.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_permissions_as_string (AmmoniteFile *file)
{
    guint32 permissions;
    gboolean is_directory;
    gboolean is_link;
    gboolean suid, sgid, sticky;

    if (!ammonite_file_can_get_permissions (file))
    {
        return NULL;
    }

    g_assert (AMMONITE_IS_FILE (file));

    permissions = file->details->permissions;
    is_directory = file->details->type == G_FILE_TYPE_DIRECTORY;
    is_link = ammonite_file_is_symbolic_link (file);

    /* We use ls conventions for displaying these three obscure flags */
    suid = permissions & S_ISUID;
    sgid = permissions & S_ISGID;
    sticky = permissions & S_ISVTX;

    return g_strdup_printf ("%c%c%c%c%c%c%c%c%c%c",
                            is_link ? 'l' : is_directory ? 'd' : '-',
                            permissions & S_IRUSR ? 'r' : '-',
                            permissions & S_IWUSR ? 'w' : '-',
                            permissions & S_IXUSR
                            ? (suid ? 's' : 'x')
                            : (suid ? 'S' : '-'),
                            permissions & S_IRGRP ? 'r' : '-',
                            permissions & S_IWGRP ? 'w' : '-',
                            permissions & S_IXGRP
                            ? (sgid ? 's' : 'x')
                            : (sgid ? 'S' : '-'),
                            permissions & S_IROTH ? 'r' : '-',
                            permissions & S_IWOTH ? 'w' : '-',
                            permissions & S_IXOTH
                            ? (sticky ? 't' : 'x')
                            : (sticky ? 'T' : '-'));
}

/**
 * ammonite_file_get_owner_as_string:
 *
 * Get a user-displayable string representing a file's owner. The caller
 * is responsible for g_free-ing this string.
 * @file: AmmoniteFile representing the file in question.
 * @include_real_name: Whether or not to append the real name (if any)
 * for this user after the user name.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_owner_as_string (AmmoniteFile *file,
                                   gboolean      include_real_name)
{
    char *user_name;

    /* Before we have info on a file, the owner is unknown. */
    if (file->details->owner == NULL &&
        file->details->owner_real == NULL)
    {
        return NULL;
    }

    if (include_real_name &&
        file->details->has_uid && file->details->uid == getuid ())
    {
        /* Translators: This is a username followed by "(You)" to indicate the file is owned by the current user */
        user_name = g_strdup_printf (_("%s (You)"), file->details->owner);
    }
    else if (file->details->owner_real == NULL || *file->details->owner_real == '\0')
    {
        user_name = g_strdup (file->details->owner);
    }
    else if (file->details->owner == NULL || *file->details->owner == '\0')
    {
        user_name = g_strdup (file->details->owner_real);
    }
    else if (include_real_name &&
             strcmp (file->details->owner, file->details->owner_real) != 0)
    {
        user_name = g_strdup (file->details->owner_real);
    }
    else
    {
        user_name = g_strdup (file->details->owner);
    }

    return user_name;
}

static char *
format_item_count_for_display (guint    item_count,
                               gboolean includes_directories,
                               gboolean includes_files)
{
    g_assert (includes_directories || includes_files);

    return g_strdup_printf (includes_directories
                            ? (includes_files
                               ? ngettext ("%'u item", "%'u items", item_count)
                               : ngettext ("%'u folder", "%'u folders", item_count))
                            : ngettext ("%'u file", "%'u files", item_count), item_count);
}

/**
 * ammonite_file_get_size_as_string:
 *
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string. The string is an item
 * count for directories.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_size_as_string (AmmoniteFile *file)
{
    guint item_count;
    gboolean count_unreadable;

    if (file == NULL)
    {
        return NULL;
    }

    g_assert (AMMONITE_IS_FILE (file));

    if (ammonite_file_is_directory (file))
    {
        if (!ammonite_file_get_directory_item_count (file, &item_count, &count_unreadable))
        {
            return NULL;
        }
        return format_item_count_for_display (item_count, TRUE, TRUE);
    }

    if (file->details->size == -1)
    {
        return NULL;
    }
    return g_format_size (file->details->size);
}

/**
 * ammonite_file_get_size_as_string_with_real_size:
 *
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string. The string is an item
 * count for directories.
 * This function adds the real size in the string.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_size_as_string_with_real_size (AmmoniteFile *file)
{
    guint item_count;
    gboolean count_unreadable;
    g_autofree char *size_str = NULL;

    if (file == NULL)
    {
        return NULL;
    }

    g_assert (AMMONITE_IS_FILE (file));

    if (ammonite_file_is_directory (file))
    {
        if (!ammonite_file_get_directory_item_count (file, &item_count, &count_unreadable))
        {
            return NULL;
        }
        return format_item_count_for_display (item_count, TRUE, TRUE);
    }

    if (file->details->size == -1)
    {
        return NULL;
    }

    size_str = g_strdup_printf ("%'" G_GOFFSET_FORMAT, file->details->size);
    return g_strdup_printf (ngettext ("%s byte",
                                      "%s bytes",
                                      file->details->size),
                            size_str);
}


static char *
ammonite_file_get_deep_count_as_string_internal (AmmoniteFile *file,
                                                 gboolean      report_size,
                                                 gboolean      report_directory_count,
                                                 gboolean      report_file_count)
{
    AmmoniteRequestStatus status;
    guint directory_count;
    guint file_count;
    guint unreadable_count;
    guint total_count;
    goffset total_size;

    /* Must ask for size or some kind of count, but not both. */
    g_assert (!report_size || (!report_directory_count && !report_file_count));
    g_assert (report_size || report_directory_count || report_file_count);

    if (file == NULL)
    {
        return NULL;
    }

    g_assert (AMMONITE_IS_FILE (file));
    g_assert (ammonite_file_is_directory (file));

    status = ammonite_file_get_deep_counts
                 (file, &directory_count, &file_count, &unreadable_count, &total_size, FALSE);

    /* Check whether any info is available. */
    if (status == AMMONITE_REQUEST_NOT_STARTED)
    {
        return NULL;
    }

    total_count = file_count + directory_count;

    if (total_count == 0)
    {
        switch (status)
        {
            case AMMONITE_REQUEST_IN_PROGRESS:
            {
                /* Don't return confident "zero" until we're finished looking,
                 * because of next case.
                 */
                return NULL;
            }

            case AMMONITE_REQUEST_DONE:
            {
                /* Don't return "zero" if we there were contents but we couldn't read them. */
                if (unreadable_count != 0)
                {
                    return NULL;
                }
            }

            default:
            {}
            break;
        }
    }

    /* Note that we don't distinguish the "everything was readable" case
     * from the "some things but not everything was readable" case here.
     * Callers can distinguish them using ammonite_file_get_deep_counts
     * directly if desired.
     */
    if (report_size)
    {
        return g_format_size (total_size);
    }

    return format_item_count_for_display (report_directory_count
                                          ? (report_file_count ? total_count : directory_count)
                                          : file_count,
                                          report_directory_count, report_file_count);
}

/**
 * ammonite_file_get_deep_size_as_string:
 *
 * Get a user-displayable string representing the size of all contained
 * items (only makes sense for directories). The caller
 * is responsible for g_free-ing this string.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_deep_size_as_string (AmmoniteFile *file)
{
    return ammonite_file_get_deep_count_as_string_internal (file, TRUE, FALSE, FALSE);
}

/**
 * ammonite_file_get_deep_total_count_as_string:
 *
 * Get a user-displayable string representing the count of all contained
 * items (only makes sense for directories). The caller
 * is responsible for g_free-ing this string.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_deep_total_count_as_string (AmmoniteFile *file)
{
    return ammonite_file_get_deep_count_as_string_internal (file, FALSE, TRUE, TRUE);
}

/**
 * ammonite_file_get_deep_file_count_as_string:
 *
 * Get a user-displayable string representing the count of all contained
 * items, not including directories. It only makes sense to call this
 * function on a directory. The caller
 * is responsible for g_free-ing this string.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_deep_file_count_as_string (AmmoniteFile *file)
{
    return ammonite_file_get_deep_count_as_string_internal (file, FALSE, FALSE, TRUE);
}

/**
 * ammonite_file_get_deep_directory_count_as_string:
 *
 * Get a user-displayable string representing the count of all contained
 * directories. It only makes sense to call this
 * function on a directory. The caller
 * is responsible for g_free-ing this string.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
ammonite_file_get_deep_directory_count_as_string (AmmoniteFile *file)
{
    return ammonite_file_get_deep_count_as_string_internal (file, FALSE, TRUE, FALSE);
}

/**
 * ammonite_file_get_string_attribute:
 *
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string. If the value is unknown, returns NULL. You can call
 * ammonite_file_get_string_attribute_with_default if you want a non-NULL
 * default.
 *
 * @file: AmmoniteFile representing the file in question.
 * @attribute_name: The name of the desired attribute. The currently supported
 * set includes "name", "type", "detailed_type", "mime_type", "size", "deep_size", "deep_directory_count",
 * "deep_file_count", "deep_total_count", "date_modified", "date_accessed", "date_created",
 * "date_modified_full", "date_accessed_full", "date_created_full",
 * "owner", "group", "permissions", "octal_permissions", "uri", "where",
 * "link_target", "volume", "free_space", "selinux_context", "trashed_on", "trashed_on_full", "trashed_orig_path",
 * "recency"
 *
 * Returns: Newly allocated string ready to display to the user, or NULL
 * if the value is unknown or @attribute_name is not supported.
 *
 **/
char *
ammonite_file_get_string_attribute_q (AmmoniteFile *file,
                                      GQuark        attribute_q)
{
    if (attribute_q == attribute_name_q)
    {
        return g_strdup (ammonite_file_get_display_name (file));
    }
    if (attribute_q == attribute_type_q)
    {
        return ammonite_file_get_type_as_string (file);
    }
    if (attribute_q == attribute_detailed_type_q)
    {
        return ammonite_file_get_detailed_type_as_string (file);
    }
    if (attribute_q == attribute_mime_type_q)
    {
        return g_strdup (ammonite_file_get_mime_type (file));
    }
    if (attribute_q == attribute_size_q)
    {
        return ammonite_file_get_size_as_string (file);
    }
    if (attribute_q == attribute_size_detail_q)
    {
        return ammonite_file_get_size_as_string_with_real_size (file);
    }
    if (attribute_q == attribute_deep_size_q)
    {
        return ammonite_file_get_deep_size_as_string (file);
    }
    if (attribute_q == attribute_deep_file_count_q)
    {
        return ammonite_file_get_deep_file_count_as_string (file);
    }
    if (attribute_q == attribute_deep_directory_count_q)
    {
        return ammonite_file_get_deep_directory_count_as_string (file);
    }
    if (attribute_q == attribute_deep_total_count_q)
    {
        return ammonite_file_get_deep_total_count_as_string (file);
    }
    if (attribute_q == attribute_trash_orig_path_q)
    {
        return ammonite_file_get_trash_original_file_parent_as_string (file);
    }
    if (attribute_q == attribute_date_modified_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_MODIFIED,
                                                 AMMONITE_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_date_modified_full_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_MODIFIED,
                                                 AMMONITE_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_date_accessed_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_ACCESSED,
                                                 AMMONITE_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_date_accessed_full_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_ACCESSED,
                                                 AMMONITE_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_date_created_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_CREATED,
                                                 AMMONITE_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_date_created_full_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_CREATED,
                                                 AMMONITE_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_trashed_on_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_TRASHED,
                                                 AMMONITE_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_trashed_on_full_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_TRASHED,
                                                 AMMONITE_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_recency_q)
    {
        return ammonite_file_get_date_as_string (file,
                                                 AMMONITE_DATE_TYPE_RECENCY,
                                                 AMMONITE_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_permissions_q)
    {
        return ammonite_file_get_permissions_as_string (file);
    }
    if (attribute_q == attribute_selinux_context_q)
    {
        return ammonite_file_get_selinux_context (file);
    }
    if (attribute_q == attribute_octal_permissions_q)
    {
        return ammonite_file_get_octal_permissions_as_string (file);
    }
    if (attribute_q == attribute_owner_q)
    {
        return ammonite_file_get_owner_as_string (file, TRUE);
    }
    if (attribute_q == attribute_group_q)
    {
        return g_strdup (ammonite_file_get_group_name (file));
    }
    if (attribute_q == attribute_uri_q)
    {
        return ammonite_file_get_uri (file);
    }
    if (attribute_q == attribute_where_q)
    {
        return ammonite_file_get_where_string (file);
    }
    if (attribute_q == attribute_link_target_q)
    {
        return g_strdup (ammonite_file_get_symbolic_link_target_path (file));
    }
    if (attribute_q == attribute_volume_q)
    {
        return ammonite_file_get_volume_name (file);
    }
    if (attribute_q == attribute_free_space_q)
    {
        return ammonite_file_get_volume_free_space (file);
    }
    if (file->details->extension_attributes != NULL)
    {
        return g_strdup (g_hash_table_lookup (file->details->extension_attributes,
                                              GINT_TO_POINTER (attribute_q)));
    }

    return NULL;
}

char *
ammonite_file_get_string_attribute (AmmoniteFile *file,
                                    const char   *attribute_name)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    return ammonite_file_get_string_attribute_q (file, g_quark_from_string (attribute_name));
}


/**
 * ammonite_file_get_string_attribute_with_default:
 *
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string. If the value is unknown, returns a string representing
 * the unknown value, which varies with attribute. You can call
 * ammonite_file_get_string_attribute if you want NULL instead of a default
 * result.
 *
 * @file: AmmoniteFile representing the file in question.
 * @attribute_name: The name of the desired attribute. See the description of
 * ammonite_file_get_string for the set of available attributes.
 *
 * Returns: Newly allocated string ready to display to the user, or a string
 * such as "unknown" if the value is unknown or @attribute_name is not supported.
 *
 **/
char *
ammonite_file_get_string_attribute_with_default_q (AmmoniteFile *file,
                                                   GQuark        attribute_q)
{
    char *result;
    guint item_count;
    gboolean count_unreadable;
    AmmoniteRequestStatus status;

    result = ammonite_file_get_string_attribute_q (file, attribute_q);
    if (result != NULL)
    {
        return result;
    }

    /* Supply default values for the ones we know about. */
    /* FIXME bugzilla.gnome.org 40646:
     * Use hash table and switch statement or function pointers for speed?
     */
    if (attribute_q == attribute_size_q)
    {
        if (!ammonite_file_should_show_directory_item_count (file))
        {
            return g_strdup ("—");
        }
        count_unreadable = FALSE;
        if (ammonite_file_is_directory (file))
        {
            ammonite_file_get_directory_item_count (file, &item_count, &count_unreadable);
        }
        return g_strdup (count_unreadable ? "—" : "…");
    }
    if (attribute_q == attribute_deep_size_q)
    {
        status = ammonite_file_get_deep_counts (file, NULL, NULL, NULL, NULL, FALSE);
        if (status == AMMONITE_REQUEST_DONE)
        {
            /* This means no contents at all were readable */
            return g_strdup (_("? bytes"));
        }
        return g_strdup ("…");
    }
    if (attribute_q == attribute_deep_file_count_q
        || attribute_q == attribute_deep_directory_count_q
        || attribute_q == attribute_deep_total_count_q)
    {
        status = ammonite_file_get_deep_counts (file, NULL, NULL, NULL, NULL, FALSE);
        if (status == AMMONITE_REQUEST_DONE)
        {
            /* This means no contents at all were readable */
            return g_strdup (_("? items"));
        }
        return g_strdup ("…");
    }
    if (attribute_q == attribute_type_q
        || attribute_q == attribute_detailed_type_q
        || attribute_q == attribute_mime_type_q)
    {
        /* Translators: This about a file type. */
        return g_strdup (_("Unknown type"));
    }
    if (attribute_q == attribute_trashed_on_q)
    {
        /* If n/a */
        return g_strdup ("");
    }
    if (attribute_q == attribute_trash_orig_path_q)
    {
        /* If n/a */
        return g_strdup ("—");
    }
    if (attribute_q == attribute_recency_q)
    {
        /* If n/a */
        return g_strdup ("");
    }
    if (attribute_q == attribute_starred_q)
    {
        /* If n/a */
        return g_strdup ("");
    }
    if (attribute_q == attribute_date_created_full_q)
    {
        /* If n/a */
        return g_strdup ("—");
    }

    /* Fallback, use for both unknown attributes and attributes
     * for which we have no more appropriate default.
     */
    return g_strdup (_("Unknown"));
}

char *
ammonite_file_get_string_attribute_with_default (AmmoniteFile *file,
                                                 const char   *attribute_name)
{
    return ammonite_file_get_string_attribute_with_default_q (file, g_quark_from_string (attribute_name));
}

gboolean
ammonite_file_is_date_sort_attribute_q (GQuark attribute_q)
{
    if (attribute_q == attribute_modification_date_q ||
        attribute_q == attribute_date_modified_q ||
        attribute_q == attribute_date_modified_full_q ||
        attribute_q == attribute_accessed_date_q ||
        attribute_q == attribute_date_accessed_q ||
        attribute_q == attribute_date_accessed_full_q ||
        attribute_q == attribute_date_created_q ||
        attribute_q == attribute_date_created_full_q ||
        attribute_q == attribute_trashed_on_q ||
        attribute_q == attribute_trashed_on_full_q ||
        attribute_q == attribute_recency_q)
    {
        return TRUE;
    }

    return FALSE;
}

gboolean
ammonite_file_attribute_slow_sort (const gchar *sort_attribute)
{
    GQuark attribute_q = g_quark_from_string (sort_attribute);

    return attribute_q == attribute_size_q ||
           attribute_q == attribute_size_detail_q ||
           attribute_q == attribute_deep_size_q ||
           attribute_q == attribute_deep_file_count_q ||
           attribute_q == attribute_deep_directory_count_q ||
           attribute_q == attribute_deep_total_count_q;
}

static const struct
{
    const char *icon_name;
    const char *display_name;
} mime_type_map[] =
{
    { "application-x-executable", N_("Program") },
    { "audio-x-generic", N_("Audio") },
    { "font-x-generic", N_("Font") },
    { "image-x-generic", N_("Image") },
    { "package-x-generic", N_("Archive") },
    { "text-html", N_("Markup") },
    { "text-x-generic", N_("Text") },
    { "text-x-generic-template", N_("Text") },
    { "text-x-script", N_("Program") },
    { "video-x-generic", N_("Video") },
    { "x-office-address-book", N_("Contacts") },
    { "x-office-calendar", N_("Calendar") },
    { "x-office-document", N_("Document") },
    { "x-office-presentation", N_("Presentation") },
    { "x-office-spreadsheet", N_("Spreadsheet") },
};

/** Only returns NULL, if mime_type was NULL. */
static const char *
get_basic_type_for_mime_type (const char *mime_type)
{
    g_autofree char *icon_name = NULL;

    if (mime_type == NULL)
    {
        return NULL;
    }

    icon_name = g_content_type_get_generic_icon_name (mime_type);
    if (icon_name != NULL)
    {
        for (guint i = 0; i < G_N_ELEMENTS (mime_type_map); i++)
        {
            if (strcmp (mime_type_map[i].icon_name, icon_name) == 0)
            {
                const char *basic_type = gettext (mime_type_map[i].display_name);

                if (basic_type != NULL)
                {
                    return basic_type;
                }
                break;
            }
        }
    }

    /* Refers to a file type which is known but not one of the basic types */
    return _("Other");
}

static const char *
get_common_description (AmmoniteFile *file)
{
    const char *mime_type;

    g_assert (AMMONITE_IS_FILE (file));

    mime_type = file->details->mime_type;
    if (mime_type == NULL)
    {
        return NULL;
    }

    if (g_content_type_is_unknown (mime_type))
    {
        if (ammonite_file_is_executable (file))
        {
            return _("Program");
        }
        return _("Binary");
    }

    if (strcmp (mime_type, "inode/directory") == 0)
    {
        return _("Folder");
    }

    /* No common type */
    return NULL;
}

static const char *
get_description (AmmoniteFile *file)
{
    const char *common_description = get_common_description (file);
    if (common_description != NULL)
    {
        return common_description;
    }
    else
    {
        const char *mime_type = file->details->mime_type;

        return get_basic_type_for_mime_type (mime_type);
    }
}

static char *
get_detailed_description (AmmoniteFile *file)
{
    char *description;
    const char *mime_type = file->details->mime_type;
    const char *common_description = get_common_description (file);

    if (common_description != NULL)
    {
        return g_strdup (common_description);
    }

    if (mime_type == NULL)
    {
        return NULL;
    }

    description = g_content_type_get_description (mime_type);
    if (description != NULL)
    {
        return description;
    }

    return g_strdup (mime_type);
}

static char *
update_description_for_link (AmmoniteFile *file,
                             const char   *string)
{
    if (ammonite_file_is_symbolic_link (file))
    {
        g_assert (!ammonite_file_is_broken_symbolic_link (file));
        if (string == NULL)
        {
            return g_strdup (_("Link"));
        }
        /* Note to localizers: convert file type string for file
         * (e.g. "folder", "plain text") to file type for symbolic link
         * to that kind of file (e.g. "link to folder").
         */
        return g_strdup_printf (_("Link to %s"), string);
    }

    return g_strdup (string);
}

static char *
ammonite_file_get_type_as_string (AmmoniteFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }

    if (ammonite_file_is_broken_symbolic_link (file))
    {
        return g_strdup (_("Link (broken)"));
    }

    return update_description_for_link (file, get_description (file));
}

static const char *
ammonite_file_get_type_as_string_no_extra_text (AmmoniteFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }

    if (ammonite_file_is_broken_symbolic_link (file))
    {
        return _("Link (broken)");
    }

    return get_description (file);
}

static char *
ammonite_file_get_detailed_type_as_string (AmmoniteFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }

    if (ammonite_file_is_broken_symbolic_link (file))
    {
        return g_strdup (_("Link (broken)"));
    }

    g_autofree char *detailed_description = get_detailed_description (file);
    return update_description_for_link (file, detailed_description);
}

gboolean
ammonite_file_is_launchable (AmmoniteFile *file)
{
    gboolean type_can_be_executable;

    type_can_be_executable = FALSE;
    if (file->details->mime_type != NULL)
    {
        type_can_be_executable =
            g_content_type_can_be_executable (file->details->mime_type);
    }

    return type_can_be_executable &&
           ammonite_file_can_get_permissions (file) &&
           ammonite_file_can_execute (file) &&
           ammonite_file_is_executable (file) &&
           ammonite_file_is_regular_file (file);
}

/**
 * ammonite_file_is_symbolic_link
 *
 * Check if this file is a symbolic link.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: True if the file is a symbolic link.
 *
 **/
gboolean
ammonite_file_is_symbolic_link (AmmoniteFile *file)
{
    return file->details->is_symlink;
}

static void
file_mount_unmounted (GMount   *mount,
                      gpointer  data)
{
    AmmoniteFile *file;

    file = AMMONITE_FILE (data);

    ammonite_file_invalidate_attributes (file, AMMONITE_ATTRIBUTE_MOUNT);
}

void
ammonite_file_set_mount (AmmoniteFile *file,
                         GMount       *mount)
{
    if (file->details->mount)
    {
        g_signal_handlers_disconnect_by_func (file->details->mount, file_mount_unmounted, file);
        g_object_unref (file->details->mount);
        file->details->mount = NULL;
    }

    if (mount)
    {
        file->details->mount = g_object_ref (mount);
        g_signal_connect_object (mount, "unmounted",
                                 G_CALLBACK (file_mount_unmounted), file, G_CONNECT_DEFAULT);
    }
}

/**
 * ammonite_file_is_broken_symbolic_link
 *
 * Check if this file is a symbolic link with a missing target.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: True if the file is a symbolic link with a missing target.
 *
 **/
gboolean
ammonite_file_is_broken_symbolic_link (AmmoniteFile *file)
{
    if (file == NULL)
    {
        return FALSE;
    }

    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    /* Non-broken symbolic links return the target's type for get_file_type. */
    return ammonite_file_get_file_type (file) == G_FILE_TYPE_SYMBOLIC_LINK;
}

static void
get_fs_free_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
    AmmoniteFile *file;
    guint64 free_space;
    GFileInfo *info;

    file = AMMONITE_FILE (user_data);

    free_space = (guint64) - 1;
    info = g_file_query_filesystem_info_finish (G_FILE (source_object),
                                                res, NULL);
    if (info)
    {
        if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE))
        {
            free_space = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        }
        g_object_unref (info);
    }

    if (file->details->free_space != free_space)
    {
        file->details->free_space = free_space;
        ammonite_file_emit_changed (file);
    }

    ammonite_file_unref (file);
}

/**
 * ammonite_file_get_volume_free_space
 * Get a nicely formatted char with free space on the file's volume
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: newly-allocated copy of file size in a formatted string
 */
char *
ammonite_file_get_volume_free_space (AmmoniteFile *file)
{
    GFile *location;
    char *res;
    time_t now;

    now = time (NULL);
    /* Update first time and then every 2 seconds */
    if (file->details->free_space_read == 0 ||
        (now - file->details->free_space_read) > 2)
    {
        file->details->free_space_read = now;
        location = ammonite_file_get_location (file);
        g_file_query_filesystem_info_async (location,
                                            G_FILE_ATTRIBUTE_FILESYSTEM_FREE,
                                            0, NULL,
                                            get_fs_free_cb,
                                            ammonite_file_ref (file));
        g_object_unref (location);
    }

    res = NULL;
    if (file->details->free_space != (guint64) - 1)
    {
        g_autofree gchar *size_string = g_format_size (file->details->free_space);

        /* Translators: This refers to available space in a folder; e.g.: 100 MB Free */
        res = g_strdup_printf (_("%s Free"), size_string);
    }

    return res;
}

/**
 * ammonite_file_get_volume_name
 * Get the name of the volume the file resides on
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: newly-allocated copy of the volume name of the target file.
 */
char *
ammonite_file_get_volume_name (AmmoniteFile *file)
{
    g_autoptr (GFile) location = ammonite_file_get_location (file);
    g_autoptr (GMount) mount = g_file_find_enclosing_mount (location, NULL, NULL);

    return mount != NULL ? g_mount_get_name (mount) : NULL;
}

/**
 * ammonite_file_get_symbolic_link_target_path
 *
 * Get the file path of the target of a symbolic link. It is an error
 * to call this function on a file that isn't a symbolic link.
 * @file: AmmoniteFile representing the symbolic link in question.
 *
 * Returns: the file path of the target of the symbolic link.
 */
const char *
ammonite_file_get_symbolic_link_target_path (AmmoniteFile *file)
{
    if (!ammonite_file_is_symbolic_link (file))
    {
        g_warning ("File has symlink target, but  is not marked as symlink");
    }

    return file->details->symlink_name;
}

/**
 * ammonite_file_get_symbolic_link_target_uri
 *
 * Get the uri of the target of a symbolic link. It is an error
 * to call this function on a file that isn't a symbolic link.
 * @file: AmmoniteFile representing the symbolic link in question.
 *
 * Returns: newly-allocated copy of the uri of the target of the symbolic link.
 */
char *
ammonite_file_get_symbolic_link_target_uri (AmmoniteFile *file)
{
    GFile *location, *parent, *target;
    char *target_uri;

    if (!ammonite_file_is_symbolic_link (file))
    {
        g_warning ("File has symlink target, but  is not marked as symlink");
    }

    if (file->details->symlink_name == NULL)
    {
        return NULL;
    }
    else
    {
        target = NULL;

        location = ammonite_file_get_location (file);
        parent = g_file_get_parent (location);
        g_object_unref (location);
        if (parent)
        {
            target = g_file_resolve_relative_path (parent, file->details->symlink_name);
            g_object_unref (parent);
        }

        target_uri = NULL;
        if (target)
        {
            target_uri = g_file_get_uri (target);
            g_object_unref (target);
        }
        return target_uri;
    }
}

/**
 * ammonite_file_is_regular_file
 *
 * Check if this file is a regular file.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file is a regular file.
 *
 **/
gboolean
ammonite_file_is_regular_file (AmmoniteFile *file)
{
    return ammonite_file_get_file_type (file) == G_FILE_TYPE_REGULAR;
}

gboolean
ammonite_file_is_public_share_folder (AmmoniteFile *file)
{
    if (ammonite_file_is_user_special_directory (file, G_USER_DIRECTORY_PUBLIC_SHARE))
    {
        return TRUE;
    }
    if (g_strcmp0 (g_get_home_dir (), g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE)) != 0)
    {
        /* In order to match the behavior of gnome-user-share the ~/Public folder
         * is considered to be the public sharing folder when XDG_PUBLICSHARE_DIR
         * is set to the home folder. */
        g_autoptr (GFile) public_folder = g_file_new_build_filename (g_get_home_dir (), "Public", NULL);
        g_autoptr (GFile) location = ammonite_file_get_location (file);

        return g_file_equal (public_folder, location);
    }
    return FALSE;
}

/**
 * ammonite_file_is_user_special_directory
 *
 * Check if this file is a special platform directory.
 * @file: AmmoniteFile representing the file in question.
 * @special_directory: GUserDirectory representing the type to test for
 *
 * Returns: TRUE if @file is a special directory of the given kind.
 */
gboolean
ammonite_file_is_user_special_directory (AmmoniteFile   *file,
                                         GUserDirectory  special_directory)
{
    gboolean is_special_dir;
    const gchar *special_dir;

    if (ammonite_file_is_home (file))
    {
        /* A xdg-user-dir is disabled by setting it to the home directory */
        return FALSE;
    }

    special_dir = g_get_user_special_dir (special_directory);
    is_special_dir = FALSE;

    if (special_dir)
    {
        GFile *loc;
        GFile *special_gfile;

        loc = ammonite_file_get_location (file);
        special_gfile = g_file_new_for_path (special_dir);
        is_special_dir = g_file_equal (loc, special_gfile);
        g_object_unref (special_gfile);
        g_object_unref (loc);
    }

    return is_special_dir;
}

gboolean
ammonite_file_is_archive (AmmoniteFile *file)
{
    const char *mime_type = ammonite_file_get_mime_type (file);

    return autoar_check_mime_type_supported (mime_type);
}


/**
 * ammonite_file_is_in_trash
 *
 * Check if this file is a file in trash.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file is in a trash.
 *
 **/
gboolean
ammonite_file_is_in_trash (AmmoniteFile *file)
{
    g_assert (AMMONITE_IS_FILE (file));

    return ammonite_directory_is_in_trash (file->details->directory);
}

/**
 * ammonite_file_is_in_recent
 *
 * Check if this file is a file in Recent.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file is in Recent.
 *
 **/
gboolean
ammonite_file_is_in_recent (AmmoniteFile *file)
{
    g_assert (AMMONITE_IS_FILE (file));

    return ammonite_directory_is_in_recent (file->details->directory);
}

/**
 * ammonite_file_is_in_starred
 *
 * Check if this file is a file in Starred.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file is in Starred.
 *
 **/
gboolean
ammonite_file_is_in_starred (AmmoniteFile *file)
{
    g_assert (AMMONITE_IS_FILE (file));

    return ammonite_directory_is_in_starred (file->details->directory);
}

/**
 * ammonite_file_is_remote
 *
 * Check if this file is a file in a remote filesystem.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file is in a remote filesystem.
 *
 **/
gboolean
ammonite_file_is_remote (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return get_filesystem_remote (file, NULL);
}

/**
 * ammonite_file_is_starred_location
 *
 * Check if this file is the Favorite location.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file is the Favorite location.
 *
 **/
gboolean
ammonite_file_is_starred_location (AmmoniteFile *file)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    location = ammonite_file_get_location (file);

    return g_file_has_uri_scheme (location, SCHEME_STARRED);
}

gboolean
ammonite_file_is_network_view (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    g_autoptr (GFile) location = ammonite_file_get_location (file);

    return ammonite_is_root_for_scheme (location, SCHEME_NETWORK_VIEW);
}

/**
 * ammonite_file_is_in_admin
 *
 * Check if this file is using admin backend.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file is using admin backend.
 *
 **/
gboolean
ammonite_file_is_in_admin (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return ammonite_directory_is_in_admin (file->details->directory);
}

GError *
ammonite_file_get_file_info_error (AmmoniteFile *file)
{
    if (!file->details->get_info_failed)
    {
        return NULL;
    }

    return file->details->get_info_error;
}

/**
 * ammonite_file_contains_text
 *
 * Check if this file contains text.
 * This is private and is used to decide whether or not to read the top left text.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file has a text MIME type.
 *
 **/
gboolean
ammonite_file_contains_text (AmmoniteFile *file)
{
    if (file == NULL)
    {
        return FALSE;
    }

    /* All text files inherit from text/plain */
    return ammonite_file_is_mime_type (file, "text/plain");
}

/**
 * ammonite_file_is_executable
 *
 * Check if this file is executable at all.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if any of the execute bits are set. FALSE if
 * not, or if the permissions are unknown.
 *
 **/
gboolean
ammonite_file_is_executable (AmmoniteFile *file)
{
    if (!file->details->has_permissions)
    {
        /* File's permissions field is not valid.
         * Can't access specific permissions, so return FALSE.
         */
        return FALSE;
    }

    return file->details->can_execute;
}

const char *
ammonite_file_get_filesystem_id (AmmoniteFile *file)
{
    return file->details->filesystem_id;
}

AmmoniteFile *
ammonite_file_get_trash_original_file (AmmoniteFile *file)
{
    GFile *location;
    AmmoniteFile *original_file;

    original_file = NULL;

    if (file->details->trash_orig_path != NULL)
    {
        location = g_file_new_for_path (file->details->trash_orig_path);
        original_file = ammonite_file_get (location);
        g_object_unref (location);
    }

    return original_file;
}

void
ammonite_file_mark_unmounted (AmmoniteFile *file)
{
    file->details->has_been_unmounted = TRUE;
}

void
ammonite_file_mark_gone (AmmoniteFile *file)
{
    AmmoniteDirectory *directory;

    if (file->details->is_gone)
    {
        return;
    }

    file->details->is_gone = TRUE;

    update_links_if_target (file);

    /* Drop it from the symlink hash ! */
    remove_from_link_hash_table (file);

    /* Removing the file from the directory can result in dropping the last
     * reference, and so clearing the info then will result in a crash.
     */
    ammonite_file_clear_info (file);

    /* Let the directory know it's gone. */
    directory = file->details->directory;
    if (!ammonite_file_is_self_owned (file))
    {
        ammonite_directory_remove_file (directory, file);
    }

    /* FIXME bugzilla.gnome.org 42429:
     * Maybe we can get rid of the name too eventually, but
     * for now that would probably require too many if statements
     * everywhere anyone deals with the name. Maybe we can give it
     * a hard-coded "<deleted>" name or something.
     */
}

/**
 * ammonite_file_changed
 *
 * Notify the user that this file has changed.
 * @file: AmmoniteFile representing the file in question.
 **/
void
ammonite_file_changed (AmmoniteFile *file)
{
    g_return_if_fail (AMMONITE_IS_FILE (file));

    if (ammonite_file_is_self_owned (file))
    {
        ammonite_file_emit_changed (file);
    }
    else
    {
        ammonite_directory_emit_change_signals (file->details->directory,
                                                &(AmmoniteFileList){ .data = file });
    }
}

/**
 * ammonite_file_updated_deep_count_in_progress
 *
 * Notify clients that a newer deep count is available for
 * the directory in question.
 */
void
ammonite_file_updated_deep_count_in_progress (AmmoniteFile *file)
{
    GList *link_files, *node;

    g_assert (AMMONITE_IS_FILE (file));
    g_assert (ammonite_file_is_directory (file));

    /* Send out a signal. */
    g_signal_emit (file, signals[UPDATED_DEEP_COUNT_IN_PROGRESS], 0, file);

    /* Tell link files pointing to this object about the change. */
    link_files = get_link_files (file);
    for (node = link_files; node != NULL; node = node->next)
    {
        ammonite_file_updated_deep_count_in_progress (AMMONITE_FILE (node->data));
    }
    ammonite_file_list_free (link_files);
}

/**
 * ammonite_file_emit_changed
 *
 * Emit a file changed signal.
 * This can only be called by the directory, since the directory
 * also has to emit a files_changed signal.
 *
 * @file: AmmoniteFile representing the file in question.
 **/
void
ammonite_file_emit_changed (AmmoniteFile *file)
{
    GList *link_files, *p;

    g_assert (AMMONITE_IS_FILE (file));

    /* Send out a signal. */
    g_signal_emit (file, signals[CHANGED], 0, file);

    /* Tell link files pointing to this object about the change. */
    link_files = get_link_files (file);
    for (p = link_files; p != NULL; p = p->next)
    {
        /* Looking for directly recursive links. */
        g_autolist (AmmoniteFile) link_targets = NULL;
        AmmoniteDirectory *directory;

        /* Files can be links to themselves. */
        if (p->data == file)
        {
            continue;
        }

        link_targets = get_link_files (p->data);
        directory = ammonite_file_get_directory (p->data);

        /* Reiterating (heh) that this will break with more complex cycles.
         * Users, stop trying to break things on purpose.
         */
        if (g_list_find (link_targets, file) != NULL &&
            directory == ammonite_file_get_directory (file))
        {
            g_signal_emit (p->data, signals[CHANGED], 0, p->data);
            continue;
        }

        ammonite_file_changed (AMMONITE_FILE (p->data));
    }
    ammonite_file_list_free (link_files);
}

gboolean
ammonite_file_has_been_unmounted (AmmoniteFile *file)
{
    return file->details->has_been_unmounted;
}

/**
 * ammonite_file_check_if_ready
 *
 * Check whether the values for a set of file attributes are
 * currently available, without doing any additional work. This
 * is useful for callers that want to reflect updated information
 * when it is ready but don't want to force the work required to
 * obtain the information, which might be slow network calls, e.g.
 *
 * @file: The file being queried.
 * @attributes: A bit-mask with the desired information.
 *
 * Return value: TRUE if all of the specified attributes are currently readable.
 */
gboolean
ammonite_file_check_if_ready (AmmoniteFile       *file,
                              AmmoniteAttributes  attributes)
{
    /* To be parallel with call_when_ready, return
     * TRUE for NULL file.
     */
    if (file == NULL)
    {
        return TRUE;
    }

    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file))->check_if_ready (file, attributes);
}

void
ammonite_file_call_when_ready (AmmoniteFile         *file,
                               AmmoniteAttributes    attributes,
                               AmmoniteFileCallback  callback,
                               gpointer              callback_data)
{
    if (file == NULL)
    {
        (*callback)(file, callback_data);
        return;
    }

    g_return_if_fail (AMMONITE_IS_FILE (file));

    AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file))->call_when_ready
        (file, attributes, callback, callback_data);
}

void
ammonite_file_cancel_call_when_ready (AmmoniteFile         *file,
                                      AmmoniteFileCallback  callback,
                                      gpointer              callback_data)
{
    g_return_if_fail (callback != NULL);

    if (file == NULL)
    {
        return;
    }

    g_return_if_fail (AMMONITE_IS_FILE (file));

    AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file))->cancel_call_when_ready
        (file, callback, callback_data);
}

static void
invalidate_directory_count (AmmoniteFile *file)
{
    file->details->directory_count_is_up_to_date = FALSE;
}

static void
invalidate_deep_counts (AmmoniteFile *file)
{
    file->details->deep_counts_status = AMMONITE_REQUEST_NOT_STARTED;
}

static void
invalidate_file_info (AmmoniteFile *file)
{
    file->details->file_info_is_up_to_date = FALSE;
}

static void
invalidate_thumbnail_info (AmmoniteFile *file)
{
    file->details->thumbnail_info_is_up_to_date = FALSE;
}

static void
invalidate_thumbnail (AmmoniteFile *file)
{
    file->details->thumbnail_is_up_to_date = FALSE;
}

static void
invalidate_mount (AmmoniteFile *file)
{
    file->details->mount_is_up_to_date = FALSE;
}

void
ammonite_file_invalidate_extension_info_internal (AmmoniteFile *file)
{
    g_list_free_full (file->details->extension_emblems, g_free);
    file->details->extension_emblems = NULL;
    g_clear_pointer (&file->details->extension_attributes, g_hash_table_destroy);
    g_list_free_full (file->details->pending_info_providers, g_object_unref);

    file->details->pending_info_providers =
        ammonite_module_get_extensions_for_type (AMMONITE_TYPE_INFO_PROVIDER);
}

void
ammonite_file_invalidate_attributes_internal (AmmoniteFile       *file,
                                              AmmoniteAttributes  attributes)
{
    if (file == NULL)
    {
        return;
    }

    if (IS_ATTRIBUTE_SET (attributes, AMMONITE_ATTRIBUTE_DIRECTORY_ITEM_COUNT))
    {
        invalidate_directory_count (file);
    }
    if (IS_ATTRIBUTE_SET (attributes, AMMONITE_ATTRIBUTE_DEEP_COUNT))
    {
        invalidate_deep_counts (file);
    }
    if (IS_ATTRIBUTE_SET (attributes, AMMONITE_ATTRIBUTE_INFO))
    {
        invalidate_file_info (file);
    }
    if (IS_ATTRIBUTE_SET (attributes, AMMONITE_ATTRIBUTE_EXTENSION_INFO))
    {
        ammonite_file_invalidate_extension_info_internal (file);
    }
    if (IS_ATTRIBUTE_SET (attributes, AMMONITE_ATTRIBUTE_THUMBNAIL_INFO))
    {
        invalidate_thumbnail_info (file);
    }
    if (IS_ATTRIBUTE_SET (attributes, AMMONITE_ATTRIBUTE_THUMBNAIL_BUFFER))
    {
        invalidate_thumbnail (file);
    }
    if (IS_ATTRIBUTE_SET (attributes, AMMONITE_ATTRIBUTE_MOUNT))
    {
        invalidate_mount (file);
    }

    /* FIXME bugzilla.gnome.org 45075: implement invalidating metadata */
}

gboolean
ammonite_file_set_thumbnail (AmmoniteFile *file,
                             GdkTexture   *texture,
                             time_t        thumb_mtime)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    file->details->thumbnail_is_up_to_date = TRUE;
    g_clear_object (&file->details->thumbnail);

    if (texture != NULL)
    {
        if (thumb_mtime == 0 ||
            thumb_mtime == file->details->mtime)
        {
            file->details->thumbnail = g_object_ref (texture);
            file->details->thumbnail_mtime = thumb_mtime;

            if (file->details->thumbnail_path == NULL)
            {
                g_autofree gchar *uri = ammonite_file_get_uri (file);

                file->details->thumbnail_path = ammonite_thumbnail_get_path_for_uri (uri);
            }
        }
        else
        {
            return FALSE;
        }
    }

    return TRUE;
}


/**
 * ammonite_file_invalidate_attributes
 *
 * Invalidate the specified attributes and force a reload.
 * @file: AmmoniteFile representing the file in question.
 * @attributes: attributes to froget.
 **/

void
ammonite_file_invalidate_attributes (AmmoniteFile       *file,
                                     AmmoniteAttributes  attributes)
{
    /* Cancel possible in-progress loads of any of these attributes */
    ammonite_directory_cancel_loading_attributes (file->details->directory,
                                                  file,
                                                  attributes);

    /* Actually invalidate the values */
    ammonite_file_invalidate_attributes_internal (file, attributes);

    ammonite_directory_add_file_to_work_queue (file->details->directory, file);

    /* Kick off I/O if necessary */
    ammonite_directory_async_state_changed (file->details->directory);
}

AmmoniteAttributes
ammonite_file_get_all_attributes (void)
{
    return AMMONITE_ATTRIBUTE_INFO |
           AMMONITE_ATTRIBUTE_DEEP_COUNT |
           AMMONITE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
           AMMONITE_ATTRIBUTE_EXTENSION_INFO |
           AMMONITE_ATTRIBUTE_THUMBNAIL_INFO |
           AMMONITE_ATTRIBUTE_THUMBNAIL_BUFFER |
           AMMONITE_ATTRIBUTE_MOUNT;
}

void
ammonite_file_invalidate_all_attributes (AmmoniteFile *file)
{
    AmmoniteAttributes all_attributes;

    all_attributes = ammonite_file_get_all_attributes ();
    ammonite_file_invalidate_attributes (file, all_attributes);
}


/**
 * ammonite_file_dump
 *
 * Debugging call, prints out the contents of the file
 * fields.
 *
 * @file: file to dump.
 **/
void
ammonite_file_dump (AmmoniteFile *file)
{
    long size = file->details->deep_size;
    char *uri;
    const char *file_kind;

    uri = ammonite_file_get_uri (file);
    g_print ("uri: %s \n", uri);
    if (!file->details->got_file_info)
    {
        g_print ("no file info \n");
    }
    else if (file->details->get_info_failed)
    {
        g_print ("failed to get file info \n");
    }
    else
    {
        g_print ("size: %ld \n", size);
        switch (file->details->type)
        {
            case G_FILE_TYPE_REGULAR:
            {
                file_kind = "regular file";
            }
            break;

            case G_FILE_TYPE_DIRECTORY:
            {
                file_kind = "folder";
            }
            break;

            case G_FILE_TYPE_SPECIAL:
            {
                file_kind = "special";
            }
            break;

            case G_FILE_TYPE_SYMBOLIC_LINK:
            {
                file_kind = "symbolic link";
            }
            break;

            case G_FILE_TYPE_UNKNOWN:
            default:
            {
                file_kind = "unknown";
            }
            break;
        }
        g_print ("kind: %s \n", file_kind);
        if (file->details->type == G_FILE_TYPE_SYMBOLIC_LINK)
        {
            g_print ("link to %s \n", file->details->symlink_name);
            /* FIXME bugzilla.gnome.org 42430: add following of symlinks here */
        }
        /* FIXME bugzilla.gnome.org 42431: add permissions and other useful stuff here */
    }
    g_free (uri);
}

void
ammonite_file_list_debug (GList *files)
{
    if (g_getenv ("G_MESSAGES_DEBUG") == NULL)
    {
        return;
    }

    for (GList *l = files; l != NULL; l = l->next)
    {
        AmmoniteFile *file = l->data;
        g_autofree gchar *uri = ammonite_file_get_uri (file);

        g_debug ("%s%s", uri, ammonite_file_is_gone (file) ? " (gone)" : "");
    }
}

/**
 * ammonite_file_list_free
 *
 * Free a list of files after unrefing them.
 * @list: GList of files.
 **/
void
ammonite_file_list_free (GList *list)
{
    g_list_free_full (list, (GDestroyNotify) ammonite_file_unref);
}

/**
 * ammonite_file_list_copy
 *
 * Copy the list of files, making a new ref of each,
 * @list: GList of files.
 **/
GList *
ammonite_file_list_copy (GList *list)
{
    return g_list_copy_deep (list, (GCopyFunc) ammonite_file_ref, NULL);
}

/**
 * ammonite_file_get_default_sort_order:
 * @file: A #AmmoniteFile representing a location
 * @reversed: (out): Location to store whether the order is reversed by default.
 * @is_forced: (out): Bool pointer set to TRUE when file has enforced sort order
 *
 * Gets which sort order applies by default for the provided locations.
 *
 * If @file is a location with special needs (e.g. Trash or Recent), the return
 * value and @reversed flag are dictated by design. Otherwise, they stem from
 * the "default-sort-order" and "default-sort-in-reverse-order" preference keys.
 *
 * Returns: Identifier of the default sort type for this @file.
 */
const char *
ammonite_file_get_default_sort_order (AmmoniteFile *file,
                                      gboolean     *reversed,
                                      gboolean     *is_forced)
{
    g_assert (reversed != NULL);
    g_assert (is_forced != NULL);

    /* Special handling for certain directories */
    if (ammonite_file_is_user_special_directory (file, G_USER_DIRECTORY_DOWNLOAD))
    {
        *reversed = TRUE;
        return g_quark_to_string (attribute_date_modified_q);
    }
    else if (ammonite_file_is_in_trash (file))
    {
        *reversed = TRUE;
        *is_forced = TRUE;
        return g_quark_to_string (attribute_trashed_on_q);
    }
    else if (ammonite_file_is_in_recent (file))
    {
        *reversed = TRUE;
        *is_forced = TRUE;
        return g_quark_to_string (attribute_recency_q);
    }
    else if (ammonite_file_is_in_search (file))
    {
        *reversed = TRUE;
        *is_forced = TRUE;
        return g_quark_to_string (attribute_search_relevance_q);
    }

    /* Use defaults */
    *reversed = g_settings_get_boolean (ammonite_preferences,
                                        AMMONITE_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);

    g_autofree char *sort_order_str = g_settings_get_string (ammonite_preferences,
                                                             AMMONITE_PREFERENCES_DEFAULT_SORT_ORDER);
    GQuark sort_order = g_quark_from_string (sort_order_str);

    return g_quark_to_string (sort_order);
}

static int
compare_by_display_name_cover (gconstpointer a,
                               gconstpointer b)
{
    return compare_by_display_name (AMMONITE_FILE (a), AMMONITE_FILE (b));
}

/**
 * ammonite_file_list_sort_by_display_name
 *
 * Sort the list of files by file name.
 * @list: GList of files.
 **/
GList *
ammonite_file_list_sort_by_display_name (GList *list)
{
    return g_list_sort (list, compare_by_display_name_cover);
}

static GList *ready_data_list = NULL;

typedef struct
{
    GList *file_list;
    GHashTable *remaining_files;
    gboolean all_checked;
    AmmoniteFileListCallback callback;
    gpointer callback_data;
} FileListReadyData;

static void
file_list_ready_data_free (FileListReadyData *data)
{
    GList *node = g_list_find (ready_data_list, data);

    g_return_if_fail (node != NULL);

    ready_data_list = g_list_delete_link (ready_data_list, node);

    ammonite_file_list_free (data->file_list);
    g_hash_table_unref (data->remaining_files);
    g_free (data);
}

static FileListReadyData *
file_list_ready_data_new (GList                    *file_list,
                          AmmoniteFileListCallback  callback,
                          gpointer                  callback_data)
{
    FileListReadyData *data;

    data = g_new0 (FileListReadyData, 1);
    data->file_list = ammonite_file_list_copy (file_list);
    data->remaining_files = g_hash_table_new (NULL, NULL);
    data->callback = callback;
    data->callback_data = callback_data;

    ready_data_list = g_list_prepend (ready_data_list, data);

    return data;
}

static void
file_list_file_ready_finish (FileListReadyData *data)
{
    if (data->callback)
    {
        (*data->callback)(data->file_list, data->callback_data);
    }

    file_list_ready_data_free (data);
}

static void
file_list_file_ready_callback (AmmoniteFile *file,
                               gpointer      user_data)
{
    FileListReadyData *data = user_data;

    g_hash_table_remove (data->remaining_files, file);

    if (data->all_checked && g_hash_table_size (data->remaining_files) == 0)
    {
        file_list_file_ready_finish (data);
    }
}

void
ammonite_file_list_call_when_ready (GList                     *file_list,
                                    AmmoniteAttributes         attributes,
                                    AmmoniteFileListHandle   **handle,
                                    AmmoniteFileListCallback   callback,
                                    gpointer                   callback_data)
{
    g_return_if_fail (file_list != NULL);

    FileListReadyData *data = file_list_ready_data_new (file_list,
                                                        callback,
                                                        callback_data);

    if (handle)
    {
        *handle = (AmmoniteFileListHandle *) data;
    }

    for (GList *l = file_list; l != NULL;)
    {
        AmmoniteFile *file = AMMONITE_FILE (l->data);

        /* Need to do this here, as the callback can modify the list */
        l = l->next;

        if (!ammonite_file_check_if_ready (file, attributes))
        {
            g_hash_table_add (data->remaining_files, file);
            ammonite_file_call_when_ready (file,
                                           attributes,
                                           file_list_file_ready_callback,
                                           data);
        }
    }

    /* By now all files are checked, if all are ready callback can be called */
    data->all_checked = TRUE;

    /* In case all files are already ready call callback now */
    if (g_hash_table_size (data->remaining_files) == 0)
    {
        file_list_file_ready_finish (data);
    }
}

void
ammonite_file_list_cancel_call_when_ready (AmmoniteFileListHandle *handle)
{
    g_return_if_fail (handle != NULL);

    FileListReadyData *data = (FileListReadyData *) handle;

    if (g_list_find (ready_data_list, data) == NULL)
    {
        g_return_if_reached ();
    }

    g_autoptr (GPtrArray) remaining_files = g_hash_table_steal_all_keys (data->remaining_files);

    for (guint i = 0; i < remaining_files->len; i++)
    {
        AmmoniteFile *file = AMMONITE_FILE (remaining_files->pdata[i]);
        AmmoniteFileClass *file_class = AMMONITE_FILE_CLASS (G_OBJECT_GET_CLASS (file));

        file_class->cancel_call_when_ready (file, file_list_file_ready_callback, data);
    }

    file_list_ready_data_free (data);
}

static void
update_thumbnail_limit (void)
{
    cached_thumbnail_limit = g_settings_get_uint64 (ammonite_preferences,
                                                    AMMONITE_PREFERENCES_FILE_THUMBNAIL_LIMIT);

    /*Converts the obtained limit in MB to bytes */
    cached_thumbnail_limit *= MEGA_TO_BASE_RATE;
}

static void
thumbnail_limit_changed_callback (gpointer user_data)
{
    update_thumbnail_limit ();

    /* Tell the world that icons might have changed. We could invent a narrower-scope
     * signal to mean only "thumbnails might have changed" if this ends up being slow
     * for some reason.
     */
    emit_change_signals_for_all_files_in_all_directories ();
}

static void
update_show_thumbnails (void)
{
    show_file_thumbs = g_settings_get_enum (ammonite_preferences, AMMONITE_PREFERENCES_SHOW_FILE_THUMBNAILS);
}

static void
show_thumbnails_changed_callback (gpointer user_data)
{
    update_show_thumbnails ();

    /* Tell the world that icons might have changed. We could invent a narrower-scope
     * signal to mean only "thumbnails might have changed" if this ends up being slow
     * for some reason.
     */
    emit_change_signals_for_all_files_in_all_directories ();
}

static void
update_show_directory_item_count (void)
{
    show_directory_item_count = g_settings_get_enum (ammonite_preferences, AMMONITE_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS);
}

static void
mime_type_data_changed_callback (GObject  *signaller,
                                 gpointer  user_data)
{
    /* Tell the world that icons might have changed. We could invent a narrower-scope
     * signal to mean only "thumbnails might have changed" if this ends up being slow
     * for some reason.
     */
    emit_change_signals_for_all_files_in_all_directories ();
}

static gboolean
real_get_item_count (AmmoniteFile *file,
                     guint        *count,
                     gboolean     *count_unreadable)
{
    if (count_unreadable != NULL)
    {
        *count_unreadable = file->details->directory_count_failed;
    }

    if (!file->details->got_directory_count)
    {
        if (count != NULL)
        {
            *count = 0;
        }
        return FALSE;
    }

    if (count != NULL)
    {
        *count = file->details->directory_count;
    }

    return TRUE;
}

static AmmoniteRequestStatus
real_get_deep_counts (AmmoniteFile *file,
                      guint        *directory_count,
                      guint        *file_count,
                      guint        *unreadable_directory_count,
                      goffset      *total_size)
{
    GFileType type;

    type = ammonite_file_get_file_type (file);

    if (directory_count != NULL)
    {
        *directory_count = 0;
    }
    if (file_count != NULL)
    {
        *file_count = 0;
    }
    if (unreadable_directory_count != NULL)
    {
        *unreadable_directory_count = 0;
    }
    if (total_size != NULL)
    {
        *total_size = 0;
    }

    if (type != G_FILE_TYPE_DIRECTORY)
    {
        return AMMONITE_REQUEST_DONE;
    }

    if (file->details->deep_counts_status != AMMONITE_REQUEST_NOT_STARTED)
    {
        if (directory_count != NULL)
        {
            *directory_count = file->details->deep_directory_count;
        }
        if (file_count != NULL)
        {
            *file_count = file->details->deep_file_count;
        }
        if (unreadable_directory_count != NULL)
        {
            *unreadable_directory_count = file->details->deep_unreadable_count;
        }
        if (total_size != NULL)
        {
            *total_size = file->details->deep_size;
        }
        return file->details->deep_counts_status;
    }

    /* For directories, or before we know the type, we haven't started. */
    if (type == G_FILE_TYPE_UNKNOWN || type == G_FILE_TYPE_DIRECTORY)
    {
        return AMMONITE_REQUEST_NOT_STARTED;
    }

    /* For other types, we are done, and the zeros are permanent. */
    return AMMONITE_REQUEST_DONE;
}

static void
ammonite_file_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
    AmmoniteFile *file = AMMONITE_FILE (object);

    switch (prop_id)
    {
        case PROP_DISPLAY_NAME:
        {
            g_value_set_string (value, ammonite_file_get_display_name (file));
        }
        break;

        case PROP_A11Y_NAME:
        {
            AmmoniteTagManager *tag_manager = ammonite_tag_manager_get ();
            g_autofree gchar *uri = ammonite_file_get_uri (file);
            gboolean is_starred = ammonite_tag_manager_file_is_starred (tag_manager, uri);
            gboolean is_directory = ammonite_file_is_directory (file);

            const gchar *role = is_directory ?
                                /* Translators: This is the role for a folder icon when a screen reader is
                                 * used. If a folder is selected, it will say "XXXXXX. Folder" */
                                C_("Role", "Folder") :
                                /* Translators: This is the role for a file icon when a screen reader is
                                 * used. If a file is selected, it will say "XXXXXX. File" */
                                C_("Role", "File");

            g_autofree gchar *accessible_name = NULL;
            if (is_starred)
            {
                accessible_name = g_strdup_printf (
                    /* A "." is added in between file name and the role,
                     * and another one between the role and the starring
                     * action description as a useful pause in the screen reader
                     * announcement.
                     */
                    "%s. %s. %s",
                    file->details->display_name,
                    role,
                    /* Translators: when a file is marked with a star, this text
                     * is added at the end of the name when prononunced by a
                     * screen reader, after the role
                     * (e.g. "My documents. Folder. Starred")
                     */
                    C_("Role", "Starred"));
            }
            else
            {
                accessible_name = g_strdup_printf (
                    /* A "." is added in between file name and the role
                     * as a useful pause in the screen reader
                     * announcement.
                     */
                    "%s. %s",
                    file->details->display_name,
                    role);
            }
            g_value_set_string (value, accessible_name);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_file_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    AmmoniteFile *file = AMMONITE_FILE (object);

    switch (prop_id)
    {
        case PROP_DIRECTORY:
        {
            ammonite_file_set_directory (file, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
default_no_op (AmmoniteFile *file)
{
    /* Dummy default impl */
}

static AmmoniteFileInfo *
ammonite_file_get_info (GFile    *location,
                        gboolean  create)
{
    return AMMONITE_FILE_INFO ((create)
                               ? ammonite_file_get (location)
                               : ammonite_file_get_existing (location));
}

static void
ammonite_file_class_init (AmmoniteFileClass *class)
{
    ammonite_file_info_getter = ammonite_file_get_info;

    attribute_name_q = g_quark_from_static_string ("name");
    attribute_size_q = g_quark_from_static_string ("size");
    attribute_type_q = g_quark_from_static_string ("type");
    attribute_detailed_type_q = g_quark_from_static_string ("detailed_type");
    attribute_modification_date_q = g_quark_from_static_string ("modification_date");
    attribute_date_modified_q = g_quark_from_static_string ("date_modified");
    attribute_date_modified_full_q = g_quark_from_static_string ("date_modified_full");
    attribute_recency_q = g_quark_from_static_string ("recency");
    attribute_accessed_date_q = g_quark_from_static_string ("accessed_date");
    attribute_date_accessed_q = g_quark_from_static_string ("date_accessed");
    attribute_date_accessed_full_q = g_quark_from_static_string ("date_accessed_full");
    attribute_date_created_q = g_quark_from_static_string ("date_created");
    attribute_date_created_full_q = g_quark_from_static_string ("date_created_full");
    attribute_mime_type_q = g_quark_from_static_string ("mime_type");
    attribute_size_detail_q = g_quark_from_static_string ("size_detail");
    attribute_deep_size_q = g_quark_from_static_string ("deep_size");
    attribute_deep_file_count_q = g_quark_from_static_string ("deep_file_count");
    attribute_deep_directory_count_q = g_quark_from_static_string ("deep_directory_count");
    attribute_deep_total_count_q = g_quark_from_static_string ("deep_total_count");
    attribute_search_relevance_q = g_quark_from_static_string ("search_relevance");
    attribute_trashed_on_q = g_quark_from_static_string ("trashed_on");
    attribute_trashed_on_full_q = g_quark_from_static_string ("trashed_on_full");
    attribute_trash_orig_path_q = g_quark_from_static_string ("trash_orig_path");
    attribute_permissions_q = g_quark_from_static_string ("permissions");
    attribute_selinux_context_q = g_quark_from_static_string ("selinux_context");
    attribute_octal_permissions_q = g_quark_from_static_string ("octal_permissions");
    attribute_owner_q = g_quark_from_static_string ("owner");
    attribute_group_q = g_quark_from_static_string ("group");
    attribute_uri_q = g_quark_from_static_string ("uri");
    attribute_where_q = g_quark_from_static_string ("where");
    attribute_link_target_q = g_quark_from_static_string ("link_target");
    attribute_volume_q = g_quark_from_static_string ("volume");
    attribute_free_space_q = g_quark_from_static_string ("free_space");
    attribute_starred_q = g_quark_from_static_string ("starred");

    G_OBJECT_CLASS (class)->dispose = ammonite_file_dispose;
    G_OBJECT_CLASS (class)->finalize = finalize;
    G_OBJECT_CLASS (class)->constructor = ammonite_file_constructor;
    G_OBJECT_CLASS (class)->get_property = ammonite_file_get_property;
    G_OBJECT_CLASS (class)->set_property = ammonite_file_set_property;

    class->get_item_count = real_get_item_count;
    class->get_deep_counts = real_get_deep_counts;

    class->set_metadata = (void *) default_no_op;
    class->mount = (void *) default_no_op;
    class->unmount = (void *) default_no_op;
    class->eject = (void *) default_no_op;
    class->start = (void *) default_no_op;
    class->stop = (void *) default_no_op;
    class->poll_for_media = (void *) default_no_op;

    signals[CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AmmoniteFileClass, changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[UPDATED_DEEP_COUNT_IN_PROGRESS] =
        g_signal_new ("updated-deep-count-in-progress",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AmmoniteFileClass, updated_deep_count_in_progress),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    update_thumbnail_limit ();
    g_signal_connect_swapped (ammonite_preferences,
                              "changed::" AMMONITE_PREFERENCES_FILE_THUMBNAIL_LIMIT,
                              G_CALLBACK (thumbnail_limit_changed_callback),
                              NULL);
    update_show_thumbnails ();
    g_signal_connect_swapped (ammonite_preferences,
                              "changed::" AMMONITE_PREFERENCES_SHOW_FILE_THUMBNAILS,
                              G_CALLBACK (show_thumbnails_changed_callback),
                              NULL);
    update_show_directory_item_count ();
    g_signal_connect_swapped (ammonite_preferences,
                              "changed::" AMMONITE_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
                              G_CALLBACK (update_show_directory_item_count),
                              NULL);

    g_signal_connect (ammonite_signaller_get_current (),
                      "mime-data-changed",
                      G_CALLBACK (mime_type_data_changed_callback),
                      NULL);

    properties[PROP_DIRECTORY] = g_param_spec_object ("directory", NULL, NULL,
                                                      AMMONITE_TYPE_DIRECTORY,
                                                      (G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                                                       G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    properties[PROP_DISPLAY_NAME] = g_param_spec_string ("display-name", NULL, NULL,
                                                         "",
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_A11Y_NAME] = g_param_spec_string ("a11y-name", NULL, NULL,
                                                      "",
                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (G_OBJECT_CLASS (class), N_PROPS, properties);
}

static gboolean
is_gone (AmmoniteFileInfo *file_info)
{
    return ammonite_file_is_gone (AMMONITE_FILE (file_info));
}

/**
 * ammonite_file_is_gone
 *
 * Check if a file has already been deleted.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if the file is already gone.
 **/
gboolean
ammonite_file_is_gone (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->is_gone;
}

static char *
get_name (AmmoniteFileInfo *file_info)
{
    return g_strdup (ammonite_file_get_name (AMMONITE_FILE (file_info)));
}

const char *
ammonite_file_get_name (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    return file->details->name;
}

static char *
get_uri (AmmoniteFileInfo *file_info)
{
    return ammonite_file_get_uri (AMMONITE_FILE (file_info));
}

/* Return the actual uri associated with the passed-in file. */
char *
ammonite_file_get_uri (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    g_autoptr (GFile) location = ammonite_file_get_location (file);

    return g_file_get_uri (location);
}

static char *
get_parent_uri (AmmoniteFileInfo *file_info)
{
    return ammonite_file_get_parent_uri (AMMONITE_FILE (file_info));
}

/**
 * ammonite_file_get_parent_uri:
 *
 * Get the uri for the parent directory.
 *
 * @file: The file in question.
 *
 * Return value: A string for the parent's location, in "raw URI" form.
 * Use ammonite_file_get_parent_uri_for_display instead if the
 * result is to be displayed on-screen.
 * If the parent is NULL, returns the empty string.
 */
char *
ammonite_file_get_parent_uri (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    if (ammonite_file_is_self_owned (file))
    {
        /* Callers expect an empty string, not a NULL. */
        return g_strdup ("");
    }

    return ammonite_directory_get_uri (file->details->directory);
}

static char *
get_uri_scheme (AmmoniteFileInfo *file_info)
{
    return ammonite_file_get_uri_scheme (AMMONITE_FILE (file_info));
}

char *
ammonite_file_get_uri_scheme (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    if (file->details->directory == NULL)
    {
        return NULL;
    }

    g_autoptr (GFile) location = ammonite_directory_get_location (file->details->directory);

    if (location == NULL)
    {
        return NULL;
    }

    return g_file_get_uri_scheme (location);
}

static char *
get_mime_type (AmmoniteFileInfo *file_info)
{
    return g_strdup (ammonite_file_get_mime_type (AMMONITE_FILE (file_info)));
}

/**
 * ammonite_file_get_mime_type
 *
 * Return this file's default mime type.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: (transfer none): The mime type.
 *
 **/
const char *
ammonite_file_get_mime_type (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    return file->details->mime_type != NULL ?
           file->details->mime_type :
           "application/octet-stream";
}

static gboolean
is_mime_type (AmmoniteFileInfo *file_info,
              const char       *mime_type)
{
    return ammonite_file_is_mime_type (AMMONITE_FILE (file_info), mime_type);
}

/**
 * ammonite_file_is_mime_type
 *
 * Check whether a file is of a particular MIME type, or inherited
 * from it.
 * @file: AmmoniteFile representing the file in question.
 * @mime_type: The MIME-type string to test (e.g. "text/plain")
 *
 * Return value: TRUE if @mime_type exactly matches the
 * file's MIME type.
 *
 **/
gboolean
ammonite_file_is_mime_type (AmmoniteFile *file,
                            const char   *mime_type)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);
    g_return_val_if_fail (mime_type != NULL, FALSE);

    if (file->details->mime_type == NULL)
    {
        return FALSE;
    }

    return g_content_type_is_a (file->details->mime_type, mime_type);
}

static gboolean
is_directory (AmmoniteFileInfo *file_info)
{
    return ammonite_file_is_directory (AMMONITE_FILE (file_info));
}

/**
 * ammonite_file_is_directory
 *
 * Check if this file is a directory.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: TRUE if @file is a directory.
 *
 **/
gboolean
ammonite_file_is_directory (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return ammonite_file_get_file_type (file) == G_FILE_TYPE_DIRECTORY;
}

static void
add_emblem (AmmoniteFileInfo *file_info,
            const char       *emblem_name)
{
    AmmoniteFile *file = AMMONITE_FILE (file_info);

    file->details->extension_emblems = g_list_prepend (file->details->extension_emblems,
                                                       g_strdup (emblem_name));

    ammonite_file_changed (file);
}

static char *
get_string_attribute (AmmoniteFileInfo *file_info,
                      const char       *attribute_name)
{
    return ammonite_file_get_string_attribute (AMMONITE_FILE (file_info), attribute_name);
}

static void
add_string_attribute (AmmoniteFileInfo *file_info,
                      const char       *attribute_name,
                      const char       *value)
{
    AmmoniteFile *file = AMMONITE_FILE (file_info);

    /* Lazily create hashtable */
    if (file->details->extension_attributes == NULL)
    {
        file->details->extension_attributes = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                                     NULL,
                                                                     (GDestroyNotify) g_free);
    }
    g_hash_table_insert (file->details->extension_attributes,
                         GINT_TO_POINTER (g_quark_from_string (attribute_name)),
                         g_strdup (value));

    ammonite_file_changed (file);
}

static void
invalidate_extension_info (AmmoniteFileInfo *file_info)
{
    AmmoniteFile *file = AMMONITE_FILE (file_info);

    ammonite_file_invalidate_attributes (file, AMMONITE_ATTRIBUTE_EXTENSION_INFO);
}

static char *
get_activation_uri (AmmoniteFileInfo *file_info)
{
    return ammonite_file_get_activation_uri (AMMONITE_FILE (file_info));
}

/* Return the uri associated with the passed-in file, which may not be
 * the actual uri if the file is an desktop file or a ammonite
 * xml link file.
 */
char *
ammonite_file_get_activation_uri (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    if (file->details->activation_uri != NULL)
    {
        return g_strdup (file->details->activation_uri);
    }

    return ammonite_file_get_uri (file);
}

static GFileType
get_file_type (AmmoniteFileInfo *file_info)
{
    return ammonite_file_get_file_type (AMMONITE_FILE (file_info));
}

/**
 * ammonite_file_get_file_type
 *
 * Return this file's type.
 * @file: AmmoniteFile representing the file in question.
 *
 * Returns: The type.
 *
 **/
GFileType
ammonite_file_get_file_type (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), G_FILE_TYPE_UNKNOWN);

    return file->details->type;
}

static GFile *
get_location (AmmoniteFileInfo *file_info)
{
    return ammonite_file_get_location (AMMONITE_FILE (file_info));
}

GFile *
ammonite_file_get_location (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    g_autoptr (GFile) location = ammonite_directory_get_location (file->details->directory);

    if (ammonite_file_is_self_owned (file))
    {
        return g_steal_pointer (&location);
    }

    return g_file_get_child (location, file->details->name);
}

static GFile *
get_parent_location (AmmoniteFileInfo *file_info)
{
    return ammonite_file_get_parent_location (AMMONITE_FILE (file_info));
}

GFile *
ammonite_file_get_parent_location (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    if (ammonite_file_is_self_owned (file))
    {
        return NULL;
    }

    return ammonite_directory_get_location (file->details->directory);
}

static AmmoniteFileInfo *
get_parent_info (AmmoniteFileInfo *file_info)
{
    return AMMONITE_FILE_INFO (ammonite_file_get_parent (AMMONITE_FILE (file_info)));
}

AmmoniteFile *
ammonite_file_get_parent (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    if (ammonite_file_is_self_owned (file))
    {
        return NULL;
    }

    return ammonite_directory_get_corresponding_file (file->details->directory);
}

static GMount *
get_mount (AmmoniteFileInfo *file_info)
{
    return ammonite_file_get_mount (AMMONITE_FILE (file_info));
}

GMount *
ammonite_file_get_mount (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), NULL);

    return (file->details->mount != NULL) ? g_object_ref (file->details->mount) : NULL;
}

static gboolean
can_write (AmmoniteFileInfo *file_info)
{
    return ammonite_file_can_write (AMMONITE_FILE (file_info));
}

/**
 * ammonite_file_can_write:
 *
 * Check whether the user is allowed to write to this file.
 *
 * @file: The file to check.
 *
 * Return value: FALSE if the user is definitely not allowed to write
 * to the file. If the user has write permission, or
 * the code can't tell whether the user has write permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
ammonite_file_can_write (AmmoniteFile *file)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (file), FALSE);

    return file->details->can_write;
}

static void
ammonite_file_info_iface_init (AmmoniteFileInfoInterface *iface)
{
    iface->is_gone = is_gone;

    iface->get_name = get_name;
    iface->get_uri = get_uri;
    iface->get_parent_uri = get_parent_uri;
    iface->get_uri_scheme = get_uri_scheme;

    iface->get_mime_type = get_mime_type;
    iface->is_mime_type = is_mime_type;
    iface->is_directory = is_directory;

    iface->add_emblem = add_emblem;
    iface->get_string_attribute = get_string_attribute;
    iface->add_string_attribute = add_string_attribute;
    iface->invalidate_extension_info = invalidate_extension_info;

    iface->get_activation_uri = get_activation_uri;

    iface->get_file_type = get_file_type;
    iface->get_location = get_location;
    iface->get_parent_location = get_parent_location;
    iface->get_parent_info = get_parent_info;
    iface->get_mount = get_mount;
    iface->can_write = can_write;
}
