/*
 * SPDX-FileCopyrightText: 2024 GNOME Foundation Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#define G_LOG_DOMAIN "ammonite-file-chooser"

#include "ammonite-file-chooser.h"

#include <config.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ammonite-directory.h"
#include "ammonite-enum-types.h"
#include "ammonite-file.h"
#include "ammonite-file-utilities.h"
#include "ammonite-filename-utilities.h"
#include "ammonite-filename-validator.h"
#include "ammonite-global-preferences.h"
#include "ammonite-scheme.h"
#include "ammonite-shortcut-manager.h"
#include "ammonite-sidebar.h"
#include "ammonite-toolbar.h"
#include "ammonite-view-item-filter.h"
#include "ammonite-window-slot.h"

#define FILTER_WIDTH_CHARS 12

struct _AmmoniteFileChooser
{
    AdwWindow parent_instance;

    GActionGroup *chooser_action_group;

    AmmoniteMode mode;
    char *accept_label;
    char *suggested_name;
    gboolean flag_initial_focus_done;

    GtkWidget *split_view;
    GtkWidget *places_sidebar;
    AmmoniteToolbar *toolbar;
    AdwBin *slot_container;
    AmmoniteWindowSlot *slot;
    GtkDropDown *filters_dropdown;
    GtkWidget *choices_menu_button;
    GtkWidget *read_only_checkbox;
    GtkWidget *accept_button;
    GtkWidget *filename_widget;
    GtkWidget *filename_button_container;
    GtkWidget *filename_undo_button;
    GtkWidget *filename_entry;
    GtkWidget *new_folder_button;
    GtkWidget *title_widget;

    AmmoniteFilenameValidator *validator;
    AdwBreakpoint *breakpoint;
};

G_DEFINE_FINAL_TYPE (AmmoniteFileChooser, ammonite_file_chooser, ADW_TYPE_WINDOW)

enum
{
    PROP_0,
    PROP_MODE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

enum
{
    SIGNAL_ACCEPTED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void open_filename_entry (AmmoniteFileChooser *self);

static gboolean
mode_can_accept_files (AmmoniteMode      mode,
                       AmmoniteFileList *files)
{
    if (files == NULL)
    {
        return FALSE;
    }

    gboolean exactly_one_item = (files->next == NULL);

    switch (mode)
    {
        case AMMONITE_MODE_OPEN_FILE:
        case AMMONITE_MODE_SAVE_FILE:
        {
            gboolean is_folder = ammonite_file_opens_in_view (AMMONITE_FILE (files->data));

            return (exactly_one_item && !is_folder);
        }

        case AMMONITE_MODE_OPEN_FOLDER:
        {
            gboolean is_folder = ammonite_file_opens_in_view (AMMONITE_FILE (files->data));

            return (exactly_one_item && is_folder);
        }

        case AMMONITE_MODE_SAVE_FILES:
        {
            return FALSE;
        }

        case AMMONITE_MODE_OPEN_FILES:
        case AMMONITE_MODE_OPEN_FOLDERS:
        {
            for (AmmoniteFileList *l = files; l != NULL; l = l->next)
            {
                gboolean is_folder = ammonite_file_opens_in_view (AMMONITE_FILE (l->data));

                if (mode != (is_folder ?
                             AMMONITE_MODE_OPEN_FOLDERS : AMMONITE_MODE_OPEN_FILES))
                {
                    return FALSE;
                }
            }

            return TRUE;
        }

        default:
        {
            g_assert_not_reached ();
        }
    }

    return FALSE;
}

static gboolean
mode_can_accept_current_directory (AmmoniteMode  mode,
                                   GFile        *location)
{
    if (location == NULL)
    {
        return FALSE;
    }

    switch (mode)
    {
        case AMMONITE_MODE_OPEN_FOLDER:
        case AMMONITE_MODE_OPEN_FOLDERS:
        case AMMONITE_MODE_SAVE_FILE:
        case AMMONITE_MODE_SAVE_FILES:
        {
            g_autofree char *scheme = g_file_get_uri_scheme (location);

            return !ammonite_scheme_is_internal (scheme);
        }

        case AMMONITE_MODE_OPEN_FILE:
        case AMMONITE_MODE_OPEN_FILES:
        {
            return FALSE;
        }

        default:
        {
            g_assert_not_reached ();
        }
    }

    return FALSE;
}

static gboolean
file_chooser_has_filename_entry (AmmoniteFileChooser *self)
{
    return (self->mode == AMMONITE_MODE_SAVE_FILE);
}

static gboolean
ammonite_file_chooser_can_accept (AmmoniteFileChooser *self,
                                  AmmoniteFileList    *files,
                                  GFile               *location,
                                  gboolean             filename_passed)
{
    if (self->mode == AMMONITE_MODE_SAVE_FILE)
    {
        return (filename_passed &&
                mode_can_accept_current_directory (self->mode, location));
    }
    else
    {
        return (mode_can_accept_files (self->mode, files) ||
                mode_can_accept_current_directory (self->mode, location));
    }
}

static void
emit_accepted (AmmoniteFileChooser *self,
               GList               *file_locations)
{
    g_signal_emit (self, signals[SIGNAL_ACCEPTED], 0,
                   file_locations,
                   GTK_FILE_FILTER (gtk_drop_down_get_selected_item (self->filters_dropdown)));
}

static void
on_overwrite_confirm_response (AdwAlertDialog      *dialog,
                               GAsyncResult        *result,
                               AmmoniteFileChooser *self)
{
    const char *response = adw_alert_dialog_choose_finish (dialog, result);

    if (g_strcmp0 (response, "replace") == 0)
    {
        GFile *parent_location = ammonite_window_slot_get_location (self->slot);
        g_autofree char *new_filename = ammonite_filename_validator_get_new_name (self->validator);
        g_autoptr (GFile) new_file_location = g_file_get_child (parent_location, new_filename);

        emit_accepted (self, &(GList){ .data = new_file_location });
    }
    else
    {
        /* The user probably wants to rename the file name, so focus entry */
        open_filename_entry (self);
    }
}

