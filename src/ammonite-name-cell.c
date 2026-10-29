/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-name-cell.h"

#include <adwaita.h>

#include "ammonite-directory.h"
#include "ammonite-file.h"
#include "ammonite-file-utilities.h"
#include "ammonite-image.h"
#include "ammonite-ui-utilities.h"
#include "ammonite-view-item.h"

#define SPINNER_DELAY_MS 200

struct _AmmoniteNameCell
{
    AmmoniteViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GQuark path_attribute_q;
    GFile *file_path_base_location;

    GtkWidget *top_child;
    GtkWidget *expander;
    GtkWidget *content;
    GtkWidget *fixed_height_box;
    GtkWidget *spinner;
    GtkWidget *icon;
    GtkWidget *emblems_box;
    GtkWidget *snippet_button;
    GtkLabel *snippet;
    GtkWidget *path;

    gboolean show_snippet;
    gboolean in_file_change;
    guint loading_timeout_id;
};

G_DEFINE_FINAL_TYPE (AmmoniteNameCell, ammonite_name_cell, AMMONITE_TYPE_VIEW_CELL)

static gchar *
get_path_text (AmmoniteFile *file,
               GQuark        path_attribute_q,
               GFile        *base_location)
{
    if (path_attribute_q == 0)
    {
        return NULL;
    }

    g_autofree gchar *path = ammonite_file_get_string_attribute_q (file, path_attribute_q);
    g_autoptr (GFile) dir_location = g_file_new_for_commandline_arg (path);

    if (base_location != NULL && g_file_equal (base_location, dir_location))
    {
        /* Only occurs when search result is
         * a direct child of the base location
         */
        return NULL;
    }

    g_autoptr (GFile) home_location = g_file_new_for_path (g_get_home_dir ());

    if (g_file_equal (dir_location, home_location))
    {
        return ammonite_compute_title_for_location (home_location);
    }

    g_autoptr (GFile) root_location = g_file_new_for_path ("/");
    GFile *relative_location_base = base_location;

    if (relative_location_base == NULL)
    {
        /* Only occurs in Recent, Starred and Trash. */
        relative_location_base = home_location;
    }

    if (!g_file_equal (relative_location_base, root_location) &&
        g_file_has_prefix (dir_location, relative_location_base))
    {
        g_autofree gchar *relative_path = NULL;
        g_autofree gchar *display_name = NULL;

        relative_path = g_file_get_relative_path (relative_location_base, dir_location);
        display_name = g_filename_display_name (relative_path);

        /* Ensure a trailing slash to emphasize it is a directory */
        if (g_str_has_suffix (display_name, G_DIR_SEPARATOR_S))
        {
            return g_steal_pointer (&display_name);
        }

        return g_strconcat (display_name, G_DIR_SEPARATOR_S, NULL);
    }

    return g_steal_pointer (&path);
}

static void
update_labels (AmmoniteNameCell *self)
{
    g_autoptr (AmmoniteViewItem) item = NULL;
    AmmoniteFile *file;
    g_autofree gchar *path_text = NULL;
    const gchar *fts_snippet = NULL;

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = ammonite_view_item_get_file (item);

    path_text = get_path_text (file,
                               self->path_attribute_q,
                               self->file_path_base_location);
    if (self->show_snippet)
    {
        fts_snippet = ammonite_file_get_search_fts_snippet (file);
    }

    gtk_label_set_text (GTK_LABEL (self->path), path_text);
    if (fts_snippet != NULL &&
        !g_str_equal (gtk_label_get_label (self->snippet), fts_snippet))
    {
        gtk_label_set_markup (self->snippet, fts_snippet);
    }

    gtk_widget_set_visible (self->path, (path_text != NULL));
    gtk_widget_set_visible (self->snippet_button, (fts_snippet != NULL));
}

static void
update_icon (AmmoniteNameCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    gboolean is_cut;

    g_return_if_fail (item != NULL);

    g_object_get (item, "is-cut", &is_cut, NULL);

    if (is_cut)
    {
        gtk_widget_set_visible (self->icon, FALSE);
        gtk_widget_remove_css_class (self->icon, "hidden-file");

        return;
    }

    guint icon_size;
    g_autoptr (GdkPaintable) icon_paintable = NULL;
    AmmoniteFile *file = ammonite_view_item_get_file (item);
    gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    AmmoniteFileIconFlags flags = AMMONITE_FILE_ICON_FLAGS_NONE;
    gboolean show_thumbnail;

    g_object_get (self, "icon-size", &icon_size, NULL);
    icon_paintable = ammonite_file_get_icon_paintable (file, icon_size, scale_factor, flags);
    show_thumbnail = icon_size >= AMMONITE_THUMBNAIL_MINIMUM_ICON_SIZE &&
                     ammonite_file_should_show_thumbnail (file);

    gtk_widget_set_visible (self->icon, TRUE);
    ammonite_image_set_size (AMMONITE_IMAGE (self->icon), icon_size);
    ammonite_image_set_fallback (AMMONITE_IMAGE (self->icon), icon_paintable);

    if (self->in_file_change ||
        !show_thumbnail)
    {
        ammonite_image_set_source (AMMONITE_IMAGE (self->icon), NULL);
    }

    if (show_thumbnail)
    {
        g_autoptr (GFile) location = ammonite_file_get_location (file);

        ammonite_image_set_source (AMMONITE_IMAGE (self->icon), location);
    }

    if (ammonite_file_is_hidden_file (file))
    {
        gtk_widget_add_css_class (self->icon, "hidden-file");
    }
    else
    {
        gtk_widget_remove_css_class (self->icon, "hidden-file");
    }
}

