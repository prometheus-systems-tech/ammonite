/*
 * SPDX-FileCopyrightText: 2004 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2004 Jonathan Blandford <jrb@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */


#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "ammonite-pathbar.h"
#include "ammonite-properties.h"

#include "ammonite-dnd.h"
#include "ammonite-enums.h"
#include "ammonite-enum-types.h"
#include "ammonite-file.h"
#include "ammonite-files-view.h"
#include "ammonite-file-utilities.h"
#include "ammonite-icon-names.h"
#include "ammonite-scheme.h"
#include "ammonite-trash-monitor.h"
#include "ammonite-ui-utilities.h"
#include "ammonite-window-slot.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

enum
{
    OPEN_LOCATION,
    LAST_SIGNAL
};

typedef enum
{
    NORMAL_BUTTON,
    ROOT_BUTTON,
    ADMIN_ROOT_BUTTON,
    HOME_BUTTON,
    STARRED_BUTTON,
    RECENT_BUTTON,
    MOUNT_BUTTON,
    TRASH_BUTTON,
    NETWORK_BUTTON,
    COMPUTER_BUTTON,
    BURN_BUTTON,
} ButtonType;

#define BUTTON_DATA(x) ((ButtonData *) (x))

static guint path_bar_signals[LAST_SIGNAL] = { 0 };

enum
{
    PROP_0,
    PROP_WINDOW_SLOT,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

#define AMMONITE_PATH_BAR_BUTTON_ELLIPSIZE_MINIMUM_CHARS 7

typedef struct
{
    GtkWidget *button;
    ButtonType type;
    char *dir_name;
    GFile *path;
    AmmoniteFile *file;
    unsigned int file_changed_signal_id;

    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *separator;
    GtkWidget *container;

    AmmonitePathBar *path_bar;

    guint ignore_changes : 1;
    guint is_root : 1;

    graphene_point_t hover_start_point;
    guint switch_location_timer;
} ButtonData;

struct _AmmonitePathBar
{
    GtkBox parent_instance;

    GtkWidget *scrolled;
    GtkWidget *buttons_box;

    GFile *current_path;
    gpointer current_button_data;

    GList *button_list;

    GActionGroup *action_group;

    AmmoniteFile *context_menu_file;
    GtkPopoverMenu *current_view_menu_popover;
    guint bind_menu_model_to_popover_id;
    GtkWidget *current_view_menu_button;
    GtkWidget *button_menu_popover;
    GMenu *current_view_menu;
    GMenu *extensions_section;
    GMenu *templates_submenu;
    GMenu *button_menu;

    gchar *os_name;

    AmmoniteWindowSlot *slot;
};

G_DEFINE_FINAL_TYPE (AmmonitePathBar, ammonite_path_bar, GTK_TYPE_BOX);

static void ammonite_path_bar_update_button_state (ButtonData *button_data,
                                                   gboolean    current_dir);

static void     unschedule_pop_up_context_menu (AmmonitePathBar *self);
static void     action_pathbar_open_item_new_window (GSimpleAction *action,
                                                     GVariant      *state,
                                                     gpointer       user_data);
static void     action_pathbar_open_item_new_tab (GSimpleAction *action,
                                                  GVariant      *state,
                                                  gpointer       user_data);
static void     action_pathbar_properties (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data);
static void     pop_up_pathbar_context_menu (AmmonitePathBar *self,
                                             AmmoniteFile    *file);
static void ammonite_path_bar_clear_buttons (AmmonitePathBar *self);

static const GActionEntry path_bar_actions[] =
{
    { .name = "open-item-new-tab", .activate = action_pathbar_open_item_new_tab },
    { .name = "open-item-new-window", .activate = action_pathbar_open_item_new_window },
    { .name = "properties", .activate = action_pathbar_properties}
};


static void
action_pathbar_open_item_new_tab (GSimpleAction *action,
                                  GVariant      *state,
                                  gpointer       user_data)
{
    AmmonitePathBar *self;
    GFile *location;

    self = AMMONITE_PATH_BAR (user_data);

    if (self->context_menu_file == NULL)
    {
        return;
    }

    location = ammonite_file_get_location (self->context_menu_file);

    if (location)
    {
        g_signal_emit (user_data, path_bar_signals[OPEN_LOCATION], 0, location,
                       AMMONITE_OPEN_FLAG_NEW_TAB | AMMONITE_OPEN_FLAG_DONT_MAKE_ACTIVE);
        g_object_unref (location);
    }
}

static void
action_pathbar_open_item_new_window (GSimpleAction *action,
                                     GVariant      *state,
                                     gpointer       user_data)
{
    AmmonitePathBar *self;
    GFile *location;

    self = AMMONITE_PATH_BAR (user_data);

    if (self->context_menu_file == NULL)
    {
        return;
    }

    location = ammonite_file_get_location (self->context_menu_file);

    if (location)
    {
        g_signal_emit (user_data, path_bar_signals[OPEN_LOCATION], 0, location, AMMONITE_OPEN_FLAG_NEW_WINDOW);
        g_object_unref (location);
    }
}

static void
action_pathbar_properties (GSimpleAction *action,
                           GVariant      *state,
                           gpointer       user_data)
{
    AmmonitePathBar *self;
    GList *files;
    g_autoptr (GFile) location = NULL;

    self = AMMONITE_PATH_BAR (user_data);

    g_return_if_fail (AMMONITE_IS_FILE (self->context_menu_file));

    files = g_list_append (NULL, ammonite_file_ref (self->context_menu_file));
    location = ammonite_file_get_location (self->context_menu_file);

    ammonite_properties_present_dialog (files, GTK_WIDGET (self), location);

    ammonite_file_list_free (files);
}

static void
on_adjustment_changed (GtkAdjustment   *adjustment,
                       AmmonitePathBar *self)
{
    /* Automatically scroll to the end, to reveal the current folder. */
    GtkWidget *last = gtk_widget_get_last_child (self->buttons_box);
    GtkViewport *viewport;

    viewport = GTK_VIEWPORT (gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (self->scrolled)));