static void
ask_confirm_overwrite (AmmoniteFileChooser *self)
{
    AdwDialog *dialog = adw_alert_dialog_new (_("Replace When Saving?"), NULL);
    g_autofree char *filename = ammonite_filename_validator_get_new_name (self->validator);
    g_autoptr (AmmoniteFile) directory_as_file = ammonite_file_get (ammonite_window_slot_get_location (self->slot));
    const char *directory_name = ammonite_file_get_display_name (directory_as_file);

    adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog),
                                  _("“%s” already exists in “%s”. Saving will replace or overwrite its contents."),
                                  filename, directory_name);

    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                    "cancel", _("_Cancel"),
                                    "replace", _("_Replace"),
                                    NULL);

    adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                              "replace",
                                              ADW_RESPONSE_DESTRUCTIVE);

    adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "cancel");
    adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

    adw_alert_dialog_choose (ADW_ALERT_DIALOG (dialog), GTK_WIDGET (self),
                             NULL, (GAsyncReadyCallback) on_overwrite_confirm_response, self);
}

static GFile *
get_file_chooser_activation_location (AmmoniteFile *file)
{
    g_autoptr (GFile) location = ammonite_file_get_location (file);
    const gchar *path = g_file_peek_path (location);

    if (path != NULL)
    {
        return g_steal_pointer (&location);
    }

    return ammonite_file_get_activation_location (file);
}