static void
update_emblems (AmmoniteNameCell *self)
{
    g_autoptr (AmmoniteViewItem) item = NULL;
    AmmoniteFile *file;
    GtkWidget *child;
    GtkIconTheme *theme;
    g_autolist (GIcon) emblems = NULL;

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = ammonite_view_item_get_file (item);

    /* Remove old emblems. */
    while ((child = gtk_widget_get_first_child (self->emblems_box)) != NULL)
    {
        gtk_box_remove (GTK_BOX (self->emblems_box), child);
    }

    theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    emblems = ammonite_file_get_emblem_icons (file);
    for (GList *l = emblems; l != NULL; l = l->next)
    {
        if (!gtk_icon_theme_has_gicon (theme, l->data))
        {
            g_autofree gchar *icon_string = g_icon_to_string (l->data);
            g_warning ("Failed to add emblem. “%s” not found in the icon theme",
                       icon_string);
            continue;
        }

        gtk_box_append (GTK_BOX (self->emblems_box),
                        gtk_image_new_from_gicon (l->data));
    }
}

static void
on_file_changed (AmmoniteNameCell *self)
{
    self->in_file_change = TRUE;

    update_icon (self);
    update_labels (self);
    update_emblems (self);

    self->in_file_change = FALSE;
}

static void
on_icon_size_changed (AmmoniteNameCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));

    if (item == NULL)
    {
        /* Cell is not bound to an item yet. Do nothing. */
        return;
    }

    update_icon (self);
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
on_item_drag_accept_changed (AmmoniteNameCell *self)
{
    gboolean drag_accept;
    g_autoptr (AmmoniteViewItem) item = NULL;
    GtkWidget *list_row = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (self)));

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_object_get (item, "drag-accept", &drag_accept, NULL);
    if (drag_accept)
    {
        gtk_widget_set_state_flags (list_row, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
    }
    else
    {
        gtk_widget_unset_state_flags (list_row, GTK_STATE_FLAG_DROP_ACTIVE);
    }
}

static void
on_loading_timeout (gpointer user_data)
{
    AmmoniteNameCell *self = AMMONITE_NAME_CELL (user_data);
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    gboolean is_loading = ammonite_view_item_get_loading (item);

    self->loading_timeout_id = 0;

    if (is_loading)
    {
        gtk_widget_set_visible (self->spinner, TRUE);
    }
}

static void
on_item_is_loading_changed (AmmoniteNameCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    gboolean is_loading = ammonite_view_item_get_loading (item);

    g_clear_handle_id (&self->loading_timeout_id, g_source_remove);

    if (is_loading)
    {
        self->loading_timeout_id = g_timeout_add_once (SPINNER_DELAY_MS,
                                                       on_loading_timeout,
                                                       self);
    }
    else
    {
        g_autoptr (AmmoniteDirectory) directory = ammonite_directory_get_for_file (ammonite_view_item_get_file (item));

        gtk_tree_expander_set_hide_expander (GTK_TREE_EXPANDER (self->expander),
                                             !ammonite_directory_is_not_empty (directory));

        gtk_widget_set_visible (self->spinner, FALSE);
    }
}

static void
popover_show_cb (AmmoniteNameCell *self)
{
    const char *label = gtk_label_get_text (self->snippet);

    gtk_accessible_announce (GTK_ACCESSIBLE (self),
                             label,
                             GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
}

static gboolean
on_label_query_tooltip (GtkWidget  *widget,
                        int         x,
                        int         y,
                        gboolean    keyboard_tip,
                        GtkTooltip *tooltip,
                        gpointer    user_data)
{
    GtkLabel *label = GTK_LABEL (widget);

    if (pango_layout_is_ellipsized (gtk_label_get_layout (label)))
    {
        gtk_tooltip_set_text (tooltip, gtk_label_get_text (label));

        return TRUE;
    }

    return FALSE;
}

static void
ammonite_name_cell_init (AmmoniteNameCell *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self, "notify::icon-size",
                      G_CALLBACK (on_icon_size_changed), NULL);
    g_signal_connect (self, "notify::scale-factor", G_CALLBACK (on_icon_size_changed), NULL);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (AMMONITE_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::drag-accept",
                                    (GCallback) on_item_drag_accept_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::is-cut",
                                    (GCallback) update_icon, self);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::loading",
                                    (GCallback) on_item_is_loading_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    (GCallback) on_file_changed, self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             (GCallback) on_file_changed, self,
                             G_CONNECT_SWAPPED);

    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);
}

