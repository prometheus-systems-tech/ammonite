/*
 * ammonite-dnd.h - Common Drag & drop handling code
 *
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Pavel Cisler <pavel@eazel.com>,
 *          Ettore Perazzoli <ettore@gnu.org>
 */

#include <glib/gi18n.h>

#include "ammonite-directory.h"
#include "ammonite-dnd.h"
#include "ammonite-file.h"
#include "ammonite-file-utilities.h"
#include "ammonite-files-view.h"
#include "ammonite-files-view-dnd.h"
#include "ammonite-scheme.h"
#include "ammonite-tag-manager.h"
#include "ammonite-ui-utilities.h"

static gboolean
check_same_fs (AmmoniteFile *file1,
               AmmoniteFile *file2)
{
    if (file1 == NULL || file2 == NULL)
    {
        return FALSE;
    }

    const char *id1 = ammonite_file_get_filesystem_id (file1);
    const char *id2 = ammonite_file_get_filesystem_id (file2);

    return id1 != NULL && id2 != NULL && strcmp (id1, id2) == 0;
}

static gboolean
source_is_deletable (GFile *file)
{
    AmmoniteFile *naut_file;
    gboolean ret;

    /* if there's no a cached AmmoniteFile, it returns NULL */
    naut_file = ammonite_file_get (file);
    if (naut_file == NULL)
    {
        return FALSE;
    }

    ret = ammonite_file_can_delete (naut_file);
    ammonite_file_unref (naut_file);

    return ret;
}

#if 0 && AMMONITE_DND_NEEDS_GTK4_REIMPLEMENTATION
static void
append_drop_action_menu_item (GtkWidget          *menu,
                              const char         *text,
                              GdkDragAction       action,
                              gboolean            sensitive,
                              DropActionMenuData *damd)
{
    GtkWidget *menu_item;

    menu_item = gtk_button_new_with_mnemonic (text);
    gtk_widget_set_sensitive (menu_item, sensitive);
    gtk_box_append (GTK_BOX (menu), menu_item);

    gtk_widget_add_css_class (menu_item, "flat");

    g_object_set_data (G_OBJECT (menu_item),
                       "action",
                       GINT_TO_POINTER (action));

    g_signal_connect (menu_item, "clicked",
                      G_CALLBACK (drop_action_activated_callback),
                      damd);

    gtk_widget_show (menu_item);
}
#endif
/* Pops up a menu of actions to perform on dropped files */
GdkDragAction
ammonite_drag_drop_action_ask (GtkWidget     *widget,
                               GdkDragAction  actions)
{
#if 0
    GtkWidget *popover;
    GtkWidget *menu;
    GtkWidget *menu_item;
    DropActionMenuData damd;

    /* Create the menu and set the sensitivity of the items based on the
     * allowed actions.
     */
    popover = gtk_popover_new (widget);
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_TOP);

    menu = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top (menu, 6);
    gtk_widget_set_margin_bottom (menu, 6);
    gtk_widget_set_margin_start (menu, 6);
    gtk_widget_set_margin_end (menu, 6);
    gtk_popover_set_child (GTK_POPOVER (popover), menu);
    gtk_widget_show (menu);

    append_drop_action_menu_item (menu, _("_Move Here"),
                                  GDK_ACTION_MOVE,
                                  (actions & GDK_ACTION_MOVE) != 0,
                                  &damd);

    append_drop_action_menu_item (menu, _("_Copy Here"),
                                  GDK_ACTION_COPY,
                                  (actions & GDK_ACTION_COPY) != 0,
                                  &damd);

    append_drop_action_menu_item (menu, _("_Link Here"),
                                  GDK_ACTION_LINK,
                                  (actions & GDK_ACTION_LINK) != 0,
                                  &damd);

    menu_item = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_append (GTK_BOX (menu), menu_item);
    gtk_widget_show (menu_item);

    append_drop_action_menu_item (menu, _("Cancel"), 0, TRUE, &damd);

    damd.chosen = 0;
    damd.loop = g_main_loop_new (NULL, FALSE);

    g_signal_connect (popover, "closed",
                      G_CALLBACK (menu_deactivate_callback),
                      &damd);

    gtk_grab_add (popover);

    /* We don't have pointer coords here. Just pick the center of the widget. */
    gtk_popover_set_pointing_to (GTK_POPOVER (popover),
                                 &(GdkRectangle){ .x = 0.5 * gtk_widget_get_width (widget),
                                                  .y = 0.5 * gtk_widget_get_height (widget),
                                                  .width = 0, .height = 0 });

    gtk_popover_popup (GTK_POPOVER (popover));

    g_main_loop_run (damd.loop);

    gtk_grab_remove (popover);

    g_main_loop_unref (damd.loop);

    g_object_ref_sink (popover);
    g_object_unref (popover);

    return damd.chosen;