static void
action_accept (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
    AmmoniteFileChooser *self = AMMONITE_FILE_CHOOSER (user_data);

    if (self->mode == AMMONITE_MODE_SAVE_FILE)
    {
        if (ammonite_filename_validator_get_will_overwrite (self->validator))
        {
            ask_confirm_overwrite (self);
        }
        else
        {
            GFile *parent_location = ammonite_window_slot_get_location (self->slot);
            g_autofree char *name = ammonite_filename_validator_get_new_name (self->validator);
            g_autoptr (GFile) file_location = g_file_get_child (parent_location, name);

            emit_accepted (self, &(GList){ .data = file_location });
        }
    }
    else
    {
        AmmoniteFileList *selection = ammonite_window_slot_get_selection (self->slot);

        if (mode_can_accept_files (self->mode, selection))
        {
            g_autolist (GFile) file_locations = g_list_copy_deep (selection,
                                                                  (GCopyFunc) get_file_chooser_activation_location,
                                                                  NULL);

            emit_accepted (self, file_locations);
        }
        else
        {
            GFile *location = ammonite_window_slot_get_location (self->slot);

            emit_accepted (self, &(GList){ .data = location });
        }
    }
}

static void
action_focus_entry (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    AmmoniteFileChooser *self = user_data;

    if (file_chooser_has_filename_entry (self))
    {
        open_filename_entry (self);
    }
}

static void
on_validator_has_feedback_changed (AmmoniteFileChooser *self)
{
    gboolean has_feedback;

    g_object_get (self->validator,
                  "has-feedback", &has_feedback,
                  NULL);
    if (has_feedback)
    {
        gtk_widget_add_css_class (self->filename_entry, "warning");
    }
    else
    {
        gtk_widget_remove_css_class (self->filename_entry, "warning");
    }
}

static void
on_validator_will_overwrite_changed (AmmoniteFileChooser *self)
{
    if (ammonite_filename_validator_get_will_overwrite (self->validator))
    {
        gtk_widget_remove_css_class (self->accept_button, "suggested-action");
        gtk_widget_add_css_class (self->accept_button, "destructive-action");
        gtk_button_set_label (GTK_BUTTON (self->accept_button), _("_Replace"));
    }
    else
    {
        gtk_widget_remove_css_class (self->accept_button, "destructive-action");
        gtk_widget_add_css_class (self->accept_button, "suggested-action");
        gtk_button_set_label (GTK_BUTTON (self->accept_button), self->accept_label);
    }
}

static void
update_cursor (AmmoniteFileChooser *self)
{
    if (self->slot != NULL &&
        ammonite_window_slot_get_allow_stop (self->slot))
    {
        gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "progress");
    }
    else
    {
        gtk_widget_set_cursor (GTK_WIDGET (self), NULL);
    }
}

static void
on_file_drop (GtkDropTarget *target,
              const GValue  *value,
              gdouble        x,
              gdouble        y,
              gpointer       user_data)
{
    GSList *locations = g_value_get_boxed (value);

    g_return_if_fail (locations != NULL);

    g_autolist (AmmoniteFile) selection = NULL;
    g_autoptr (GFile) location = NULL;
    AmmoniteFileChooser *self = user_data;

    for (GSList *l = locations; l != NULL; l = l->next)
    {
        selection = g_list_prepend (selection, ammonite_file_get (l->data));
    }

    selection = g_list_reverse (selection);

    if (ammonite_file_opens_in_view (selection->data) &&
        self->mode != AMMONITE_MODE_OPEN_FOLDER &&
        self->mode != AMMONITE_MODE_OPEN_FOLDERS)
    {
        /* If it's a folder go into the folder unless you want to open that folder */
        location = g_object_ref (locations->data);
    }
    else
    {
        location = g_file_get_parent (locations->data);
    }

    ammonite_window_slot_open_location_full (self->slot, location, selection);
}

static void
on_slot_activate_files (AmmoniteFileChooser *self,
                        GList               *files)
{
    if (gtk_widget_get_sensitive (self->accept_button) &&
        mode_can_accept_files (self->mode, files))
    {
        gtk_widget_activate (self->accept_button);
    }
}

static void
on_filename_entry_focus_leave (AmmoniteFileChooser *self)
{
    /* The filename entry is a transient: it should hide when it loses focus.
     *
     * However, if we lose focus because the window itself lost focus, then the
     * filename entry should persist, because this may happen due to the user
     * switching keyboard layout/input method; or they may want to copy/drop
     * an path from another window/app. We detect this case by looking at the
     * focus widget of the window (GtkRoot).
     */

    GtkWidget *focus_widget = gtk_root_get_focus (gtk_widget_get_root (GTK_WIDGET (self)));
    if (focus_widget != NULL &&
        gtk_widget_is_ancestor (focus_widget, GTK_WIDGET (self->filename_entry)))
    {
        return;
    }

    gtk_stack_set_visible_child (GTK_STACK (self->filename_widget),
                                 self->filename_button_container);
}

