/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-grid-cell.h"

#include <adwaita.h>

#include "ammonite-file.h"
#include "ammonite-global-preferences.h"
#include "ammonite-image.h"
#include "ammonite-tag-manager.h"
#include "ammonite-ui-utilities.h"
#include "ammonite-view-item.h"
#include "ammonite-view-cell.h"

struct _AmmoniteGridCell
{
    AmmoniteViewCell parent_instance;

    GSignalGroup *item_signal_group;

    /* Unowned pointer from the grid view */
    GQuark *caption_attributes;

    GtkWidget *icon;
    GtkWidget *emblems_box;
    GtkWidget *labels_box;
    GtkWidget *caption_labels[AMMONITE_GRID_CELL_N_CAPTIONS];

    gboolean in_file_change;
};

G_DEFINE_FINAL_TYPE (AmmoniteGridCell, ammonite_grid_cell, AMMONITE_TYPE_VIEW_CELL)

static void
update_icon (AmmoniteGridCell *self)
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

    g_autoptr (GdkPaintable) icon_paintable = NULL;
    AmmoniteFile *file = ammonite_view_item_get_file (item);
    guint icon_size;
    gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    AmmoniteFileIconFlags flags = AMMONITE_FILE_ICON_FLAGS_NONE;
    gboolean show_thumbnail = ammonite_file_should_show_thumbnail (file);

    g_object_get (self, "icon-size", &icon_size, NULL);
    icon_paintable = ammonite_file_get_icon_paintable (file, icon_size, scale_factor, flags);
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

static GtkWidget *
caption_widget_new (void)
{
    GtkWidget *caption = gtk_label_new (NULL);

    gtk_widget_set_valign (caption, GTK_ALIGN_START);
    gtk_widget_add_css_class (caption, "caption");
    gtk_widget_add_css_class (caption, "dimmed");

    gtk_label_set_ellipsize (GTK_LABEL (caption), PANGO_ELLIPSIZE_END);
    gtk_label_set_justify (GTK_LABEL (caption), GTK_JUSTIFY_CENTER);
    gtk_label_set_lines (GTK_LABEL (caption), 2);
    gtk_label_set_wrap (GTK_LABEL (caption), TRUE);
    gtk_label_set_wrap_mode (GTK_LABEL (caption), PANGO_WRAP_WORD_CHAR);

    return caption;
}

static void
ensure_captions (AmmoniteGridCell *self)
{
    if (self->caption_labels[0] != NULL)
    {
        return;
    }

    for (guint i = 0; i < AMMONITE_GRID_CELL_N_CAPTIONS; i++)
    {
        self->caption_labels[i] = caption_widget_new ();

        gtk_box_append (GTK_BOX (self->labels_box), self->caption_labels[i]);
    }
}

static void
update_captions (AmmoniteGridCell *self)
{
    g_autoptr (AmmoniteViewItem) item = NULL;
    AmmoniteFile *file;

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = ammonite_view_item_get_file (item);
    for (guint i = 0; i < AMMONITE_GRID_CELL_N_CAPTIONS; i++)
    {
        GQuark attribute_q = self->caption_attributes[i];
        gboolean show_caption = (attribute_q != 0);

        if (!show_caption &&
            self->caption_labels[i] == NULL)
        {
            /* No need to create widgets if they will not be shown. */
            continue;
        }

        ensure_captions (self);

        gtk_widget_set_visible (self->caption_labels[i], show_caption);
        if (show_caption)
        {
            g_autofree gchar *string = NULL;
            string = ammonite_file_get_string_attribute_q (file, attribute_q);
            gtk_label_set_text (GTK_LABEL (self->caption_labels[i]), string);
        }
    }
}

