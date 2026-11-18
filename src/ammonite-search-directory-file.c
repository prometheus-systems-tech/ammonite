/*
 * SPDX-FileCopyrightText: 2005 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 */

#include "ammonite-search-directory-file.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "ammonite-enums.h"
#include "ammonite-file-private.h"
#include "ammonite-file-utilities.h"
#include "ammonite-keyfile-metadata.h"
#include "ammonite-query.h"
#include "ammonite-search-directory.h"

struct _AmmoniteSearchDirectoryFile
{
    AmmoniteFile parent_instance;

    gchar *metadata_filename;
};

G_DEFINE_FINAL_TYPE (AmmoniteSearchDirectoryFile, ammonite_search_directory_file, AMMONITE_TYPE_FILE);


static void
search_directory_file_monitor_add (AmmoniteFile       *file,
                                   gconstpointer       client,
                                   AmmoniteAttributes  attributes)
{
    /* No need for monitoring, we always emit changed when files
     *  are added/removed, and no other metadata changes */

    /* Update display name, in case this didn't happen yet */
    ammonite_search_directory_file_update_display_name (AMMONITE_SEARCH_DIRECTORY_FILE (file));
}

static void
search_directory_file_monitor_remove (AmmoniteFile  *file,
                                      gconstpointer  client)
{
    /* Do nothing here, we don't have any monitors */
}

static void
search_directory_file_call_when_ready (AmmoniteFile         *file,
                                       AmmoniteAttributes    attributes,
                                       AmmoniteFileCallback  callback,
                                       gpointer              callback_data)
{
    /* Update display name, in case this didn't happen yet */
    ammonite_search_directory_file_update_display_name (AMMONITE_SEARCH_DIRECTORY_FILE (file));

    if (callback != NULL)
    {
        /* All data for directory-as-file is always up to date */
        (*callback)(file, callback_data);
    }
}

static void
search_directory_file_cancel_call_when_ready (AmmoniteFile         *file,
                                              AmmoniteFileCallback  callback,
                                              gpointer              callback_data)
{
    /* Do nothing here, we don't have any pending calls */
}

static gboolean
search_directory_file_check_if_ready (AmmoniteFile       *file,
                                      AmmoniteAttributes  attributes)
{
    return TRUE;
}

static gboolean
search_directory_file_get_item_count (AmmoniteFile *file,
                                      guint        *count,
                                      gboolean     *count_unreadable)
{
    GList *file_list;

    if (count)
    {
        AmmoniteDirectory *directory;

        directory = ammonite_file_get_directory (file);
        file_list = ammonite_directory_get_file_list (directory);

        *count = g_list_length (file_list);

        ammonite_file_list_free (file_list);
    }

    return TRUE;
}

static AmmoniteRequestStatus
search_directory_file_get_deep_counts (AmmoniteFile *file,
                                       guint        *directory_count,
                                       guint        *file_count,
                                       guint        *unreadable_directory_count,
                                       goffset      *total_size)
{
    AmmoniteDirectory *directory;
    AmmoniteFile *dir_file;
    GList *file_list, *l;
    guint dirs, files;
    GFileType type;

    directory = ammonite_file_get_directory (file);
    file_list = ammonite_directory_get_file_list (directory);

    dirs = files = 0;
    for (l = file_list; l != NULL; l = l->next)
    {
        dir_file = AMMONITE_FILE (l->data);
        type = ammonite_file_get_file_type (dir_file);
        if (type == G_FILE_TYPE_DIRECTORY)
        {
            dirs++;
        }
        else
        {
            files++;
        }
    }

    if (directory_count != NULL)
    {
        *directory_count = dirs;
    }
    if (file_count != NULL)
    {
        *file_count = files;
    }
    if (unreadable_directory_count != NULL)
    {
        *unreadable_directory_count = 0;
    }
    if (total_size != NULL)
    {
        /* FIXME: Maybe we want to calculate this? */
        *total_size = 0;
    }

    ammonite_file_list_free (file_list);

    return AMMONITE_REQUEST_DONE;
}