static void
on_filename_entry_changed (AmmoniteFileChooser *self)
{
    const char *current_text = gtk_editable_get_text (GTK_EDITABLE (self->filename_entry));
    gboolean is_not_suggested_text = (g_strcmp0 (self->suggested_name, current_text) != 0 &&
                                      self->suggested_name != NULL);

    gtk_widget_set_visible (self->filename_undo_button, is_not_suggested_text);

    ammonite_filename_validator_validate (self->validator);
}

static void
open_filename_entry (AmmoniteFileChooser *self)
{
    gtk_stack_set_visible_child (GTK_STACK (self->filename_widget),
                                 self->filename_entry);
    gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->filename_entry));

    const char *filename = gtk_editable_get_text (GTK_EDITABLE (self->filename_entry));
    int extension_offset = ammonite_filename_get_extension_char_offset (filename);
    gtk_editable_select_region (GTK_EDITABLE (self->filename_entry), 0, extension_offset);
}

static void
on_filename_undo_button_clicked (AmmoniteFileChooser *self)
{
    gtk_editable_set_text (GTK_EDITABLE (self->filename_entry), self->suggested_name);

    ammonite_window_slot_open_location_full (self->slot, ammonite_window_slot_get_location (self->slot), NULL);
}

static void
on_slot_selection_notify (AmmoniteFileChooser *self)
{
    g_return_if_fail (self->mode == AMMONITE_MODE_SAVE_FILE);

    AmmoniteSelectionSource selection_source
        = ammonite_window_slot_get_selection_source (self->slot);

    if (!selection_source_is_intentional (selection_source))
    {
        /* If the selection is auto, don't override the suggested name with the
         *  auto-selected file. */
        return;
    }

    AmmoniteFileList *selection = ammonite_window_slot_get_selection (self->slot);

    if (mode_can_accept_files (self->mode, selection))
    {
        AmmoniteFile *file = AMMONITE_FILE (selection->data);

        gtk_editable_set_text (GTK_EDITABLE (self->filename_entry),
                               ammonite_file_get_edit_name (file));
    }
}

static void
on_location_changed (AmmoniteFileChooser *self)
{
    if (self->mode != AMMONITE_MODE_SAVE_FILE)
    {
        return;
    }

    g_autoptr (AmmoniteDirectory) directory = NULL;
    GFile *location = ammonite_window_slot_get_location (self->slot);
    g_autofree char *scheme = g_file_get_uri_scheme (location);

    if (ammonite_scheme_is_internal (scheme))
    {
        return;
    }

    directory = ammonite_directory_get (location);
    ammonite_filename_validator_set_containing_directory (self->validator, directory);
    ammonite_filename_validator_validate (self->validator);
}