    if (last != NULL)
    {
        gtk_viewport_scroll_to (viewport, last, NULL);
    }
}

static gboolean
bind_current_view_menu_model_to_popover (AmmonitePathBar *self)
{
    gtk_popover_menu_set_menu_model (self->current_view_menu_popover,
                                     G_MENU_MODEL (self->current_view_menu));
    self->bind_menu_model_to_popover_id = 0;
    return G_SOURCE_REMOVE;
}

static gboolean
on_scroll (GtkEventControllerScroll *scroll,
           gdouble                   dx,
           gdouble                   dy,
           GtkScrolledWindow        *self)
{
    GtkAdjustment *hadjustment;
    gdouble step;
    gdouble new_value;

    if (dy == 0)
    {
        return GDK_EVENT_PROPAGATE;
    }

    /* Scroll horizontally when vertically scrolled */
    hadjustment = gtk_scrolled_window_get_hadjustment (self);
    step = gtk_adjustment_get_step_increment (hadjustment);
    new_value = gtk_adjustment_get_value (hadjustment) + dy * step;
    gtk_adjustment_set_value (hadjustment, new_value);

    return GDK_EVENT_STOP;
}


static void
ammonite_path_bar_init (AmmonitePathBar *self)
{
    GtkAdjustment *adjustment;
    GtkBuilder *builder;
    GtkEventController *controller;
    g_autoptr (GError) error = NULL;

    self->os_name = g_get_os_info (G_OS_INFO_KEY_NAME);

    self->scrolled = gtk_scrolled_window_new ();
    /* Scroll horizontally only and don't use internal scrollbar. */
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled),
                                    /* hscrollbar-policy */ GTK_POLICY_EXTERNAL,
                                    /* vscrollbar-policy */ GTK_POLICY_NEVER);
    gtk_widget_set_hexpand (self->scrolled, TRUE);
    gtk_box_append (GTK_BOX (self), self->scrolled);

    adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (self->scrolled));
    g_signal_connect (adjustment, "changed", G_CALLBACK (on_adjustment_changed), self);

    controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_widget_add_controller (GTK_WIDGET (self->scrolled), controller);
    g_signal_connect (controller, "scroll", G_CALLBACK (on_scroll), self->scrolled);

    self->buttons_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scrolled), self->buttons_box);

    self->current_view_menu_button = gtk_menu_button_new ();
    gtk_widget_add_css_class (self->current_view_menu_button, "flat");
    gtk_menu_button_set_child (GTK_MENU_BUTTON (self->current_view_menu_button),
                               gtk_image_new_from_icon_name ("view-more-symbolic"));
    gtk_box_append (GTK_BOX (self), self->current_view_menu_button);

    gtk_widget_set_tooltip_text (self->current_view_menu_button, _("Current Folder Menu"));

    builder = gtk_builder_new ();

    /* Add context menu for pathbar buttons */
    gtk_builder_add_from_resource (builder,
                                   "/org/ammonite/ammonite/menu/ammonite-pathbar-context-menu.ui",
                                   &error);
    if (error != NULL)
    {
        g_error ("Failed to add pathbar-context-menu.ui: %s", error->message);
    }
    self->button_menu = g_object_ref_sink (G_MENU (gtk_builder_get_object (builder, "button-menu")));
    self->button_menu_popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (self->button_menu));
    gtk_widget_set_parent (self->button_menu_popover, GTK_WIDGET (self));
    gtk_popover_set_has_arrow (GTK_POPOVER (self->button_menu_popover), FALSE);
    gtk_widget_set_halign (self->button_menu_popover, GTK_ALIGN_START);

    /* Add current location menu, which shares features with the view's background context menu */
    self->current_view_menu = g_object_ref_sink (G_MENU (gtk_builder_get_object (builder, "current-view-menu")));
    self->extensions_section = g_object_ref (G_MENU (gtk_builder_get_object (builder, "background-extensions-section")));
    self->templates_submenu = g_object_ref (G_MENU (gtk_builder_get_object (builder, "templates-submenu")));
    self->current_view_menu_popover = g_object_ref_sink (GTK_POPOVER_MENU (gtk_popover_menu_new_from_model (NULL)));

    g_object_unref (builder);

    gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->current_view_menu_button),
                                 GTK_WIDGET (self->current_view_menu_popover));
    bind_current_view_menu_model_to_popover (self);

    gtk_widget_add_css_class (GTK_WIDGET (self), "linked");
    gtk_widget_add_css_class (GTK_WIDGET (self), "ammonite-pathbar");

    /* Action group */
    self->action_group = G_ACTION_GROUP (g_simple_action_group_new ());
    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     path_bar_actions,
                                     G_N_ELEMENTS (path_bar_actions),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "pathbar",
                                    G_ACTION_GROUP (self->action_group));
}

