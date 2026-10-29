/*
 * SPDX-FileCopyrightText: 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "ammonite-rename-file-popover.h"

#include "ammonite-directory.h"
#include "ammonite-filename-validator.h"
#include "ammonite-file-private.h"
#include "ammonite-filename-utilities.h"


#define RENAME_ENTRY_MIN_CHARS 30
#define RENAME_ENTRY_MAX_CHARS 50

struct _AmmoniteRenameFilePopover
{
    GtkPopover parent_instance;

    AmmoniteFilenameValidator *validator;

    AmmoniteFile *target_file;
    gboolean target_is_folder;

    AmmoniteRenameCallback callback;
    gpointer callback_data;

    GtkWidget *name_entry;
    GtkWidget *title_label;

    gulong file_changed_handler_id;
};

G_DEFINE_FINAL_TYPE (AmmoniteRenameFilePopover,
                     ammonite_rename_file_popover,
                     GTK_TYPE_POPOVER);

static void
disconnect_signal_handlers (AmmoniteRenameFilePopover *self)
{
    g_assert (AMMONITE_IS_RENAME_FILE_POPOVER (self));

    g_clear_signal_handler (&self->file_changed_handler_id, self->target_file);
}

static void
reset_state (AmmoniteRenameFilePopover *self)
{
    g_assert (AMMONITE_IS_RENAME_FILE_POPOVER (self));

    disconnect_signal_handlers (self);

    g_clear_object (&self->target_file);

    gtk_popover_popdown (GTK_POPOVER (self));
}

static void
on_closed (GtkPopover *popover,
           gpointer    user_data)
{
    AmmoniteRenameFilePopover *self = AMMONITE_RENAME_FILE_POPOVER (user_data);

    reset_state (self);
}

static gboolean
name_entry_on_f2_pressed (GtkWidget                 *widget,
                          AmmoniteRenameFilePopover *self)
{
    guint text_length;
    gint start_pos;
    gint end_pos;
    gboolean all_selected;

    text_length = (guint) gtk_entry_get_text_length (GTK_ENTRY (widget));
    if (text_length == 0)
    {
        return GDK_EVENT_STOP;
    }

    gtk_editable_get_selection_bounds (GTK_EDITABLE (widget),
                                       &start_pos, &end_pos);

    all_selected = (start_pos == 0) && ((guint) end_pos == text_length);
    if (!all_selected || !ammonite_file_is_regular_file (self->target_file))
    {
        gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
    }
    else
    {
        /* Select the name part without the file extension */
        const char *text = gtk_editable_get_text (GTK_EDITABLE (widget));
        gtk_editable_select_region (GTK_EDITABLE (widget), 0,
                                    ammonite_filename_get_extension_char_offset (text));
    }

    return GDK_EVENT_STOP;
}

static gboolean
name_entry_on_undo (GtkWidget                 *widget,
                    AmmoniteRenameFilePopover *self)
{
    const char *edit_name = ammonite_file_get_edit_name (self->target_file);

    gtk_editable_set_text (GTK_EDITABLE (widget), edit_name);

    gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);

    return GDK_EVENT_STOP;
}

static gboolean
on_event_controller_key_key_pressed (GtkEventControllerKey *controller,
                                     guint                  keyval,
                                     guint                  keycode,
                                     GdkModifierType        state,
                                     gpointer               user_data)
{
    GtkWidget *widget;
    AmmoniteRenameFilePopover *self = AMMONITE_RENAME_FILE_POPOVER (user_data);

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));

    if (keyval == GDK_KEY_F2)
    {
        return name_entry_on_f2_pressed (widget, self);
    }
    else if (keyval == GDK_KEY_z && (state & GDK_CONTROL_MASK) != 0)
    {
        return name_entry_on_undo (widget, self);
    }

    return GDK_EVENT_PROPAGATE;
}

static void
target_file_on_changed (AmmoniteFile *file,
                        gpointer      user_data)
{
    AmmoniteRenameFilePopover *self = AMMONITE_RENAME_FILE_POPOVER (user_data);

    if (ammonite_file_is_gone (file))
    {
        reset_state (self);
    }
}

static void
on_name_accepted (AmmoniteRenameFilePopover *self)
{
    g_autofree char *name = ammonite_filename_validator_get_new_name (self->validator);

    self->callback (self->target_file, name, self->callback_data);

    reset_state (self);
}

GtkWidget *
ammonite_rename_file_popover_new (void)
{
    AmmoniteRenameFilePopover *self = g_object_new (AMMONITE_TYPE_RENAME_FILE_POPOVER, NULL);

    return GTK_WIDGET (self);
}

