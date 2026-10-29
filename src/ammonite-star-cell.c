/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-star-cell.h"

#include "ammonite-file.h"
#include "ammonite-tag-manager.h"
#include "ammonite-view-cell.h"
#include "ammonite-view-item.h"

#include <glib/gi18n.h>

struct _AmmoniteStarCell
{
    AmmoniteViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GtkButton *star;
};

G_DEFINE_FINAL_TYPE (AmmoniteStarCell, ammonite_star_cell, AMMONITE_TYPE_VIEW_CELL)

static void
toggle_star (AmmoniteStarCell *self)
{
    AmmoniteTagManager *tag_manager = ammonite_tag_manager_get ();
    g_autoptr (AmmoniteViewItem) item = NULL;
    AmmoniteFile *file;
    g_autofree gchar *uri = NULL;

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = ammonite_view_item_get_file (item);
    uri = ammonite_file_get_activation_uri (file);

    if (ammonite_tag_manager_file_is_starred (tag_manager, uri))
    {
        ammonite_tag_manager_unstar_files (tag_manager,
                                           G_OBJECT (item),
                                           &(GList){ .data = file },
                                           (GAsyncReadyCallback) ammonite_tag_manager_announce_unstarred_cb,
                                           self->star,
                                           NULL);
        gtk_widget_remove_css_class (GTK_WIDGET (self->star), "starred");
    }
    else
    {
        ammonite_tag_manager_star_files (tag_manager,
                                         G_OBJECT (item),
                                         &(GList){ .data = file },
                                         (GAsyncReadyCallback) ammonite_tag_manager_announce_starred_cb,
                                         self->star,
                                         NULL);
        gtk_widget_add_css_class (GTK_WIDGET (self->star), "starred");
    }
    gtk_widget_add_css_class (GTK_WIDGET (self->star), "interacted");
}

static void
update_star (GtkButton    *star,
             AmmoniteFile *file)
{
    g_return_if_fail (AMMONITE_IS_FILE (file));

    g_autofree gchar *file_uri = ammonite_file_get_activation_uri (file);
    gboolean is_starred = ammonite_tag_manager_file_is_starred (ammonite_tag_manager_get (),
                                                                file_uri);
    const gchar *tooltip = is_starred ? _("Unstar") : C_("Verb", "Star");

    /* Setting the tooltip is somewhat expensive as it involves system calls, so only
     * update UI on change. */
    if (g_strcmp0 (gtk_widget_get_tooltip_text (GTK_WIDGET (star)), tooltip) != 0)
    {
        gtk_button_set_icon_name (star, is_starred ? "starred-symbolic" : "non-starred-symbolic");
        gtk_widget_set_tooltip_text (GTK_WIDGET (star), tooltip);
    }
}

static void
on_file_changed (AmmoniteStarCell *self)
{
    g_autoptr (AmmoniteViewItem) item = NULL;
    AmmoniteFile *file;

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = ammonite_view_item_get_file (item);

    update_star (self->star, file);
}

static void
on_starred_changed (AmmoniteTagManager *tag_manager,
                    GList              *changed_files,
                    gpointer            user_data)
{
    AmmoniteStarCell *self = user_data;
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
        update_star (self->star, file);
    }
}

static void
ammonite_star_cell_init (AmmoniteStarCell *self)
{
    GtkWidget *star;

    /* Create star icon */
    star = gtk_button_new ();
    gtk_widget_set_halign (star, GTK_ALIGN_END);
    gtk_widget_set_valign (star, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (star, "dimmed");
    gtk_widget_add_css_class (star, "star");
    gtk_widget_add_css_class (star, "flat");
    gtk_widget_add_css_class (star, "circular");
    gtk_widget_set_parent (star, GTK_WIDGET (self));
    self->star = GTK_BUTTON (star);

    g_signal_connect_swapped (self->star, "clicked", G_CALLBACK (toggle_star), self);

    /* Update on tag changes */
    g_signal_connect_object (ammonite_tag_manager_get (), "starred-changed",
                             G_CALLBACK (on_starred_changed), self, G_CONNECT_DEFAULT);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (AMMONITE_TYPE_VIEW_ITEM);
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
ammonite_star_cell_dispose (GObject *object)
{
    AmmoniteStarCell *self = (AmmoniteStarCell *) object;

    g_clear_object (&self->item_signal_group);
    if (self->star)
    {
        gtk_widget_unparent (GTK_WIDGET (self->star));
        self->star = NULL;
    }
    G_OBJECT_CLASS (ammonite_star_cell_parent_class)->dispose (object);
}

static void
ammonite_star_cell_class_init (AmmoniteStarCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ammonite_star_cell_dispose;

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

AmmoniteViewCell *
ammonite_star_cell_new (AmmoniteListBase *view)
{
    return AMMONITE_VIEW_CELL (g_object_new (AMMONITE_TYPE_STAR_CELL,
                                             "view", view,
                                             NULL));
}