static void
ammonite_path_bar_finalize (GObject *object)
{
    AmmonitePathBar *self;

    self = AMMONITE_PATH_BAR (object);

    g_clear_object (&self->current_path);
    g_clear_object (&self->current_view_menu);
    g_clear_object (&self->extensions_section);
    g_clear_object (&self->templates_submenu);
    g_clear_object (&self->button_menu);
    g_clear_object (&self->current_view_menu_popover);
    g_free (self->os_name);

    g_clear_weak_pointer (&self->slot);

    G_OBJECT_CLASS (ammonite_path_bar_parent_class)->finalize (object);
}

static void
ammonite_path_bar_dispose (GObject *object)
{
    AmmonitePathBar *self = AMMONITE_PATH_BAR (object);

    g_clear_handle_id (&self->bind_menu_model_to_popover_id, g_source_remove);

    ammonite_path_bar_clear_buttons (self);

    unschedule_pop_up_context_menu (AMMONITE_PATH_BAR (object));

    G_OBJECT_CLASS (ammonite_path_bar_parent_class)->dispose (object);
}

static const char *
get_dir_name (ButtonData *button_data)
{
    switch (button_data->type)
    {
        case ROOT_BUTTON:
        {
            if (button_data->path_bar != NULL &&
                button_data->path_bar->os_name != NULL)
            {
                return button_data->path_bar->os_name;
            }
            /* Translators: This is the label used in the pathbar when seeing
             * the root directory (also known as /) */
            return _("Operating System");
        }

        case ADMIN_ROOT_BUTTON:
        {
            /* Translators: This is the filesystem root directory (also known
             * as /) when seen as administrator */
            return _("Administrator Root");
        }

        case HOME_BUTTON:
        {
            return _("Home");
        }

        default:
        {
            return button_data->dir_name;
        }
    }
}