static gboolean
on_key_pressed_bubble (GtkEventControllerKey *controller,
                       unsigned int           keyval,
                       unsigned int           keycode,
                       GdkModifierType        state,
                       gpointer               user_data)
{
    AmmoniteFileChooser *self = AMMONITE_FILE_CHOOSER (user_data);

    if (self->slot != NULL &&
        ammonite_window_slot_handle_event (self->slot, controller, keyval, state))
    {
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void
on_click_gesture_pressed (GtkGestureClick *gesture,
                          gint             n_press,
                          gdouble          x,
                          gdouble          y,
                          gpointer         user_data)
{
    AmmoniteFileChooser *self = user_data;
    guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    if (ammonite_global_preferences_get_use_extra_buttons () &&
        (button == ammonite_global_preferences_get_back_button ()))
    {
        ammonite_window_slot_navigate (self->slot, -1);
    }
    else if (ammonite_global_preferences_get_use_extra_buttons () &&
             (button == ammonite_global_preferences_get_forward_button ()))
    {
        ammonite_window_slot_navigate (self->slot, 1);
    }
}

static int
get_filter_width_chars (GtkListItem *listitem,
                        const char  *name)
{
    return MIN (g_utf8_strlen (name, -1), FILTER_WIDTH_CHARS);
}

static void
update_dropdown_checkmark (GtkDropDown *dropdown,
                           GParamSpec  *psepc,
                           GtkListItem *list_item)
{
    guint selected = gtk_drop_down_get_selected (dropdown);
    GtkWidget *cell = gtk_list_item_get_child (list_item);
    GtkWidget *check_mark = gtk_widget_get_last_child (cell);
    gdouble opacity = (selected == gtk_list_item_get_position (list_item)) ? 1 : 0;

    gtk_widget_set_opacity (check_mark, opacity);
}

static void
filters_dropdown_setup (GtkListItemFactory *factory,
                        GtkListItem        *list_item,
                        gpointer            user_data)
{
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *label = gtk_label_new (NULL);
    GtkWidget *icon;

    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_box_append (GTK_BOX (box), label);
    icon = g_object_new (GTK_TYPE_IMAGE,
                         "icon-name", "object-select-symbolic",
                         "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                         NULL);
    gtk_box_append (GTK_BOX (box), icon);
    gtk_list_item_set_child (list_item, box);

    g_assert (gtk_widget_get_first_child (box) == label &&
              gtk_widget_get_last_child (box) == icon);
}

static void
filters_dropdown_bind (GtkListItemFactory *factory,
                       GtkListItem        *list_item,
                       gpointer            user_data)
{
    AmmoniteFileChooser *self = user_data;
    GtkFileFilter *filter = gtk_list_item_get_item (list_item);
    GtkWidget *cell = gtk_list_item_get_child (list_item);
    GtkWidget *label = gtk_widget_get_first_child (cell);

    gtk_label_set_label (GTK_LABEL (label), gtk_file_filter_get_name (filter));

    g_signal_connect (self->filters_dropdown, "notify::selected", G_CALLBACK (update_dropdown_checkmark), list_item);
    update_dropdown_checkmark (self->filters_dropdown, NULL, list_item);
}

static void
filters_dropdown_unbind (GtkListItemFactory *factory,
                         GtkListItem        *list_item,
                         gpointer            user_data)
{
    AmmoniteFileChooser *self = user_data;

    g_signal_handlers_disconnect_by_func (self->filters_dropdown, update_dropdown_checkmark, list_item);
}

static gboolean
title_widget_query_tooltip (GtkWidget  *widget,
                            gint        x,
                            gint        y,
                            gboolean    keyboard_mode,
                            GtkTooltip *tooltip,
                            gpointer    user_data)
{
    PangoLayout *layout = gtk_label_get_layout (GTK_LABEL (widget));

    if (pango_layout_is_ellipsized (layout))
    {
        const char *label = gtk_label_get_label (GTK_LABEL (widget));
        gtk_tooltip_set_text (tooltip, label);
        return TRUE;
    }

    return FALSE;
}


static void
ammonite_file_chooser_dispose (GObject *object)
{
    AmmoniteFileChooser *self = (AmmoniteFileChooser *) object;

    if (self->slot != NULL)
    {
        g_assert (adw_bin_get_child (self->slot_container) == GTK_WIDGET (self->slot));

        ammonite_window_slot_set_active (self->slot, FALSE);
        /* Let bindings on AdwToolbarView:content react to the slot being unset
         * while the slot itself is still alive. */
        adw_bin_set_child (self->slot_container, NULL);
        g_clear_object (&self->slot);
    }

    G_OBJECT_CLASS (ammonite_file_chooser_parent_class)->dispose (object);
}

static void
ammonite_file_chooser_finalize (GObject *object)
{
    AmmoniteFileChooser *self = (AmmoniteFileChooser *) object;

    g_free (self->accept_label);
    g_free (self->suggested_name);

    G_OBJECT_CLASS (ammonite_file_chooser_parent_class)->finalize (object);
}

static void
ammonite_file_chooser_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    AmmoniteFileChooser *self = AMMONITE_FILE_CHOOSER (object);

    switch (prop_id)
    {
        case PROP_MODE:
        {
            g_value_set_enum (value, self->mode);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_file_chooser_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    AmmoniteFileChooser *self = AMMONITE_FILE_CHOOSER (object);

    switch (prop_id)
    {
        case PROP_MODE:
        {
            self->mode = g_value_get_enum (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static gboolean
ammonite_file_chooser_grab_focus (GtkWidget *widget)
{
    AmmoniteFileChooser *self = AMMONITE_FILE_CHOOSER (widget);

    if (self->mode == AMMONITE_MODE_SAVE_FILE && !self->flag_initial_focus_done)
    {
        self->flag_initial_focus_done = TRUE;
        open_filename_entry (self);
    }
    else if (self->slot != NULL)
    {
        return gtk_widget_grab_focus (GTK_WIDGET (self->slot));
    }

    return GTK_WIDGET_CLASS (ammonite_file_chooser_parent_class)->grab_focus (widget);
}

static void
ammonite_file_chooser_constructed (GObject *object)
{
    G_OBJECT_CLASS (ammonite_file_chooser_parent_class)->constructed (object);

    AmmoniteFileChooser *self = (AmmoniteFileChooser *) object;

    /* Setup slot. */
    self->slot = ammonite_window_slot_new (self->mode);
    g_signal_connect_swapped (self->slot, "notify::location", G_CALLBACK (on_location_changed), self);
    adw_bin_set_child (self->slot_container, GTK_WIDGET (self->slot));
    ammonite_window_slot_set_active (self->slot, TRUE);
    g_signal_connect_swapped (self->slot, "notify::allow-stop",
                              G_CALLBACK (update_cursor), self);
    g_signal_connect_swapped (self->slot, "activate-files",
                              G_CALLBACK (on_slot_activate_files), self);

    g_autoptr (AmmoniteViewItemFilter) filter = ammonite_view_item_filter_new ();

    g_object_bind_property (self->filters_dropdown, "selected-item",
                            filter, "file-filter",
                            G_BINDING_SYNC_CREATE);
    ammonite_window_slot_set_filter (self->slot, GTK_FILTER (filter));

    gtk_widget_set_visible (self->filename_widget,
                            (self->mode == AMMONITE_MODE_SAVE_FILE));

    gtk_widget_set_visible (self->new_folder_button,
                            (self->mode == AMMONITE_MODE_SAVE_FILE ||
                             self->mode == AMMONITE_MODE_SAVE_FILES ||
                             self->mode == AMMONITE_MODE_OPEN_FOLDER ||
                             self->mode == AMMONITE_MODE_OPEN_FOLDERS));

    /* Add the setter here once the new folder property is set */
    adw_breakpoint_add_setters (self->breakpoint, G_OBJECT (self->toolbar),
                                "show-new-folder-button", FALSE, NULL);

    if (self->mode == AMMONITE_MODE_SAVE_FILE)
    {
        g_signal_connect_object (self->slot, "notify::selection",
                                 G_CALLBACK (on_slot_selection_notify), self,
                                 G_CONNECT_SWAPPED);
    }

    if (g_strcmp0 (PROFILE, "") != 0)
    {
        gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
    }

    gtk_widget_add_css_class (self->title_widget, "windowtitle");
    gtk_widget_add_css_class (self->title_widget, "title");
    g_signal_connect (self->title_widget, "query-tooltip",
                      G_CALLBACK (title_widget_query_tooltip), self);

    int width, height;
    g_settings_get (ammonite_window_state, AMMONITE_WINDOW_STATE_INITIAL_SIZE_FILE_CHOOSER,
                    "(ii)", &width, &height);
    gtk_window_set_default_size (GTK_WINDOW (self), width, height);
}

static const GActionEntry chooser_action_entries[] =
{
    { .name = "accept", .activate = action_accept },
    { .name = "focus-entry", .activate = action_focus_entry },
};

static void
ammonite_file_chooser_init (AmmoniteFileChooser *self)
{
    g_type_ensure (AMMONITE_TYPE_FILENAME_VALIDATOR);
    g_type_ensure (AMMONITE_TYPE_TOOLBAR);
    g_type_ensure (AMMONITE_TYPE_PLACES_SIDEBAR);
    g_type_ensure (AMMONITE_TYPE_SHORTCUT_MANAGER);
    gtk_widget_init_template (GTK_WIDGET (self));

    /* Give the dialog its own window group. Otherwise all such dialogs would
     * belong to the same default window group, acting like a stack of modals.
     */
    g_autoptr (GtkWindowGroup) window_group = gtk_window_group_new ();
    gtk_window_group_add_window (window_group, GTK_WINDOW (self));

    /* Setup sidebar */
    ammonite_sidebar_set_open_flags (AMMONITE_PLACES_SIDEBAR (self->places_sidebar),
                                     AMMONITE_OPEN_FLAG_NORMAL);
    ammonite_sidebar_set_show_trash (AMMONITE_PLACES_SIDEBAR (self->places_sidebar),
                                     FALSE);

    GtkEventController *controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (on_key_pressed_bubble), self);
    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed", G_CALLBACK (on_click_gesture_pressed), self);

    /* The factory is set in the ui, but we need to set the popup (list) factory
     * in code to make the checkmark appear correctly. */
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (filters_dropdown_setup), self);
    g_signal_connect (factory, "bind", G_CALLBACK (filters_dropdown_bind), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (filters_dropdown_unbind), self);
    gtk_drop_down_set_list_factory (self->filters_dropdown, factory);

    /* Setup Actions */
    self->chooser_action_group = G_ACTION_GROUP (g_simple_action_group_new ());
    g_action_map_add_action_entries (G_ACTION_MAP (self->chooser_action_group),
                                     chooser_action_entries,
                                     G_N_ELEMENTS (chooser_action_entries),
                                     self);
    gtk_widget_insert_action_group (GTK_WIDGET (self),
                                    "chooser",
                                    G_ACTION_GROUP (self->chooser_action_group));

    /* Bind "accept" action enabled to ammonite_file_chooser_can_accept(). */
    GAction *action = g_action_map_lookup_action (G_ACTION_MAP (self->chooser_action_group),
                                                  "accept");
    GtkExpression *container_expr, *validator_expr, *slot_expr, **expressions, *closure_expr;

    container_expr = gtk_constant_expression_new (ADW_TYPE_BIN, self->slot_container);
    validator_expr = gtk_constant_expression_new (AMMONITE_TYPE_FILENAME_VALIDATOR, self->validator);

    slot_expr = gtk_property_expression_new (ADW_TYPE_BIN, container_expr, "child"),
    expressions = (GtkExpression *[])
    {
        gtk_property_expression_new (AMMONITE_TYPE_WINDOW_SLOT,
                                     gtk_expression_ref (slot_expr),
                                     "selection"),
        gtk_property_expression_new (AMMONITE_TYPE_WINDOW_SLOT, slot_expr, "location"),
        gtk_property_expression_new (AMMONITE_TYPE_FILENAME_VALIDATOR, validator_expr, "passed"),
    };

    closure_expr = gtk_cclosure_expression_new (G_TYPE_BOOLEAN, NULL,
                                                3, expressions,
                                                G_CALLBACK (ammonite_file_chooser_can_accept),
                                                NULL, NULL);
    gtk_expression_bind (closure_expr, action, "enabled", self);
}

static void
ammonite_file_chooser_class_init (AmmoniteFileChooserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = ammonite_file_chooser_constructed;
    object_class->dispose = ammonite_file_chooser_dispose;
    object_class->finalize = ammonite_file_chooser_finalize;
    object_class->get_property = ammonite_file_chooser_get_property;
    object_class->set_property = ammonite_file_chooser_set_property;

    widget_class->grab_focus = ammonite_file_chooser_grab_focus;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/ammonite/ammonite/ui/ammonite-file-chooser.ui");
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, split_view);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, places_sidebar);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, toolbar);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, slot_container);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, filters_dropdown);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, choices_menu_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, accept_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, filename_widget);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, filename_button_container);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, filename_undo_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, filename_entry);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, new_folder_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, validator);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, title_widget);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileChooser, breakpoint);

    gtk_widget_class_bind_template_callback (widget_class, open_filename_entry);
    gtk_widget_class_bind_template_callback (widget_class, on_filename_undo_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class, on_filename_entry_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_filename_entry_focus_leave);
    gtk_widget_class_bind_template_callback (widget_class, ammonite_filename_validator_validate);
    gtk_widget_class_bind_template_callback (widget_class, on_validator_has_feedback_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_validator_will_overwrite_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_file_drop);
    gtk_widget_class_bind_template_callback (widget_class, get_filter_width_chars);

    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_e, GDK_CONTROL_MASK,
                                         "chooser.focus-entry", NULL);
    /* This shortcut was provided via a mnemonic by the GTK file chooser */
    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_n, GDK_ALT_MASK,
                                         "chooser.focus-entry", NULL);

    properties[PROP_MODE] =
        g_param_spec_enum ("mode", NULL, NULL,
                           AMMONITE_TYPE_MODE, AMMONITE_MODE_OPEN_FILE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);

    signals[SIGNAL_ACCEPTED] =
        g_signal_new ("accepted",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 2,
                      G_TYPE_POINTER, GTK_TYPE_FILE_FILTER);
}