static void
update_emblems (AmmoniteGridCell *self)
{
    g_autoptr (AmmoniteViewItem) item = NULL;
    AmmoniteFile *file;
    GtkWidget *child;
    GtkIconTheme *theme;
    g_autolist (GIcon) emblems = NULL;
    g_autofree gchar *file_uri = NULL;

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = ammonite_view_item_get_file (item);
    file_uri = ammonite_file_get_activation_uri (file);

    /* Remove old emblems. */
    while ((child = gtk_widget_get_first_child (self->emblems_box)) != NULL)
    {
        gtk_box_remove (GTK_BOX (self->emblems_box), child);
    }

    if (ammonite_tag_manager_file_is_starred (ammonite_tag_manager_get (), file_uri))
    {
        gtk_box_append (GTK_BOX (self->emblems_box),
                        gtk_image_new_from_icon_name ("starred-symbolic"));
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
on_file_changed (AmmoniteGridCell *self)
{
    self->in_file_change = TRUE;

    update_icon (self);
    update_emblems (self);
    update_captions (self);

    self->in_file_change = FALSE;
}

static void
on_icon_size_changed (AmmoniteGridCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));

    if (item == NULL)
    {
        /* Cell is not bound to an item yet. Do nothing. */
        return;
    }

    update_icon (self);
    update_captions (self);
    gtk_widget_queue_resize (GTK_WIDGET (self));
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
on_starred_changed (AmmoniteTagManager *tag_manager,
                    GList              *changed_files,
                    gpointer            user_data)
{
    AmmoniteGridCell *self = AMMONITE_GRID_CELL (user_data);
    g_autoptr (AmmoniteViewItem) item = NULL;
    AmmoniteFile *file;

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    if (item == NULL)
    {
        return;
    }

    file = ammonite_view_item_get_file (item);
    if (g_list_find (changed_files, file))
    {
        update_emblems (self);
    }
}

static void
ammonite_grid_cell_dispose (GObject *object)
{
    AmmoniteGridCell *self = (AmmoniteGridCell *) object;

    gtk_widget_dispose_template (GTK_WIDGET (self), AMMONITE_TYPE_GRID_CELL);
    g_clear_object (&self->item_signal_group);

    G_OBJECT_CLASS (ammonite_grid_cell_parent_class)->dispose (object);
}

#define EMBLEMS_BOX_WIDTH 18
#define VERTICAL_PADDING 6

static void
ammonite_grid_cell_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *min_baseline,
                            int            *nat_baseline)
{
    AmmoniteGridCell *self = AMMONITE_GRID_CELL (widget);
    guint icon_size;
    int width, child_min, child_nat;

    g_object_get (self, "icon-size", &icon_size, NULL);
    width = EMBLEMS_BOX_WIDTH + icon_size + EMBLEMS_BOX_WIDTH;

    /* We always expect our layout to fit into this width. Check that
     * this is indeed the case. This also shuts up the
     *     Allocating size to ... without calling gtk_widget_measure().
     *     How does the code know the size to allocate?
     * warning that we get in debug builds of GTK otherwise. The answer
     * to that question is: we know that the icon is a GtkPicture, and
     * we set the icon_paintable of the specific size on it.
     */
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        gtk_widget_measure (self->labels_box, orientation, -1,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > width))
        {
            g_warning ("%s %p unexpectedly doesn't fit into width of %d",
                       gtk_widget_get_name (self->labels_box), self->labels_box, width);
            width = child_min;
        }
        gtk_widget_measure (self->icon, orientation, -1,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > (int) icon_size))
        {
            g_warning ("%s %p unexpectedly doesn't fit into width of %d",
                       gtk_widget_get_name (self->icon), self->icon, icon_size);
            width += child_min - icon_size;
        }
        gtk_widget_measure (self->emblems_box, orientation, -1,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > EMBLEMS_BOX_WIDTH))
        {
            g_warning ("%s %p unexpectedly doesn't fit into width of %d",
                       gtk_widget_get_name (self->emblems_box), self->emblems_box, EMBLEMS_BOX_WIDTH);
            width += 2 * (child_min - EMBLEMS_BOX_WIDTH);
        }
    }
    else /* GTK_ORIENTATION_VERTICAL */
    {
        gtk_widget_measure (self->icon, orientation, icon_size,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > (int) icon_size))
        {
            g_warning ("%s %p unexpectedly doesn't fit into height of %d",
                       gtk_widget_get_name (self->icon), self->icon, icon_size);
        }
        gtk_widget_measure (self->emblems_box, orientation, EMBLEMS_BOX_WIDTH,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > (int) icon_size))
        {
            g_warning ("%s %p unexpectedly doesn't fit into height of %d",
                       gtk_widget_get_name (self->emblems_box), self->emblems_box, icon_size);
        }
    }

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        *natural = *minimum = width;
        *min_baseline = *nat_baseline = -1;
        return;
    }

    gtk_widget_measure (self->labels_box, GTK_ORIENTATION_VERTICAL, width,
                        &child_min, &child_nat,
                        min_baseline, nat_baseline);

    *minimum = icon_size + VERTICAL_PADDING + child_min;
    *natural = icon_size + VERTICAL_PADDING + child_nat;
    if (*min_baseline != -1)
    {
        *min_baseline += icon_size + VERTICAL_PADDING;
    }
    if (*nat_baseline != -1)
    {
        *nat_baseline += icon_size + VERTICAL_PADDING;
    }
}

