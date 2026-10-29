/*
 * SPDX-FileCopyrightText: 1999, 2000 Free Software Foundaton
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2016 Carlos Soriano <csoriano@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Rebecca Schulman <rebecka@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "ammonite-clipboard.h"
#include "ammonite-file.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

/* The .files member contains elements of type AmmoniteFile. */
struct _AmmoniteClipboard
{
    gboolean cut;
    GList *files;
};

/* Boxed type used to wrap this struct in a clipboard GValue. */
G_DEFINE_BOXED_TYPE (AmmoniteClipboard, ammonite_clipboard,
                     ammonite_clipboard_copy, ammonite_clipboard_free)

static char *
ammonite_clipboard_to_string (AmmoniteClipboard *clip)
{
    GString *uris;
    char *uri;
    GList *l;

    uris = g_string_new (clip->cut ? "cut" : "copy");

    for (l = clip->files; l != NULL; l = l->next)
    {
        uri = ammonite_file_get_uri (l->data);

        g_string_append_c (uris, '\n');
        g_string_append (uris, uri);

        g_free (uri);
    }

    return g_string_free_and_steal (uris);
}

static AmmoniteClipboard *
ammonite_clipboard_from_string (char    *string,
                                GError **error)
{
    AmmoniteClipboard *clip;
    g_auto (GStrv) lines = NULL;
    g_autolist (AmmoniteFile) files = NULL;

    if (string == NULL)
    {
        *error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_DATA, _("Clipboard string cannot be NULL."));
        return NULL;
    }

    lines = g_strsplit (string, "\n", 0);

    if (g_strcmp0 (lines[0], "cut") != 0 && g_strcmp0 (lines[0], "copy") != 0)
    {
        /* Translators: Do not translate 'cut' and 'copy'. These are literal keywords. */
        *error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_DATA, _("Ammonite Clipboard must begin with “cut” or “copy”."));
        return NULL;
    }

    /* Line 0 is "cut" or "copy", so uris start at line 1. */
    for (int i = 1; lines[i] != NULL; i++)
    {
        if (g_strcmp0 (lines[i], "") == 0)
        {
            *error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_DATA, _("Ammonite Clipboard must not have empty lines."));
            return NULL;
        }
        else if (!g_uri_is_valid (lines[i], G_URI_FLAGS_NONE, error))
        {
            return NULL;
        }
        files = g_list_prepend (files, ammonite_file_get_by_uri (lines[i]));
    }

    clip = g_new0 (AmmoniteClipboard, 1);
    files = g_list_reverse (files);
    clip->files = g_steal_pointer (&files);
    clip->cut = g_str_equal (lines[0], "cut");

    return clip;
}

void
ammonite_clipboard_clear_if_colliding_uris (GtkWidget   *widget,
                                            const GList *item_uris)
{
    GdkClipboard *clipboard = gtk_widget_get_clipboard (widget);
    GdkContentFormats *formats = gdk_clipboard_get_formats (clipboard);
    g_auto (GValue) value = G_VALUE_INIT;
    AmmoniteClipboard *ammonite_clipboard;
    GList *clipboard_item_uris;

    if (!gdk_clipboard_is_local (clipboard) ||
        !gdk_content_formats_contain_gtype (formats, AMMONITE_TYPE_CLIPBOARD))
    {
        return;
    }

    g_value_init (&value, AMMONITE_TYPE_CLIPBOARD);
    if (!gdk_content_provider_get_value (gdk_clipboard_get_content (clipboard), &value, NULL))
    {
        return;
    }
    ammonite_clipboard = g_value_get_boxed (&value);
    clipboard_item_uris = ammonite_clipboard_get_uri_list (ammonite_clipboard);

    for (GList *l = (GList *) item_uris; l != NULL; l = l->next)
    {
        if (g_list_find_custom ((GList *) clipboard_item_uris, l->data,
                                (GCompareFunc) g_strcmp0))
        {
            gdk_clipboard_set_content (clipboard, NULL);
            break;
        }
    }

    g_list_free_full (clipboard_item_uris, g_free);
}

/*
 * This asumes the implementation of GTK_TYPE_FILE_LIST is a GSList<GFile>.
 * As of writing this, the API docs don't provide for this assumption.
 */
static GSList *
convert_file_list_to_gdk_file_list (AmmoniteClipboard *clip)
{
    GSList *file_list = NULL;
    for (GList *l = clip->files; l != NULL; l = l->next)
    {
        file_list = g_slist_prepend (file_list,
                                     ammonite_file_get_location (l->data));
    }
    return g_slist_reverse (file_list);
}

static void
ammonite_clipboard_serialize (GdkContentSerializer *serializer)
{
    AmmoniteClipboard *clip;
    g_autofree gchar *str = NULL;
    g_autoptr (GError) error = NULL;

    clip = g_value_get_boxed (gdk_content_serializer_get_value (serializer));

    str = ammonite_clipboard_to_string (clip);

    if (g_output_stream_printf (gdk_content_serializer_get_output_stream (serializer),
                                NULL,
                                gdk_content_serializer_get_cancellable (serializer),
                                &error,
                                "%s", str))
    {
        gdk_content_serializer_return_success (serializer);
    }
    else
    {
        gdk_content_serializer_return_error (serializer, error);
    }
}