static void
ammonite_name_cell_dispose (GObject *object)
{
    AmmoniteNameCell *self = (AmmoniteNameCell *) object;

    g_clear_handle_id (&self->loading_timeout_id, g_source_remove);
    g_clear_object (&self->item_signal_group);

    gtk_widget_dispose_template (GTK_WIDGET (self), AMMONITE_TYPE_NAME_CELL);

    G_OBJECT_CLASS (ammonite_name_cell_parent_class)->dispose (object);
}

static void
ammonite_name_cell_finalize (GObject *object)
{
    AmmoniteNameCell *self = (AmmoniteNameCell *) object;

    g_clear_object (&self->file_path_base_location);
    G_OBJECT_CLASS (ammonite_name_cell_parent_class)->finalize (object);
}

static void
snapshot (GtkWidget   *widget,
          GtkSnapshot *snapshot)
{
    AmmoniteNameCell *self = AMMONITE_NAME_CELL (widget);
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    gboolean is_cut;

    g_object_get (item, "is-cut", &is_cut, NULL);

    if (is_cut)
    {
        graphene_rect_t dash_bounds;

        if (gtk_widget_compute_bounds (self->fixed_height_box, widget, &dash_bounds))
        {
            AdwStyleManager *style_manager = adw_style_manager_get_default ();
            gboolean is_high_contrast = adw_style_manager_get_high_contrast (style_manager);
            guint icon_size;
            GdkRGBA color, dashed_border_color, icon_color;
            gboolean use_small_icon;
            const double border_opacity = is_high_contrast ? 0.5 : 0.15;
            const double dim_opacity = is_high_contrast ? 0.9 : 0.55;
            graphene_rect_t icon_bounds = dash_bounds;

            g_object_get (self, "icon-size", &icon_size, NULL);
            use_small_icon = icon_size <= AMMONITE_LIST_ICON_SIZE_MEDIUM;
            gtk_widget_get_color (widget, &color);

            if (icon_size >= AMMONITE_THUMBNAIL_MINIMUM_ICON_SIZE)
            {
                dashed_border_color = color;
                dashed_border_color.alpha *= border_opacity;
                ammonite_ui_draw_icon_dashed_border (snapshot, &dash_bounds, dashed_border_color);

                graphene_rect_inset_r (&dash_bounds,
                                       0.2 * dash_bounds.size.width,
                                       0.2 * dash_bounds.size.height,
                                       &icon_bounds);
            }

            icon_color = color;
            icon_color.alpha *= dim_opacity;
            ammonite_ui_draw_svg (snapshot, ammonite_ui_get_cut_svg (!use_small_icon),
                                  &icon_bounds, icon_color);
        }
        else
        {
            g_warning ("Could not compute icon bounds in cell coordinates.");
        }
    }

    GTK_WIDGET_CLASS (ammonite_name_cell_parent_class)->snapshot (widget, snapshot);
}

static void
ammonite_name_cell_class_init (AmmoniteNameCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ammonite_name_cell_dispose;
    object_class->finalize = ammonite_name_cell_finalize;

    widget_class->snapshot = snapshot;

    g_type_ensure (AMMONITE_TYPE_IMAGE);

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
    gtk_widget_class_set_template_from_resource (widget_class, "/org/ammonite/ammonite/ui/ammonite-name-cell.ui");

    /* Needs to add the direct child of the template widget to dispose it since
     * a plain GtkWidget doesn't dispose it's child automatically. */
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, top_child);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, expander);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, content);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, fixed_height_box);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, spinner);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, icon);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, emblems_box);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, snippet_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, snippet);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNameCell, path);

    gtk_widget_class_bind_template_callback (widget_class, on_label_query_tooltip);
    gtk_widget_class_bind_template_callback (widget_class, popover_show_cb);
}

AmmoniteViewCell *
ammonite_name_cell_new (AmmoniteListBase *view)
{
    return AMMONITE_VIEW_CELL (g_object_new (AMMONITE_TYPE_NAME_CELL,
                                             "view", view,
                                             NULL));
}

void
ammonite_name_cell_set_path (AmmoniteNameCell *self,
                             GQuark            path_attribute_q,
                             GFile            *base_location)
{
    self->path_attribute_q = path_attribute_q;
    g_set_object (&self->file_path_base_location, base_location);
}

void
ammonite_name_cell_set_show_snippet (AmmoniteNameCell *self,
                                     gboolean          show)
{
    self->show_snippet = show;
}

GtkTreeExpander *
ammonite_name_cell_get_expander (AmmoniteNameCell *self)
{
    return GTK_TREE_EXPANDER (self->expander);
}

GtkWidget *
ammonite_name_cell_get_content (AmmoniteNameCell *self)
{
    return self->content;
}