static void
button_data_free (ButtonData *button_data)
{
    g_clear_handle_id (&button_data->switch_location_timer, g_source_remove);
    g_object_unref (button_data->path);
    g_free (button_data->dir_name);
    if (button_data->file != NULL)
    {
        g_signal_handler_disconnect (button_data->file,
                                     button_data->file_changed_signal_id);
        ammonite_file_monitor_remove (button_data->file, button_data);
        ammonite_file_unref (button_data->file);
    }

    g_free (button_data);
}
static void
ammonite_path_bar_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    AmmonitePathBar *self = AMMONITE_PATH_BAR (object);

    switch (property_id)
    {
        case PROP_WINDOW_SLOT:
        {
            g_set_weak_pointer (&self->slot, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
ammonite_path_bar_class_init (AmmonitePathBarClass *path_bar_class)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass *) path_bar_class;

    gobject_class->finalize = ammonite_path_bar_finalize;
    gobject_class->dispose = ammonite_path_bar_dispose;
    gobject_class->set_property = ammonite_path_bar_set_property;

    path_bar_signals [OPEN_LOCATION] =
        g_signal_new ("open-location",
                      G_OBJECT_CLASS_TYPE (path_bar_class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 2,
                      G_TYPE_FILE,
                      AMMONITE_TYPE_OPEN_FLAGS);

    properties [PROP_WINDOW_SLOT] =
        g_param_spec_object ("window-slot", NULL, NULL,
                             AMMONITE_TYPE_WINDOW_SLOT,
                             G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

void
ammonite_path_bar_set_extensions_background_menu (AmmonitePathBar *self,
                                                  GMenuModel      *menu)
{
    g_return_if_fail (AMMONITE_IS_PATH_BAR (self));

    ammonite_gmenu_set_from_model (self->extensions_section, menu);
}

void
ammonite_path_bar_set_templates_menu (AmmonitePathBar *self,
                                      GMenuModel      *menu)
{
    g_return_if_fail (AMMONITE_IS_PATH_BAR (self));

    if (!gtk_widget_is_visible (GTK_WIDGET (self->current_view_menu_popover)))
    {
        /* Workaround to avoid leaking duplicated GtkStack pages each time the
         * templates menu is set. Unbinding the model is the only way to clear
         * all children. After that's done, on idle, we rebind it. */
        gtk_popover_menu_set_menu_model (self->current_view_menu_popover, NULL);
    }

    ammonite_gmenu_set_from_model (self->templates_submenu, menu);
    if (self->bind_menu_model_to_popover_id == 0)
    {
        self->bind_menu_model_to_popover_id = g_idle_add ((GSourceFunc) bind_current_view_menu_model_to_popover, self);
    }

    ammonite_menu_item_change_attribute (G_MENU_MODEL (self->current_view_menu),
                                         "templates-submenu",
                                         "hidden-when",
                                         (menu == NULL) ? "action-missing" : NULL);
}

/* Public functions and their helpers */
static void
ammonite_path_bar_clear_buttons (AmmonitePathBar *self)
{
    while (self->button_list != NULL)
    {
        ButtonData *button_data;

        button_data = BUTTON_DATA (self->button_list->data);

        gtk_box_remove (GTK_BOX (self->buttons_box), button_data->container);

        self->button_list = g_list_remove (self->button_list, button_data);
        button_data_free (button_data);
    }
}

void
ammonite_path_bar_show_current_location_menu (AmmonitePathBar *self)
{
    g_return_if_fail (AMMONITE_IS_PATH_BAR (self));

    gtk_menu_button_popup (GTK_MENU_BUTTON (self->current_view_menu_button));
}

static void
button_clicked_cb (GtkButton *button,
                   gpointer   data)
{
    ButtonData *button_data;
    AmmonitePathBar *self;

    button_data = BUTTON_DATA (data);
    if (button_data->ignore_changes)
    {
        return;
    }

    self = button_data->path_bar;

    if (g_file_equal (button_data->path, self->current_path))
    {
        return;
    }
    else
    {
        g_signal_emit (self, path_bar_signals[OPEN_LOCATION], 0,
                       button_data->path,
                       0);
    }
}

static void
real_pop_up_pathbar_context_menu (AmmonitePathBar *self)
{
    gtk_popover_popup (GTK_POPOVER (self->button_menu_popover));
}

static void
pathbar_popup_attributes_ready (AmmoniteFile *file,
                                gpointer      data)
{
    AmmonitePathBar *self;

    g_return_if_fail (AMMONITE_IS_PATH_BAR (data));

    self = AMMONITE_PATH_BAR (data);

    g_return_if_fail (file == self->context_menu_file);

    real_pop_up_pathbar_context_menu (self);
}

static void
unschedule_pop_up_context_menu (AmmonitePathBar *self)
{
    if (self->context_menu_file != NULL)
    {
        g_return_if_fail (AMMONITE_IS_FILE (self->context_menu_file));
        ammonite_file_cancel_call_when_ready (self->context_menu_file,
                                              pathbar_popup_attributes_ready,
                                              self);
        g_clear_pointer (&self->context_menu_file, ammonite_file_unref);
    }
}

static void
schedule_pop_up_context_menu (AmmonitePathBar *self,
                              AmmoniteFile    *file)
{
    g_return_if_fail (AMMONITE_IS_FILE (file));

    if (file == self->context_menu_file)
    {
        if (ammonite_file_check_if_ready (file,
                                          AMMONITE_ATTRIBUTE_INFO |
                                          AMMONITE_ATTRIBUTE_MOUNT |
                                          AMMONITE_ATTRIBUTE_FILESYSTEM_INFO))
        {
            real_pop_up_pathbar_context_menu (self);
        }
    }
    else
    {
        unschedule_pop_up_context_menu (self);

        self->context_menu_file = ammonite_file_ref (file);
        ammonite_file_call_when_ready (self->context_menu_file,
                                       AMMONITE_ATTRIBUTE_INFO |
                                       AMMONITE_ATTRIBUTE_MOUNT |
                                       AMMONITE_ATTRIBUTE_FILESYSTEM_INFO,
                                       pathbar_popup_attributes_ready,
                                       self);
    }
}

static void
pop_up_pathbar_context_menu (AmmonitePathBar *self,
                             AmmoniteFile    *file)
{
    if (file != NULL &&
        ammonite_window_slot_get_mode (self->slot) == AMMONITE_MODE_BROWSE)
    {
        schedule_pop_up_context_menu (self, file);
    }
}


static void
on_click_gesture_pressed (GtkGestureClick *gesture,
                          gint             n_press,
                          gdouble          x,
                          gdouble          y,
                          gpointer         user_data)
{
    ButtonData *button_data;
    AmmonitePathBar *self;
    guint current_button;
    GdkModifierType state;
    graphene_point_t pathbar_point;

    if (n_press != 1)
    {
        return;
    }

    button_data = BUTTON_DATA (user_data);
    self = button_data->path_bar;
    current_button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));


    switch (current_button)
    {
        case GDK_BUTTON_MIDDLE:
        {
            if ((state & gtk_accelerator_get_default_mod_mask ()) == 0)
            {
                g_signal_emit (self, path_bar_signals[OPEN_LOCATION], 0,
                               button_data->path,
                               AMMONITE_OPEN_FLAG_NEW_TAB | AMMONITE_OPEN_FLAG_DONT_MAKE_ACTIVE);
            }
        }
        break;

        case GDK_BUTTON_SECONDARY:
        {
            if (g_file_equal (button_data->path, self->current_path))
            {
                break;
            }
            else
            {
                if (!gtk_widget_compute_point (GTK_WIDGET (button_data->button),
                                               GTK_WIDGET (self),
                                               &GRAPHENE_POINT_INIT (x, y),
                                               &pathbar_point))
                {
                    g_return_if_reached ();
                }

                gtk_popover_set_pointing_to (GTK_POPOVER (self->button_menu_popover),
                                             &(GdkRectangle){pathbar_point.x, pathbar_point.y, 0, 0});
                pop_up_pathbar_context_menu (self, button_data->file);
            }
        }
        break;

        case GDK_BUTTON_PRIMARY:
        {
            if ((state & GDK_CONTROL_MASK) != 0)
            {
                g_signal_emit (button_data->path_bar, path_bar_signals[OPEN_LOCATION], 0,
                               button_data->path,
                               AMMONITE_OPEN_FLAG_NEW_WINDOW);
            }
            else
            {
                /* GtkButton will claim the primary button presses and emit the
                 * "clicked" signal. Handle it in the signal callback, not here.
                 */
                return;
            }
        }
        break;

        default:
        {
            /* Ignore other buttons in this gesture. */
            return;
        }
        break;
    }

    /* Both middle- and secondary-clicking the title bar can have interesting
     * effects (minimizing the window, popping up a window manager menu, etc.),
     * and this avoids all that.
     */
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
switch_location (ButtonData *button_data)
{
    if (button_data->file == NULL)
    {
        return;
    }

    AmmonitePathBar *self = button_data->path_bar;
    g_autoptr (GFile) location = ammonite_file_get_location (button_data->file);

    g_return_if_fail (self->slot != NULL);

    ammonite_window_slot_open_location_full (self->slot,
                                             location,
                                             NULL);
}

static void
switch_location_timer (gpointer user_data)
{
    ButtonData *button_data = user_data;

    button_data->switch_location_timer = 0;

    switch_location (button_data);
}

static void
check_switch_location_timer (ButtonData *button_data)
{
    if (button_data->switch_location_timer)
    {
        return;
    }

    button_data->switch_location_timer =
        g_timeout_add_once (HOVER_TIMEOUT, switch_location_timer, button_data);
}

static void
remove_switch_location_timer (ButtonData *button_data)
{
    if (button_data->switch_location_timer != 0)
    {
        g_source_remove (button_data->switch_location_timer);
        button_data->switch_location_timer = 0;
    }
}

static GdkDragAction
on_drag_motion (GtkDropTarget *target,
                gdouble        x,
                gdouble        y,
                gpointer       user_data)
{
    ButtonData *button_data = user_data;
    AmmonitePathBar *self = button_data->path_bar;
    GdkDragAction action = 0;
    const GValue *value;
    graphene_point_t start;

    if (ammonite_window_slot_get_mode (self->slot) != AMMONITE_MODE_BROWSE)
    {
        gtk_drop_target_reject (target);
    }

    value = gtk_drop_target_get_value (target);
    if (value == NULL)
    {
        return 0;
    }

    if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GSList *items = g_value_get_boxed (value);
        if (items != NULL)
        {
            action = ammonite_dnd_get_preferred_action (button_data->file, items->data);
        }
    }
    else if (G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
    {
        action = GDK_ACTION_COPY;
    }

    start = button_data->hover_start_point;
    if (gtk_drag_check_threshold (button_data->button, start.x, start.y, x, y))
    {
        remove_switch_location_timer (button_data);
        check_switch_location_timer (button_data);
        button_data->hover_start_point.x = x;
        button_data->hover_start_point.y = y;
    }

    return action;
}

static void
on_drag_leave (GtkDropTarget *target,
               gpointer       user_data)
{
    remove_switch_location_timer (user_data);
}

static gboolean
on_drag_drop (GtkDropTarget *target,
              const GValue  *value,
              gdouble        x,
              gdouble        y,
              gpointer       user_data)
{
    ButtonData *button_data = user_data;
    AmmonitePathBar *self = button_data->path_bar;
    AmmoniteFilesView *target_view;
    g_autoptr (GFile) target_location = NULL;
    GdkDragAction action;

    g_return_val_if_fail (self->slot != NULL, FALSE);

    if (ammonite_window_slot_get_mode (self->slot) != AMMONITE_MODE_BROWSE)
    {
        return FALSE;
    }

    target_view = ammonite_window_slot_get_current_view (self->slot);

    if (target_view == NULL)
    {
        return FALSE;
    }

    target_location = ammonite_file_get_location (button_data->file);
    action = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));

    #ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (self))))
    {
        /* Temporary workaround until the below GTK MR (or equivalent fix)
         * is merged.  Without this fix, the preferred action isn't set correctly.
         * https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4982 */
        GdkDrag *drag = gdk_drop_get_drag (gtk_drop_target_get_current_drop (target));
        action = gdk_drag_get_selected_action (drag);
    }
    #endif

    return ammonite_dnd_perform_drop (target_view, value, action, target_location);
}

static GIcon *
get_gicon_for_mount (ButtonData *button_data)
{
    GIcon *icon;
    GMount *mount;

    icon = NULL;
    mount = ammonite_get_mounted_mount_for_root (button_data->path);

    if (mount != NULL)
    {
        icon = g_mount_get_symbolic_icon (mount);
        g_object_unref (mount);
    }

    return icon;
}

static GIcon *
get_gicon (ButtonData *button_data)
{
    switch (button_data->type)
    {
        case ROOT_BUTTON:
        case ADMIN_ROOT_BUTTON:
        {
            return g_themed_icon_new (AMMONITE_ICON_FILESYSTEM);
        }

        case HOME_BUTTON:
        {
            return g_themed_icon_new (AMMONITE_ICON_HOME);
        }

        case MOUNT_BUTTON:
        {
            return get_gicon_for_mount (button_data);
        }

        case STARRED_BUTTON:
        {
            return g_themed_icon_new ("starred-symbolic");
        }

        case RECENT_BUTTON:
        {
            return g_themed_icon_new ("document-open-recent-symbolic");
        }

        case TRASH_BUTTON:
        {
            return ammonite_trash_monitor_get_symbolic_icon ();
        }

        case NETWORK_BUTTON:
        {
            return g_themed_icon_new ("folder-remote-symbolic");
        }

        default:
        {
            return NULL;
        }
    }

    return NULL;
}

static void
ammonite_path_bar_update_button_appearance (ButtonData *button_data,
                                            gboolean    current_dir)
{
    const gchar *dir_name = get_dir_name (button_data);
    gint min_chars = AMMONITE_PATH_BAR_BUTTON_ELLIPSIZE_MINIMUM_CHARS;
    GIcon *icon;

    if (button_data->label != NULL)
    {
        gtk_label_set_text (GTK_LABEL (button_data->label), dir_name);

        gtk_widget_set_tooltip_text (button_data->button, dir_name);

        if (current_dir)
        {
            /* We want to avoid ellipsizing the current directory name, but
             * still need to set a limit. */
            min_chars = 4 * min_chars;
        }

        /* Labels can ellipsize until they become a single ellipsis character.
         * We don't want that, so we must set a minimum.
         *
         * However, for labels shorter than the minimum, setting this minimum
         * width would make them unnecessarily wide. In that case, just make it
         * not ellipsize instead.
         *
         * Due to variable width fonts, labels can be shorter than the space
         * that would be reserved by setting a minimum amount of characters.
         * Compensate for this with a tolerance of +50% characters.
         */
        if (g_utf8_strlen (dir_name, -1) > min_chars * 1.5)
        {
            gtk_label_set_width_chars (GTK_LABEL (button_data->label), min_chars);
            gtk_label_set_ellipsize (GTK_LABEL (button_data->label), PANGO_ELLIPSIZE_MIDDLE);
        }
        else
        {
            gtk_label_set_width_chars (GTK_LABEL (button_data->label), -1);
            gtk_label_set_ellipsize (GTK_LABEL (button_data->label), PANGO_ELLIPSIZE_NONE);
        }
    }

    icon = get_gicon (button_data);
    if (icon != NULL)
    {
        gtk_image_set_from_gicon (GTK_IMAGE (button_data->image), icon);
        gtk_widget_set_visible (GTK_WIDGET (button_data->image), TRUE);
        g_object_unref (icon);
    }
    else
    {
        gtk_widget_set_visible (GTK_WIDGET (button_data->image), FALSE);
    }
}

static void
ammonite_path_bar_update_button_state (ButtonData *button_data,
                                       gboolean    current_dir)
{
    if (button_data->label != NULL)
    {
        gtk_label_set_label (GTK_LABEL (button_data->label), NULL);
    }

    ammonite_path_bar_update_button_appearance (button_data, current_dir);
}

static void
setup_button_type (ButtonData      *button_data,
                   AmmonitePathBar *self,
                   GFile           *location)
{
    g_autoptr (GMount) mount = NULL;

    if (ammonite_is_root_directory (location))
    {
        button_data->type = ROOT_BUTTON;
    }
    else if (ammonite_is_home_directory (location))
    {
        button_data->type = HOME_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (g_file_has_uri_scheme (location, SCHEME_RECENT))
    {
        button_data->type = RECENT_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (g_file_has_uri_scheme (location, SCHEME_STARRED))
    {
        button_data->type = STARRED_BUTTON;
        button_data->is_root = TRUE;
    }
    else if ((mount = ammonite_get_mounted_mount_for_root (location)) != NULL)
    {
        button_data->dir_name = g_mount_get_name (mount);
        button_data->type = MOUNT_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (ammonite_is_root_for_scheme (location, SCHEME_ADMIN))
    {
        button_data->type = ADMIN_ROOT_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (ammonite_is_root_for_scheme (location, SCHEME_TRASH))
    {
        button_data->type = TRASH_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (ammonite_is_root_for_scheme (location, SCHEME_NETWORK_VIEW) ||
             ammonite_is_root_for_scheme (location, SCHEME_NETWORK))
    {
        button_data->type = NETWORK_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (ammonite_is_root_for_scheme (location, SCHEME_COMPUTER))
    {
        button_data->type = COMPUTER_BUTTON;
        button_data->is_root = TRUE;
    }
    else if (ammonite_is_root_for_scheme (location, SCHEME_BURN))
    {
        button_data->type = BURN_BUTTON;
        button_data->is_root = TRUE;
    }
    else
    {
        button_data->type = NORMAL_BUTTON;
    }
}

static void
button_data_file_changed (AmmoniteFile *file,
                          ButtonData   *button_data)
{
    GtkWidget *ancestor;
    GFile *location;
    GFile *current_location;
    GFile *parent;
    GFile *button_parent;
    ButtonData *current_button_data;
    AmmonitePathBar *self;
    gboolean renamed;
    gboolean child;
    gboolean current_dir;

    ancestor = gtk_widget_get_ancestor (button_data->button, AMMONITE_TYPE_PATH_BAR);
    if (ancestor == NULL)
    {
        return;
    }
    self = AMMONITE_PATH_BAR (ancestor);

    g_return_if_fail (self->current_path != NULL);
    g_return_if_fail (self->current_button_data != NULL);

    current_button_data = self->current_button_data;

    location = ammonite_file_get_location (file);
    if (!g_file_equal (button_data->path, location))
    {
        parent = g_file_get_parent (location);
        button_parent = g_file_get_parent (button_data->path);

        renamed = (parent != NULL && button_parent != NULL) &&
                  g_file_equal (parent, button_parent);

        if (parent != NULL)
        {
            g_object_unref (parent);
        }
        if (button_parent != NULL)
        {
            g_object_unref (button_parent);
        }

        if (renamed)
        {
            g_set_object (&button_data->path, location);
        }
        else
        {
            /* the file has been moved.
             * If it was below the currently displayed location, remove it.
             * If it was not below the currently displayed location, update the path bar
             */
            child = g_file_has_prefix (button_data->path,
                                       self->current_path);

            if (child)
            {
                /* moved file inside current path hierarchy */
                current_location = g_object_ref (self->current_path);
            }
            else
            {
                /* moved current path, or file outside current path hierarchy.
                 * Update path bar to new locations.
                 */
                current_location = ammonite_file_get_location (current_button_data->file);
            }

            ammonite_path_bar_set_path (self, current_location);
            g_object_unref (location);
            g_object_unref (current_location);
            return;
        }
    }
    else if (ammonite_file_is_gone (file))
    {
        /* Do nothing here, the view will set new path if needed. */
        g_object_unref (location);
        return;
    }
    g_object_unref (location);

    /* MOUNTs use the GMount as the name, so don't update for those */
    if (button_data->type != MOUNT_BUTTON)
    {
        g_set_str (&button_data->dir_name, ammonite_file_get_display_name (file));
    }
    current_dir = g_file_equal (self->current_path, button_data->path);
    ammonite_path_bar_update_button_appearance (button_data, current_dir);
}

static ButtonData *
make_button_data (AmmonitePathBar *self,
                  AmmoniteFile    *file,
                  gboolean         current_dir)
{
    GFile *path;
    GtkWidget *child = NULL;
    GtkEventController *controller;
    GtkDropTarget *target;
    ButtonData *button_data;

    path = ammonite_file_get_location (file);

    /* Is it a special button? */
    button_data = g_new0 (ButtonData, 1);

    setup_button_type (button_data, self, path);
    button_data->button = gtk_button_new ();
    gtk_widget_set_focus_on_click (button_data->button, FALSE);
    gtk_widget_add_css_class (button_data->button, "ammonite-path-button");

    /* TODO update button type when xdg directories change */

    button_data->image = gtk_image_new ();

    switch (button_data->type)
    {
        case ROOT_BUTTON:
        case ADMIN_ROOT_BUTTON:
        case HOME_BUTTON:
        case MOUNT_BUTTON:
        case TRASH_BUTTON:
        case RECENT_BUTTON:
        case STARRED_BUTTON:
        case NETWORK_BUTTON:
        case COMPUTER_BUTTON:
        case BURN_BUTTON:
        {
            button_data->label = gtk_label_new (NULL);
            child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
            button_data->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_box_append (GTK_BOX (button_data->container), button_data->button);

            gtk_accessible_update_relation (GTK_ACCESSIBLE (button_data->image),
                                            GTK_ACCESSIBLE_RELATION_LABELLED_BY,
                                            button_data->label, NULL,
                                            -1);

            gtk_box_append (GTK_BOX (child), button_data->image);
            gtk_box_append (GTK_BOX (child), button_data->label);
        }
        break;

        case NORMAL_BUTTON:
        /* Fall through */
        default:
        {
            GtkWidget *separator_label;

            separator_label = gtk_label_new (G_DIR_SEPARATOR_S);
            gtk_widget_add_css_class (separator_label, "dimmed");
            button_data->label = gtk_label_new (NULL);
            child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
            button_data->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_box_append (GTK_BOX (button_data->container), separator_label);
            gtk_box_append (GTK_BOX (button_data->container), button_data->button);

            gtk_widget_set_visible (button_data->image, FALSE);
            gtk_box_append (GTK_BOX (child), button_data->image);
            gtk_box_append (GTK_BOX (child), button_data->label);
        }
        break;
    }

    if (current_dir)
    {
        gtk_widget_add_css_class (button_data->button, "current-dir");
        gtk_widget_set_hexpand (button_data->button, TRUE);
        gtk_widget_set_halign (button_data->label, GTK_ALIGN_START);
        gtk_actionable_set_action_name (GTK_ACTIONABLE (button_data->button), "toolbar.edit-location");
    }

    if (button_data->label != NULL)
    {
        PangoAttrList *attrs;

        gtk_label_set_single_line_mode (GTK_LABEL (button_data->label), TRUE);

        attrs = pango_attr_list_new ();
        pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes (GTK_LABEL (button_data->label), attrs);
        pango_attr_list_unref (attrs);

        if (!current_dir)
        {
            gtk_widget_add_css_class (button_data->label, "dimmed");
            gtk_widget_add_css_class (button_data->image, "dimmed");
        }
    }

    button_data->path = g_object_ref (path);
    button_data->dir_name = g_strdup (ammonite_file_get_display_name (file));
    button_data->file = ammonite_file_ref (file);
    ammonite_file_monitor_add (button_data->file, button_data,
                               AMMONITE_ATTRIBUTE_INFO);
    button_data->file_changed_signal_id =
        g_signal_connect (button_data->file, "changed",
                          G_CALLBACK (button_data_file_changed),
                          button_data);

    gtk_button_set_child (GTK_BUTTON (button_data->button), child);
    gtk_widget_set_visible (button_data->container, TRUE);

    button_data->path_bar = self;

    ammonite_path_bar_update_button_state (button_data, current_dir);

    g_signal_connect (button_data->button, "clicked", G_CALLBACK (button_clicked_cb), button_data);

    /* A gesture is needed here, because GtkButton doesn’t react to middle- or
     * secondary-clicking.
     */
    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (button_data->button, controller);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_click_gesture_pressed), button_data);

    /* TODO: Implement GDK_ACTION_ASK */
    target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_ALL);

    gtk_drop_target_set_preload (target, TRUE);
    /* TODO: Implement GDK_TYPE_STRING */
    gtk_drop_target_set_gtypes (target, (GType[2]) { GDK_TYPE_TEXTURE, GDK_TYPE_FILE_LIST }, 2);
    g_signal_connect (target, "enter", G_CALLBACK (on_drag_motion), button_data);
    g_signal_connect (target, "motion", G_CALLBACK (on_drag_motion), button_data);
    g_signal_connect (target, "drop", G_CALLBACK (on_drag_drop), button_data);
    g_signal_connect (target, "leave", G_CALLBACK (on_drag_leave), button_data);
    gtk_widget_add_controller (button_data->button, GTK_EVENT_CONTROLLER (target));

    g_object_unref (path);

    return button_data;
}

static void
ammonite_path_bar_update_path (AmmonitePathBar *self,
                               GFile           *file_path)
{
    AmmoniteFile *file;
    gboolean first_directory;
    GList *new_buttons, *l;
    ButtonData *button_data;

    g_return_if_fail (AMMONITE_IS_PATH_BAR (self));
    g_return_if_fail (file_path != NULL);

    first_directory = TRUE;
    new_buttons = NULL;

    file = ammonite_file_get (file_path);

    while (file != NULL)
    {
        AmmoniteFile *parent_file;

        parent_file = ammonite_file_get_parent (file);
        button_data = make_button_data (self, file, first_directory);
        ammonite_file_unref (file);

        if (first_directory)
        {
            first_directory = FALSE;
        }

        new_buttons = g_list_prepend (new_buttons, button_data);

        if (parent_file != NULL &&
            button_data->is_root)
        {
            ammonite_file_unref (parent_file);
            break;
        }

        file = parent_file;
    }

    ammonite_path_bar_clear_buttons (self);

    /* Buttons are listed in reverse order such that the current location is
     * always the first link. */
    self->button_list = g_list_reverse (new_buttons);

    for (l = self->button_list; l; l = l->next)
    {
        GtkWidget *container;
        container = BUTTON_DATA (l->data)->container;
        gtk_box_prepend (GTK_BOX (self->buttons_box), container);
    }
}

void
ammonite_path_bar_set_path (AmmonitePathBar *self,
                            GFile           *file_path)
{
    ButtonData *button_data;

    g_return_if_fail (AMMONITE_IS_PATH_BAR (self));
    g_return_if_fail (file_path != NULL);

    ammonite_path_bar_update_path (self, file_path);
    button_data = g_list_nth_data (self->button_list, 0);

    g_set_object (&self->current_path, file_path);
    self->current_button_data = button_data;
}