static void
ammonite_grid_cell_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
    AmmoniteGridCell *self = AMMONITE_GRID_CELL (widget);
    GtkAllocation child_allocation;
    guint icon_size;

    g_object_get (self, "icon-size", &icon_size, NULL);

    /* Put the icon at the top. Center it horizontally, with
     * EMBLEMS_BOX_WIDTH horizontal margins on both sides.
     * Note that the icon is expected to center itself further
     * inside the allocation that we give it.
     */
    child_allocation = (GtkAllocation) {
        EMBLEMS_BOX_WIDTH, 0,
        width - EMBLEMS_BOX_WIDTH * 2, icon_size
    };
    gtk_widget_size_allocate (self->icon, &child_allocation, -1);

    /* Put the emblems_box into the "end" margin. */
    child_allocation.width = EMBLEMS_BOX_WIDTH;
    if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    {
        child_allocation.x = width - EMBLEMS_BOX_WIDTH;
    }
    else
    {
        child_allocation.x = 0;
    }
    gtk_widget_size_allocate (self->emblems_box, &child_allocation, -1);

    /* Give the remaining space to labels box. */
    child_allocation = (GtkAllocation) {
        0, icon_size + VERTICAL_PADDING,
        width, height - (icon_size + VERTICAL_PADDING)
    };
    if (baseline != -1)
    {
        baseline -= icon_size + VERTICAL_PADDING;
    }
    gtk_widget_size_allocate (self->labels_box, &child_allocation, baseline);
}

static void
snapshot (GtkWidget   *widget,
          GtkSnapshot *snapshot)
{
    AmmoniteGridCell *self = AMMONITE_GRID_CELL (widget);
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    gboolean is_cut;

    g_object_get (item, "is-cut", &is_cut, NULL);

    if (is_cut)
    {
        AdwStyleManager *style_manager = adw_style_manager_get_default ();
        gboolean is_high_contrast = adw_style_manager_get_high_contrast (style_manager);
        guint icon_size;
        graphene_rect_t dash_bounds, icon_bounds;
        GdkRGBA color, dashed_border_color, icon_color;
        const double border_opacity = is_high_contrast ? 0.5 : 0.15;
        const double dim_opacity = is_high_contrast ? 0.9 : 0.55;

        g_object_get (self, "icon-size", &icon_size, NULL);
        dash_bounds = GRAPHENE_RECT_INIT (EMBLEMS_BOX_WIDTH, 0, icon_size, icon_size);
        graphene_rect_inset_r (&dash_bounds, 0.2 * icon_size, 0.2 * icon_size, &icon_bounds);
        gtk_widget_get_color (widget, &color);

        dashed_border_color = color;
        dashed_border_color.alpha *= border_opacity;
        ammonite_ui_draw_icon_dashed_border (snapshot, &dash_bounds, dashed_border_color);

        icon_color = color;
        icon_color.alpha *= dim_opacity;
        ammonite_ui_draw_svg (snapshot, ammonite_ui_get_cut_svg (TRUE),
                              &icon_bounds, icon_color);
    }

    GTK_WIDGET_CLASS (ammonite_grid_cell_parent_class)->snapshot (widget, snapshot);
}

static void
ammonite_grid_cell_class_init (AmmoniteGridCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ammonite_grid_cell_dispose;

    widget_class->measure = ammonite_grid_cell_measure;
    widget_class->size_allocate = ammonite_grid_cell_size_allocate;

    widget_class->snapshot = snapshot;

    g_type_ensure (AMMONITE_TYPE_IMAGE);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/ammonite/ammonite/ui/ammonite-grid-cell.ui");

    gtk_widget_class_bind_template_child (widget_class, AmmoniteGridCell, icon);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteGridCell, emblems_box);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteGridCell, labels_box);

    gtk_widget_class_bind_template_callback (widget_class, on_label_query_tooltip);

    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_NONE);
}

static void
ammonite_grid_cell_init (AmmoniteGridCell *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self, "notify::icon-size",
                      G_CALLBACK (on_icon_size_changed), NULL);
    g_signal_connect (self, "notify::scale-factor", G_CALLBACK (on_icon_size_changed), NULL);

    g_signal_connect_object (ammonite_tag_manager_get (), "starred-changed",
                             G_CALLBACK (on_starred_changed), self, G_CONNECT_DEFAULT);

    g_signal_connect_object (ammonite_preferences, "changed::" AMMONITE_PREFERENCES_DATE_TIME_FORMAT,
                             G_CALLBACK (update_captions), self,
                             G_CONNECT_SWAPPED);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (AMMONITE_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::is-cut",
                                    (GCallback) update_icon, self);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    (GCallback) on_file_changed, self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             (GCallback) on_file_changed, self,
                             G_CONNECT_SWAPPED);

    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);
}

AmmoniteGridCell *
ammonite_grid_cell_new (AmmoniteListBase *view,
                        GQuark           *attrs)
{
    g_assert (attrs != NULL);

    AmmoniteGridCell *self = g_object_new (AMMONITE_TYPE_GRID_CELL,
                                           "view", view,
                                           NULL);

    self->caption_attributes = attrs;

    return self;
}
