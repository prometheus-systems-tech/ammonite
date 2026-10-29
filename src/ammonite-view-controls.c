/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ammonite-view-controls.h"

#include "ammonite-file.h"
#include "ammonite-file-utilities.h"
#include "ammonite-scheme.h"
#include "ammonite-ui-utilities.h"
#include "ammonite-window-slot.h"

/* Assume sort section is second item in view_menu. See ammonite-view-controls.blp */
#define SORT_ITEM_POSITION 1

struct _AmmoniteViewControls
{
    AdwBin parent_instance;

    GtkWidget *view_split_button;
    GMenuModel *view_menu;
    GMenuModel *sort_section;

    AmmoniteWindowSlot *window_slot;
};

G_DEFINE_FINAL_TYPE (AmmoniteViewControls, ammonite_view_controls, ADW_TYPE_BIN);


enum
{
    PROP_0,
    PROP_WINDOW_SLOT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
update_sort_section (AmmoniteViewControls *self,
                     AmmoniteFile         *file)
{
    const char *action;

    /* When not in the special location, set an inexistant action to hide the
     * menu item. This works under the assumptiont that the menu item has its
     * "hidden-when" attribute set to "action-disabled", and that an inexistant
     * action is treated as a disabled action. */
    action = ammonite_file_is_in_trash (file) ? "view.sort" : "doesnt-exist";
    ammonite_menu_item_change_attribute (self->sort_section,
                                         "last_trashed",
                                         G_MENU_ATTRIBUTE_ACTION, action);

    action = ammonite_file_is_in_recent (file) ? "view.sort" : "doesnt-exist";
    ammonite_menu_item_change_attribute (self->sort_section,
                                         "recency",
                                         G_MENU_ATTRIBUTE_ACTION, action);

    action = ammonite_file_is_in_search (file) ? "view.sort" : "doesnt-exist";
    ammonite_menu_item_change_attribute (self->sort_section,
                                         "relevance",
                                         G_MENU_ATTRIBUTE_ACTION, action);
}

static void
adjust_to_location (AmmoniteViewControls *self,
                    GParamSpec           *param,
                    AmmoniteWindowSlot   *slot)
{
    GFile *location = ammonite_window_slot_get_location (slot);
    gboolean has_menu = location != NULL &&
                        !ammonite_is_root_for_scheme (location, SCHEME_NETWORK_VIEW);

    gtk_widget_set_sensitive (self->view_split_button, has_menu);

    if (!has_menu)
    {
        return;
    }

    g_autoptr (AmmoniteFile) file = ammonite_file_get (location);

    update_sort_section (self, file);
}

static void
on_tooltip_changed (AmmoniteViewControls *self,
                    GParamSpec           *param,
                    AmmoniteWindowSlot   *slot)
{
    const gchar *description;
    const gchar *tooltip = ammonite_window_slot_get_tooltip_with_description (slot, &description);

    if (tooltip == NULL)
    {
        return;
    }
    gtk_accessible_update_property (GTK_ACCESSIBLE (self->view_split_button),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL,
                                    tooltip,
                                    GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                    description,
                                    -1);
}


static void
disconnect_toolbar_menu_sections_change_handler (AmmoniteViewControls *self)
{
    if (self->window_slot == NULL)
    {
        return;
    }

    g_signal_handlers_disconnect_by_func (self->window_slot,
                                          G_CALLBACK (adjust_to_location),
                                          self);
    g_signal_handlers_disconnect_by_func (self->window_slot,
                                          G_CALLBACK (on_tooltip_changed),
                                          self);
}


static void
ammonite_view_controls_set_window_slot (AmmoniteViewControls *self,
                                        AmmoniteWindowSlot   *window_slot)
{
    g_return_if_fail (AMMONITE_IS_VIEW_CONTROLS (self));
    g_return_if_fail (window_slot == NULL || AMMONITE_IS_WINDOW_SLOT (window_slot));

    if (self->window_slot == window_slot)
    {
        return;
    }

    disconnect_toolbar_menu_sections_change_handler (self);

    self->window_slot = window_slot;

    if (self->window_slot != NULL)
    {
        adjust_to_location (self, NULL, self->window_slot);
        g_signal_connect_swapped (self->window_slot, "notify::location",
                                  G_CALLBACK (adjust_to_location), self);
        g_signal_connect_swapped (self->window_slot, "notify::tooltip",
                                  G_CALLBACK (on_tooltip_changed), self);
    }

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WINDOW_SLOT]);
}

static void
ammonite_view_controls_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    AmmoniteViewControls *self = AMMONITE_VIEW_CONTROLS (object);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            g_value_set_object (value, G_OBJECT (self->window_slot));
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_view_controls_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    AmmoniteViewControls *self = AMMONITE_VIEW_CONTROLS (object);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            ammonite_view_controls_set_window_slot (self, AMMONITE_WINDOW_SLOT (g_value_get_object (value)));
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_view_controls_dispose (GObject *obj)
{
    AmmoniteViewControls *self = AMMONITE_VIEW_CONTROLS (obj);

    disconnect_toolbar_menu_sections_change_handler (self);
    adw_bin_set_child (ADW_BIN (self), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (self), AMMONITE_TYPE_VIEW_CONTROLS);

    G_OBJECT_CLASS (ammonite_view_controls_parent_class)->dispose (obj);
}

static void
ammonite_view_controls_finalize (GObject *obj)
{
    AmmoniteViewControls *self = AMMONITE_VIEW_CONTROLS (obj);

    g_clear_object (&self->sort_section);
    g_clear_object (&self->window_slot);

    G_OBJECT_CLASS (ammonite_view_controls_parent_class)->finalize (obj);
}

static void
ammonite_view_controls_class_init (AmmoniteViewControlsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ammonite_view_controls_dispose;
    object_class->finalize = ammonite_view_controls_finalize;
    object_class->get_property = ammonite_view_controls_get_property;
    object_class->set_property = ammonite_view_controls_set_property;

    properties[PROP_WINDOW_SLOT] = g_param_spec_object ("window-slot",
                                                        NULL, NULL,
                                                        AMMONITE_TYPE_WINDOW_SLOT,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/ammonite/ammonite/ui/ammonite-view-controls.ui");
    gtk_widget_class_bind_template_child (widget_class, AmmoniteViewControls, view_menu);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteViewControls, view_split_button);
}

static void
ammonite_view_controls_init (AmmoniteViewControls *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_autoptr (GtkBuilder) builder =
        gtk_builder_new_from_resource ("/org/ammonite/ammonite/menu/ammonite-toolbar-view-menu.ui");
    g_autoptr (GMenuItem) sort_item = g_menu_item_new_from_model (self->view_menu, SORT_ITEM_POSITION);

    self->sort_section = G_MENU_MODEL (g_object_ref (gtk_builder_get_object (builder, "sort_section")));
    g_menu_item_set_section (sort_item, self->sort_section);
    g_menu_insert_item (G_MENU (self->view_menu), SORT_ITEM_POSITION, sort_item);
}