static void
search_directory_file_set_metadata (AmmoniteFile         *file,
                                    const char           *key,
                                    AmmoniteMetadataType  type,
                                    gpointer              value)
{
    AmmoniteSearchDirectoryFile *search_file;

    search_file = AMMONITE_SEARCH_DIRECTORY_FILE (file);

    if (type == AMMONITE_METADATA_STRING)
    {
        ammonite_keyfile_metadata_set_string (file,
                                              search_file->metadata_filename,
                                              "directory", key, value);
    }
    else if (type == AMMONITE_METADATA_STRINGV)
    {
        ammonite_keyfile_metadata_set_stringv (file,
                                               search_file->metadata_filename,
                                               "directory", key, value);
    }
    else
    {
        g_warn_if_reached ();
    }
}

void
ammonite_search_directory_file_update_display_name (AmmoniteSearchDirectoryFile *search_file)
{
    AmmoniteFile *file;
    AmmoniteDirectory *directory;
    char *display_name;
    gboolean changed;


    display_name = NULL;
    file = AMMONITE_FILE (search_file);
    directory = ammonite_file_get_directory (file);
    if (directory != NULL)
    {
        AmmoniteSearchDirectory *search_dir = AMMONITE_SEARCH_DIRECTORY (directory);
        AmmoniteQuery *query = ammonite_search_directory_get_query (search_dir);

        if (query != NULL)
        {
            g_autofree char *query_text = ammonite_query_get_text (query);

            if (query_text != NULL && query_text[0] != '\0')
            {
                display_name = g_strdup_printf (_("Search for “%s”"), query_text);
            }
        }
    }

    if (display_name == NULL)
    {
        display_name = g_strdup (_("Search"));
    }

    changed = ammonite_file_set_display_name (file, display_name, NULL, TRUE);
    if (changed)
    {
        ammonite_file_emit_changed (file);
    }

    g_free (display_name);
}

static void
ammonite_search_directory_file_init (AmmoniteSearchDirectoryFile *search_file)
{
    AmmoniteFile *file;
    gchar *xdg_dir;

    file = AMMONITE_FILE (search_file);

    xdg_dir = ammonite_get_user_directory ();
    search_file->metadata_filename = g_build_filename (xdg_dir,
                                                       "search-metadata",
                                                       NULL);
    g_free (xdg_dir);

    file->details->got_file_info = TRUE;
    file->details->mime_type = g_ref_string_new_intern ("x-directory/normal");
    file->details->type = G_FILE_TYPE_DIRECTORY;
    file->details->size = 0;

    file->details->file_info_is_up_to_date = TRUE;

    file->details->activation_uri = NULL;

    file->details->directory_count = 0;
    file->details->got_directory_count = TRUE;
    file->details->directory_count_is_up_to_date = TRUE;

    ammonite_file_set_display_name (file, _("Search"), NULL, TRUE);
}

static void
ammonite_search_directory_file_finalize (GObject *object)
{
    AmmoniteSearchDirectoryFile *search_file;

    search_file = AMMONITE_SEARCH_DIRECTORY_FILE (object);

    g_free (search_file->metadata_filename);

    G_OBJECT_CLASS (ammonite_search_directory_file_parent_class)->finalize (object);
}

static void
ammonite_search_directory_file_class_init (AmmoniteSearchDirectoryFileClass *klass)
{
    GObjectClass *object_class;
    AmmoniteFileClass *file_class;

    object_class = G_OBJECT_CLASS (klass);
    file_class = AMMONITE_FILE_CLASS (klass);

    object_class->finalize = ammonite_search_directory_file_finalize;

    file_class->default_file_type = G_FILE_TYPE_DIRECTORY;

    file_class->monitor_add = search_directory_file_monitor_add;
    file_class->monitor_remove = search_directory_file_monitor_remove;
    file_class->call_when_ready = search_directory_file_call_when_ready;
    file_class->cancel_call_when_ready = search_directory_file_cancel_call_when_ready;
    file_class->check_if_ready = search_directory_file_check_if_ready;
    file_class->get_item_count = search_directory_file_get_item_count;
    file_class->get_deep_counts = search_directory_file_get_deep_counts;
    file_class->set_metadata = search_directory_file_set_metadata;
}
