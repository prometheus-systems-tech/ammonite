/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-compact-cell.h"

#include "ammonite-file.h"
#include "ammonite-image.h"
#include "ammonite-tag-manager.h"
#include "ammonite-view-item.h"

/* Maximum width of the file name, in characters. Keeps columns reasonably
 * narrow, so that many items fit on screen. */
#define LABEL_MAX_WIDTH_CHARS 40

struct _AmmoniteCompactCell
{
    AmmoniteViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GtkWidget *icon;
    GtkWidget *label;
    GtkWidget *star;
};

G_DEFINE_FINAL_TYPE (AmmoniteCompactCell, ammonite_compact_cell, AMMONITE_TYPE_VIEW_CELL)

static void
update_icon (AmmoniteCompactCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_autoptr (GdkPaintable) icon_paintable = NULL;
    AmmoniteFile *file;
    guint icon_size;
    gboolean is_cut;

    if (item == NULL)
    {
        return;
    }

    file = ammonite_view_item_get_file (item);
    g_object_get (self, "icon-size", &icon_size, NULL);
    g_object_get (item, "is-cut", &is_cut, NULL);

    icon_paintable = ammonite_file_get_icon_paintable (file,
                                                       icon_size,
                                                       gtk_widget_get_scale_factor (GTK_WIDGET (self)),
                                                       AMMONITE_FILE_ICON_FLAGS_NONE);
    ammonite_image_set_size (AMMONITE_IMAGE (self->icon), icon_size);
    ammonite_image_set_fallback (AMMONITE_IMAGE (self->icon), icon_paintable);

    /* Thumbnails are pointless at these sizes (and expensive). */
    ammonite_image_set_source (AMMONITE_IMAGE (self->icon), NULL);

    gtk_widget_set_opacity (self->icon, is_cut ? 0.4 : 1.0);

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
update_label (AmmoniteCompactCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_autofree char *name = NULL;

    if (item == NULL)
    {
        return;
    }

    g_object_get (ammonite_view_item_get_file (item), "display-name", &name, NULL);
    gtk_label_set_text (GTK_LABEL (self->label), name);
}

static void
update_star (AmmoniteCompactCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_autofree char *uri = NULL;

    if (item == NULL)
    {
        return;
    }

    uri = ammonite_file_get_activation_uri (ammonite_view_item_get_file (item));
    gtk_widget_set_visible (self->star,
                            ammonite_tag_manager_file_is_starred (ammonite_tag_manager_get (), uri));
}

static void
update_all (AmmoniteCompactCell *self)
{
    update_icon (self);
    update_label (self);
    update_star (self);
}

static void
on_icon_size_changed (AmmoniteCompactCell *self)
{
    update_icon (self);
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
on_starred_changed (AmmoniteTagManager *tag_manager,
                    GList              *changed_files,
                    gpointer            user_data)
{
    update_star (AMMONITE_COMPACT_CELL (user_data));
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
ammonite_compact_cell_dispose (GObject *object)
{
    AmmoniteCompactCell *self = AMMONITE_COMPACT_CELL (object);

    g_clear_pointer (&self->icon, gtk_widget_unparent);
    g_clear_pointer (&self->label, gtk_widget_unparent);
    g_clear_pointer (&self->star, gtk_widget_unparent);
    g_clear_object (&self->item_signal_group);

    G_OBJECT_CLASS (ammonite_compact_cell_parent_class)->dispose (object);
}

static void
ammonite_compact_cell_class_init (AmmoniteCompactCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ammonite_compact_cell_dispose;

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_NONE);
}

static void
ammonite_compact_cell_init (AmmoniteCompactCell *self)
{
    GtkBoxLayout *layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (self)));

    g_type_ensure (AMMONITE_TYPE_IMAGE);

    gtk_box_layout_set_spacing (layout, 6);
    gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);
    gtk_widget_add_css_class (GTK_WIDGET (self), "ammonite-compact-cell");

    self->icon = g_object_new (AMMONITE_TYPE_IMAGE,
                               "halign", GTK_ALIGN_CENTER,
                               "valign", GTK_ALIGN_CENTER,
                               NULL);
    gtk_widget_set_parent (self->icon, GTK_WIDGET (self));

    self->label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (self->label), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (self->label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_max_width_chars (GTK_LABEL (self->label), LABEL_MAX_WIDTH_CHARS);
    gtk_widget_set_has_tooltip (self->label, TRUE);
    gtk_widget_set_hexpand (self->label, TRUE);
    g_signal_connect (self->label, "query-tooltip", G_CALLBACK (on_label_query_tooltip), NULL);
    gtk_widget_set_parent (self->label, GTK_WIDGET (self));

    self->star = gtk_image_new_from_icon_name ("starred-symbolic");
    gtk_widget_add_css_class (self->star, "dimmed");
    gtk_widget_set_visible (self->star, FALSE);
    gtk_widget_set_parent (self->star, GTK_WIDGET (self));

    g_signal_connect (self, "notify::icon-size", G_CALLBACK (on_icon_size_changed), NULL);
    g_signal_connect (self, "notify::scale-factor", G_CALLBACK (on_icon_size_changed), NULL);

    g_signal_connect_object (ammonite_tag_manager_get (), "starred-changed",
                             G_CALLBACK (on_starred_changed), self, G_CONNECT_DEFAULT);

    self->item_signal_group = g_signal_group_new (AMMONITE_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::is-cut",
                                    (GCallback) update_icon, self);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    (GCallback) update_all, self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             (GCallback) update_all, self,
                             G_CONNECT_SWAPPED);

    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);
}

AmmoniteCompactCell *
ammonite_compact_cell_new (AmmoniteListBase *view)
{
    return g_object_new (AMMONITE_TYPE_COMPACT_CELL,
                         "view", view,
                         NULL);
}