AmmoniteFileChooser *
ammonite_file_chooser_new (AmmoniteMode mode)
{
    g_assert (mode != AMMONITE_MODE_BROWSE);

    return g_object_new (AMMONITE_TYPE_FILE_CHOOSER,
                         "mode", mode,
                         NULL);
}

void
ammonite_file_chooser_set_accept_label (AmmoniteFileChooser *self,
                                        const char          *accept_label)
{
    g_set_str (&self->accept_label, accept_label);

    gtk_button_set_label (GTK_BUTTON (self->accept_button), accept_label);
}

void
ammonite_file_chooser_set_current_filter (AmmoniteFileChooser *self,
                                          guint                position)
{
    gtk_drop_down_set_selected (self->filters_dropdown, position);
}

void
ammonite_file_chooser_set_filters (AmmoniteFileChooser *self,
                                   GListModel          *filters)
{
    gboolean has_filters = (filters != NULL && g_list_model_get_n_items (filters) != 0);

    gtk_drop_down_set_model (self->filters_dropdown, filters);
    gtk_widget_set_visible (GTK_WIDGET (self->filters_dropdown), has_filters);
}

void
ammonite_file_chooser_set_starting_location (AmmoniteFileChooser *self,
                                             GFile               *starting_location)
{
    g_autoptr (GFile) location_to_open = NULL;

    if (starting_location != NULL)
    {
        location_to_open = g_object_ref (starting_location);
    }
    else
    {
        location_to_open = g_file_new_for_path (g_get_home_dir ());
    }

    ammonite_window_slot_open_location_full (self->slot, location_to_open, NULL);
}

void
ammonite_file_chooser_set_suggested_name (AmmoniteFileChooser *self,
                                          const char          *suggested_name)
{
    g_set_str (&self->suggested_name, suggested_name);

    if (suggested_name != NULL)
    {
        gtk_editable_set_text (GTK_EDITABLE (self->filename_entry), suggested_name);
    }
}

void
ammonite_file_chooser_add_choices (AmmoniteFileChooser *self,
                                   GActionGroup        *action_group,
                                   GMenuModel          *menu)
{
    gboolean visible = g_menu_model_get_n_items (menu) > 0;

    gtk_widget_insert_action_group (GTK_WIDGET (self), "choices", action_group);
    gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->choices_menu_button), menu);

    gtk_widget_set_visible (self->choices_menu_button, visible);
}