void
ammonite_rename_file_popover_show_for_file (AmmoniteRenameFilePopover *self,
                                            AmmoniteFile              *target_file,
                                            GdkRectangle              *pointing_to,
                                            AmmoniteRenameCallback     callback,
                                            gpointer                   callback_data)
{
    g_autoptr (AmmoniteDirectory) containing_directory = NULL;
    GtkEventController *controller;
    const char *edit_name;
    gint n_chars;

    g_assert (AMMONITE_IS_RENAME_FILE_POPOVER (self));
    g_assert (AMMONITE_IS_FILE (target_file));

    reset_state (self);

    self->target_file = g_object_ref (target_file);
    self->callback = callback;
    self->callback_data = callback_data;

    if (!ammonite_file_is_self_owned (self->target_file))
    {
        containing_directory = ammonite_directory_ref (ammonite_file_get_directory (self->target_file));
    }
    else
    {
        containing_directory = ammonite_directory_get_for_file (self->target_file);
    }

    ammonite_filename_validator_set_containing_directory (self->validator,
                                                          containing_directory);

    self->target_is_folder = ammonite_file_is_directory (self->target_file);

    ammonite_filename_validator_set_target_is_folder (self->validator,
                                                      self->target_is_folder);
    ammonite_filename_validator_set_original_name (self->validator,
                                                   ammonite_file_get_display_name (self->target_file));

    self->file_changed_handler_id = g_signal_connect (self->target_file,
                                                      "changed",
                                                      G_CALLBACK (target_file_on_changed),
                                                      self);

    controller = gtk_event_controller_key_new ();
    gtk_widget_add_controller (self->name_entry, controller);
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (on_event_controller_key_key_pressed), self);

    gtk_label_set_text (GTK_LABEL (self->title_label),
                        self->target_is_folder ? _("Rename Folder") :
                                                 _("Rename File"));

    edit_name = ammonite_file_get_edit_name (self->target_file);

    g_signal_handlers_block_by_func (self->name_entry,
                                     ammonite_filename_validator_validate,
                                     self->validator);

    gtk_editable_set_text (GTK_EDITABLE (self->name_entry), edit_name);

    g_signal_handlers_unblock_by_func (self->name_entry,
                                       ammonite_filename_validator_validate,
                                       self->validator);

    gtk_popover_set_pointing_to (GTK_POPOVER (self), pointing_to);
    gtk_popover_popup (GTK_POPOVER (self));
    if (!gtk_widget_get_mapped (GTK_WIDGET (self)))
    {
        /* The compositor rejected the popover and it was closed */
        return;
    }

    if (ammonite_file_is_regular_file (self->target_file))
    {
        /* Select the name part without the file extension */
        gtk_editable_select_region (GTK_EDITABLE (self->name_entry), 0,
                                    ammonite_filename_get_extension_char_offset (edit_name));
    }

    n_chars = g_utf8_strlen (edit_name, -1);
    gtk_editable_set_width_chars (GTK_EDITABLE (self->name_entry),
                                  MIN (MAX (n_chars, RENAME_ENTRY_MIN_CHARS),
                                       RENAME_ENTRY_MAX_CHARS));
}

static void
ammonite_rename_file_popover_init (AmmoniteRenameFilePopover *self)
{
    g_type_ensure (AMMONITE_TYPE_FILENAME_VALIDATOR);
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ammonite_rename_file_popover_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), AMMONITE_TYPE_RENAME_FILE_POPOVER);

    G_OBJECT_CLASS (ammonite_rename_file_popover_parent_class)->dispose (object);
}

static void
ammonite_rename_file_popover_finalize (GObject *object)
{
    AmmoniteRenameFilePopover *self = AMMONITE_RENAME_FILE_POPOVER (object);

    reset_state (self);

    G_OBJECT_CLASS (ammonite_rename_file_popover_parent_class)->finalize (object);
}

static void
ammonite_rename_file_popover_class_init (AmmoniteRenameFilePopoverClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = ammonite_rename_file_popover_finalize;
    object_class->dispose = ammonite_rename_file_popover_dispose;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/ammonite/ammonite/ui/ammonite-rename-file-popover.ui");

    gtk_widget_class_bind_template_child (widget_class, AmmoniteRenameFilePopover, name_entry);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteRenameFilePopover, title_label);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteRenameFilePopover, validator);

    gtk_widget_class_bind_template_callback (widget_class, on_closed);
    gtk_widget_class_bind_template_callback (widget_class, on_name_accepted);
    gtk_widget_class_bind_template_callback (widget_class, ammonite_filename_validator_try_accept);
    gtk_widget_class_bind_template_callback (widget_class, ammonite_filename_validator_validate);
}