#endif
    return 0;
}

GdkDragAction
ammonite_dnd_get_preferred_action (AmmoniteFile *target_file,
                                   GFile        *dropped)
{
    g_autoptr (AmmoniteDirectory) directory = NULL;
    g_autoptr (GFile) target_location = NULL;
    g_autoptr (AmmoniteFile) dropped_file = NULL;
    gboolean same_fs;
    gboolean source_deletable;

    g_return_val_if_fail (AMMONITE_IS_FILE (target_file), 0);
    /* With non-local drops, it's normal for dropped to initially be NULL */
    g_return_val_if_fail (dropped == NULL || G_IS_FILE (dropped), 0);

    target_location = ammonite_file_get_location (target_file);
    if (dropped != NULL && g_file_equal (target_location, dropped))
    {
        return 0;
    }

    /* First check target imperatives */
    directory = ammonite_directory_get_for_file (target_file);

    if (ammonite_is_file_roller_installed () &&
        ammonite_file_is_archive (target_file))
    {
        return GDK_ACTION_COPY;
    }
    else if (ammonite_file_is_starred_location (target_file))
    {
        if (ammonite_tag_manager_can_star_contents (ammonite_tag_manager_get (), dropped))
        {
            return GDK_ACTION_COPY;
        }
        else
        {
            return 0;
        }
    }
    else if (!ammonite_file_is_directory (target_file) ||
             !ammonite_file_can_write (target_file) ||
             !ammonite_directory_is_editable (directory))
    {
        /* No other file type other than archives and directories currently
         * accepts drops */
        return 0;
    }
    else if (ammonite_file_is_in_trash (target_file))
    {
        return GDK_ACTION_MOVE;
    }

    if (dropped == NULL)
    {
        return GDK_ACTION_COPY;
    }

    if (g_file_has_uri_scheme (dropped, SCHEME_TRASH))
    {
        return GDK_ACTION_MOVE;
    }

    dropped_file = ammonite_file_get (dropped);
    same_fs = check_same_fs (target_file, dropped_file);
    source_deletable = source_is_deletable (dropped);
    if (same_fs && source_deletable)
    {
        return GDK_ACTION_MOVE;
    }

    return GDK_ACTION_COPY;
}

gboolean
ammonite_dnd_perform_drop (AmmoniteFilesView *view,
                           const GValue      *value,
                           GdkDragAction      action,
                           GFile             *target_location)
{
    g_autofree gchar *target_uri = g_file_get_uri (target_location);

    if (!gdk_drag_action_is_unique (action))
    {
        /* TODO: Implement */
        return FALSE;
    }
    else if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    {
        ammonite_files_view_handle_text_drop (view,
                                              g_value_get_string (value),
                                              target_uri, action);
        return TRUE;
    }
    else if (G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
    {
        g_autofree char *dest_uri = g_file_get_uri (target_location);
        GdkTexture *texture = g_value_get_object (value);

        ammonite_file_view_save_image_from_texture (view, texture, dest_uri, _("Dropped Image"));
    }
    else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GSList *source_file_list = g_value_get_boxed (value);
        GList *source_uri_list = NULL;

        if (source_file_list == NULL)
        {
            return FALSE;
        }

        for (GSList *l = source_file_list; l != NULL; l = l->next)
        {
            source_uri_list = g_list_prepend (source_uri_list, g_file_get_uri (l->data));
        }
        source_uri_list = g_list_reverse (source_uri_list);

        ammonite_files_view_drop_proxy_received_uris (view,
                                                      source_uri_list,
                                                      target_uri,
                                                      action);
        g_list_free_full (source_uri_list, g_free);

        return TRUE;
    }

    return FALSE;
}

#define MAX_DRAWN_DRAG_ICONS 4

GdkPaintable *
get_paintable_for_drag_selection (GList     *selection,
                                  GtkWidget *widget,
                                  int        scale)
{
    g_return_val_if_fail (AMMONITE_IS_FILE (selection->data), NULL);

    g_autoqueue (GdkPaintable) icons = g_queue_new ();
    AmmoniteFileIconFlags flags = AMMONITE_FILE_ICON_FLAGS_USE_THUMBNAILS;
    guint icon_size = AMMONITE_DRAG_SURFACE_ICON_SIZE;

    for (AmmoniteFileList *l = selection;
         l != NULL && g_queue_get_length (icons) < MAX_DRAWN_DRAG_ICONS;
         l = l->next)
    {
        GdkPaintable *icon = ammonite_file_get_icon_paintable (l->data,
                                                               icon_size,
                                                               scale,
                                                               flags);

        g_queue_push_tail (icons, icon);
    }

    return ammonite_ui_draw_stacked_icons (icons, icon_size, widget, g_list_length (selection));
}