static void
ammonite_clipboard_deserialize_finish (GObject      *source,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
    GdkContentDeserializer *deserializer = user_data;
    GOutputStream *output = G_OUTPUT_STREAM (source);
    g_autoptr (GError) error = NULL;
    g_autofree gchar *string = NULL;
    g_autoptr (AmmoniteClipboard) clip = NULL;

    if (g_output_stream_splice_finish (output, result, &error) < 0)
    {
        gdk_content_deserializer_return_error (deserializer, g_steal_pointer (&error));
        return;
    }

    /* write terminating NULL */
    if (g_output_stream_write (output, "", 1, NULL, &error) < 0 ||
        !g_output_stream_close (output, NULL, &error))
    {
        gdk_content_deserializer_return_error (deserializer, g_steal_pointer (&error));
        return;
    }

    string = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (output));

    clip = ammonite_clipboard_from_string (string, &error);

    if (clip == NULL)
    {
        gdk_content_deserializer_return_error (deserializer, g_steal_pointer (&error));
        return;
    }

    g_value_set_boxed (gdk_content_deserializer_get_value (deserializer), clip);
    gdk_content_deserializer_return_success (deserializer);
}

static void
ammonite_clipboard_deserialize (GdkContentDeserializer *deserializer)
{
    g_autoptr (GOutputStream) output = NULL;

    output = g_memory_output_stream_new_resizable ();
    g_output_stream_splice_async (output,
                                  gdk_content_deserializer_get_input_stream (deserializer),
                                  G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                  gdk_content_deserializer_get_priority (deserializer),
                                  gdk_content_deserializer_get_cancellable (deserializer),
                                  ammonite_clipboard_deserialize_finish,
                                  deserializer);
}

/**
 * ammonite_clipboard_peek_files:
 * @clip: The current local clipboard value.
 *
 * Returns: (transfer none): The internal GList of GFile objects.
 */
GList *
ammonite_clipboard_peek_files (AmmoniteClipboard *clip)
{
    return clip->files;
}

/**
 * ammonite_clipboard_get_uri_list:
 * @clip: The current local clipboard value.
 *
 * Returns: (transfer full): A GList of URI strings.
 */
GList *
ammonite_clipboard_get_uri_list (AmmoniteClipboard *clip)
{
    GList *uris = NULL;

    for (GList *l = clip->files; l != NULL; l = l->next)
    {
        uris = g_list_prepend (uris, ammonite_file_get_uri (l->data));
    }

    return g_list_reverse (uris);
}

gboolean
ammonite_clipboard_is_cut (AmmoniteClipboard *clip)
{
    return clip->cut;
}

AmmoniteClipboard *
ammonite_clipboard_copy (AmmoniteClipboard *clip)
{
    AmmoniteClipboard *new_clip = g_new0 (AmmoniteClipboard, 1);

    new_clip->cut = clip->cut;
    new_clip->files = ammonite_file_list_copy (clip->files);

    return new_clip;
}

void
ammonite_clipboard_free (AmmoniteClipboard *clip)
{
    ammonite_file_list_free (clip->files);
    g_free (clip);
}

void
ammonite_clipboard_prepare_for_files (GdkClipboard *clipboard,
                                      GList        *files,
                                      gboolean      cut)
{
    g_autoptr (AmmoniteClipboard) clip = NULL;
    g_autoslist (GFile) file_list = NULL;
    GdkContentProvider *providers[2];
    g_autoptr (GdkContentProvider) provider = NULL;

    clip = g_new (AmmoniteClipboard, 1);
    clip->cut = cut;
    clip->files = ammonite_file_list_copy (files);

    file_list = convert_file_list_to_gdk_file_list (clip);

    providers[0] = gdk_content_provider_new_typed (AMMONITE_TYPE_CLIPBOARD, clip);
    providers[1] = gdk_content_provider_new_typed (GDK_TYPE_FILE_LIST, file_list);

    provider = gdk_content_provider_new_union (providers, 2);
    gdk_clipboard_set_content (clipboard, provider);
}

void
ammonite_clipboard_register (void)
{
    /*
     * While it'is not a public API and the format is not documented, some apps
     * have come to use this atom/mime type to integrate with our clipboard.
     */
    const gchar *ammonite_clipboard_mime_type = "x-special/gnome-copied-files";

    gdk_content_register_serializer (AMMONITE_TYPE_CLIPBOARD,
                                     ammonite_clipboard_mime_type,
                                     ammonite_clipboard_serialize,
                                     NULL,
                                     NULL);
    gdk_content_register_deserializer (ammonite_clipboard_mime_type,
                                       AMMONITE_TYPE_CLIPBOARD,
                                       ammonite_clipboard_deserialize,
                                       NULL,
                                       NULL);
}
