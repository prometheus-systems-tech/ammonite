/*
 * SPDX-FileCopyrightText: 2024 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-network-cell.h"

#include <glib/gi18n.h>

#include "ammonite-directory.h"
#include "ammonite-file.h"
#include "ammonite-scheme.h"
#include "ammonite-view-item.h"

struct _AmmoniteNetworkCell
{
    AmmoniteViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GtkWidget *top_child;
    GtkWidget *icon;
    GtkWidget *target_uri;
    GtkWidget *unmount_button;
};

G_DEFINE_FINAL_TYPE (AmmoniteNetworkCell, ammonite_network_cell, AMMONITE_TYPE_VIEW_CELL)

static void
update_labels (AmmoniteNetworkCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));

    g_return_if_fail (item != NULL);

    AmmoniteFile *file = ammonite_view_item_get_file (item);
    g_autofree char *target_uri = ammonite_file_get_activation_uri (file);

    if (g_str_has_prefix (target_uri, SCHEME_COMPUTER ":///"))
    {
        /* Online accounts do not currently have a target URI. */
        g_set_str (&target_uri, _("Online Account"));
    }

    gtk_label_set_label (GTK_LABEL (self->target_uri), target_uri);
}

static void
update_icon (AmmoniteNetworkCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));

    g_return_if_fail (item != NULL);

    g_autoptr (GIcon) icon = ammonite_file_get_gicon (ammonite_view_item_get_file (item),
                                                      AMMONITE_FILE_ICON_FLAGS_NONE);

    gtk_image_set_from_gicon (GTK_IMAGE (self->icon), icon);
}

static void
on_file_changed (AmmoniteNetworkCell *self)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));

    g_return_if_fail (item != NULL);

    update_icon (self);
    update_labels (self);

    AmmoniteFile *file = ammonite_view_item_get_file (item);

    gtk_widget_set_visible (self->unmount_button, ammonite_file_can_unmount (file));
}

static void
on_unmount_clicked (AmmoniteNetworkCell *self)
{
    /* Select item first, because view.unmount-volume acts on selection. */
    gtk_widget_activate_action (GTK_WIDGET (self), "listitem.select", "(bb)", FALSE, FALSE);
    gtk_widget_activate_action (GTK_WIDGET (self), "view.unmount-volume", NULL);
}

static void
ammonite_network_cell_init (AmmoniteNetworkCell *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (AMMONITE_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::loading",
                                    (GCallback) on_file_changed, self);
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
ammonite_network_cell_dispose (GObject *object)
{
    AmmoniteNetworkCell *self = (AmmoniteNetworkCell *) object;

    gtk_widget_dispose_template (GTK_WIDGET (self), AMMONITE_TYPE_NETWORK_CELL);

    G_OBJECT_CLASS (ammonite_network_cell_parent_class)->dispose (object);
}

static void
ammonite_network_cell_finalize (GObject *object)
{
    AmmoniteNetworkCell *self = (AmmoniteNetworkCell *) object;

    g_clear_object (&self->item_signal_group);
    G_OBJECT_CLASS (ammonite_network_cell_parent_class)->finalize (object);
}

static void
ammonite_network_cell_class_init (AmmoniteNetworkCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ammonite_network_cell_dispose;
    object_class->finalize = ammonite_network_cell_finalize;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/ammonite/ammonite/ui/ammonite-network-cell.ui");

    /* Needs to add the direct child of the template widget to dispose it since
     * a plain GtkWidget doesn't dispose its child automatically. */
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNetworkCell, top_child);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNetworkCell, icon);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNetworkCell, target_uri);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNetworkCell, unmount_button);
    gtk_widget_class_bind_template_callback (widget_class, on_unmount_clicked);

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

AmmoniteViewCell *
ammonite_network_cell_new (AmmoniteListBase *view)
{
    return AMMONITE_VIEW_CELL (g_object_new (AMMONITE_TYPE_NETWORK_CELL,
                                             "view", view,
                                             NULL));
}
