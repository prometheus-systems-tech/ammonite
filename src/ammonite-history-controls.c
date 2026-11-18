/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ammonite-history-controls.h"

#include "ammonite-bookmark.h"
#include "ammonite-window.h"
#include "ammonite-window-slot.h"

struct _AmmoniteHistoryControls
{
    AdwBin parent_instance;

    GtkWidget *back_button;
    GtkWidget *back_menu;

    GtkWidget *forward_button;
    GtkWidget *forward_menu;

    AmmoniteWindowSlot *window_slot;
};

G_DEFINE_FINAL_TYPE (AmmoniteHistoryControls, ammonite_history_controls, ADW_TYPE_BIN)

enum
{
    PROP_0,
    PROP_WINDOW_SLOT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
fill_menu (AmmoniteHistoryControls *self,
           GMenu                   *menu,
           gboolean                 back)
{
    int step = back ? -1 : 1;
    int index = step;
    GList *list;
    const gchar *name;

    list = back ? ammonite_window_slot_get_back_history (self->window_slot) :
                  ammonite_window_slot_get_forward_history (self->window_slot);

    while (list != NULL)
    {
        g_autoptr (GMenuItem) item = NULL;

        name = ammonite_bookmark_get_name (AMMONITE_BOOKMARK (list->data));
        item = g_menu_item_new (name, NULL);
        g_menu_item_set_action_and_target (item,
                                           back ? "slot.back-n" : "slot.forward-n",
                                           "i", index);
        g_menu_append_item (menu, item);

        list = g_list_next (list);
        index += step;
    }
}

static void
show_menu (AmmoniteHistoryControls *self,
           GtkWidget               *widget)
{
    int direction = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                        "nav-direction"));
    g_autoptr (GMenu) menu = g_menu_new ();
    GtkPopoverMenu *popover;

    if (direction == AMMONITE_NAVIGATION_DIRECTION_FORWARD)
    {
        fill_menu (self, menu, FALSE);
        popover = GTK_POPOVER_MENU (self->forward_menu);
    }
    else if (direction == AMMONITE_NAVIGATION_DIRECTION_BACK)
    {
        fill_menu (self, menu, TRUE);
        popover = GTK_POPOVER_MENU (self->back_menu);
    }
    else
    {
        g_return_if_reached ();
    }

    gtk_popover_menu_set_menu_model (popover, G_MENU_MODEL (menu));
    gtk_popover_popup (GTK_POPOVER (popover));
}

static void
navigation_button_press_cb (GtkGestureClick *gesture,
                            gint             n_press,
                            gdouble          x,
                            gdouble          y,
                            gpointer         user_data)
{
    AmmoniteHistoryControls *self = AMMONITE_HISTORY_CONTROLS (user_data);

    GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
    guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    if (button == GDK_BUTTON_PRIMARY)
    {
        /* Don't do anything, primary click is handled through activate */
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
        return;
    }
    else if (button == GDK_BUTTON_MIDDLE)
    {
        GtkRoot *window = gtk_widget_get_root (GTK_WIDGET (self));

        if (!AMMONITE_IS_WINDOW (window))
        {
            /* Only AmmoniteWindow offers special behavior for this event. */
            gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
            return;
        }

        int direction = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
                                                            "nav-direction"));

        ammonite_window_back_or_forward_in_new_tab (AMMONITE_WINDOW (window), direction);
    }
    else if (button == GDK_BUTTON_SECONDARY)
    {
        show_menu (self, widget);
    }
}

static void
back_button_longpress_cb (GtkGestureLongPress *gesture,
                          double               x,
                          double               y,
                          gpointer             user_data)
{
    AmmoniteHistoryControls *self = user_data;

    show_menu (self, self->back_button);
}

static void
forward_button_longpress_cb (GtkGestureLongPress *gesture,
                             double               x,
                             double               y,
                             gpointer             user_data)
{
    AmmoniteHistoryControls *self = user_data;

    show_menu (self, self->forward_button);
}


static void
ammonite_history_controls_contructed (GObject *object)
{
    AmmoniteHistoryControls *self;
    GtkEventController *controller;

    G_OBJECT_CLASS (ammonite_history_controls_parent_class)->constructed (object);

    self = AMMONITE_HISTORY_CONTROLS (object);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (self->back_button, controller);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (back_button_longpress_cb), self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (self->forward_button, controller);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (forward_button_longpress_cb), self);

    g_object_set_data (G_OBJECT (self->back_button), "nav-direction",
                       GUINT_TO_POINTER (AMMONITE_NAVIGATION_DIRECTION_BACK));
    g_object_set_data (G_OBJECT (self->forward_button), "nav-direction",
                       GUINT_TO_POINTER (AMMONITE_NAVIGATION_DIRECTION_FORWARD));

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (self->back_button, controller);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (navigation_button_press_cb), self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (self->forward_button, controller);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (navigation_button_press_cb), self);
}

static void
ammonite_history_controls_dispose (GObject *object)
{
    AmmoniteHistoryControls *self;

    self = AMMONITE_HISTORY_CONTROLS (object);

    g_clear_pointer (&self->back_menu, gtk_widget_unparent);
    g_clear_pointer (&self->forward_menu, gtk_widget_unparent);

    adw_bin_set_child (ADW_BIN (self), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (self), AMMONITE_TYPE_HISTORY_CONTROLS);

    G_OBJECT_CLASS (ammonite_history_controls_parent_class)->dispose (object);
}

static void
ammonite_history_controls_set_window_slot (AmmoniteHistoryControls *self,
                                           AmmoniteWindowSlot      *window_slot)
{
    g_return_if_fail (AMMONITE_IS_HISTORY_CONTROLS (self));
    g_return_if_fail (window_slot == NULL || AMMONITE_IS_WINDOW_SLOT (window_slot));

    if (self->window_slot == window_slot)
    {
        return;
    }

    self->window_slot = window_slot;

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WINDOW_SLOT]);
}

static void
ammonite_history_controls_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
    AmmoniteHistoryControls *self = AMMONITE_HISTORY_CONTROLS (object);

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
ammonite_history_controls_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    AmmoniteHistoryControls *self = AMMONITE_HISTORY_CONTROLS (object);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            ammonite_history_controls_set_window_slot (self, AMMONITE_WINDOW_SLOT (g_value_get_object (value)));
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_history_controls_class_init (AmmoniteHistoryControlsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = ammonite_history_controls_contructed;
    object_class->dispose = ammonite_history_controls_dispose;
    object_class->set_property = ammonite_history_controls_set_property;
    object_class->get_property = ammonite_history_controls_get_property;

    properties[PROP_WINDOW_SLOT] = g_param_spec_object ("window-slot",
                                                        NULL, NULL,
                                                        AMMONITE_TYPE_WINDOW_SLOT,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/ammonite/ammonite/ui/ammonite-history-controls.ui");
    gtk_widget_class_bind_template_child (widget_class, AmmoniteHistoryControls, back_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteHistoryControls, back_menu);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteHistoryControls, forward_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteHistoryControls, forward_menu);
}

static void
ammonite_history_controls_init (AmmoniteHistoryControls *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    gtk_widget_set_parent (self->back_menu, self->back_button);
    g_signal_connect (self->back_menu, "destroy", G_CALLBACK (gtk_widget_unparent), NULL);
    gtk_widget_set_parent (self->forward_menu, self->forward_button);
    g_signal_connect (self->forward_menu, "destroy", G_CALLBACK (gtk_widget_unparent), NULL);
}
