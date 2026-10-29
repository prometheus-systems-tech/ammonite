/*
 * SPDX-FileCopyrightText: 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "ammonite-new-folder-dialog.h"

#include "ammonite-filename-validator.h"

struct _AmmoniteNewFolderDialog
{
    AdwDialog parent_instance;

    AmmoniteFilenameValidator *validator;

    GtkWidget *name_entry;

    gboolean with_selection;
    NewFolderCallback callback;
    gpointer callback_data;
};

G_DEFINE_FINAL_TYPE (AmmoniteNewFolderDialog, ammonite_new_folder_dialog, ADW_TYPE_DIALOG)

static void
on_feedback_changed (AmmoniteNewFolderDialog *self)
{
    if (ammonite_filename_validator_get_has_feedback (self->validator))
    {
        gtk_widget_add_css_class (self->name_entry, "warning");
    }
    else
    {
        gtk_widget_remove_css_class (self->name_entry, "warning");
    }
}

static void
on_name_accepted (AmmoniteNewFolderDialog *self)
{
    g_autofree char *name = ammonite_filename_validator_get_new_name (self->validator);

    self->callback (name, self->with_selection, self->callback_data);

    adw_dialog_close (ADW_DIALOG (self));
}

AmmoniteNewFolderDialog *
ammonite_new_folder_dialog_new (GtkWidget         *parent,
                                AmmoniteDirectory *destination_directory,
                                gboolean           with_selection,
                                gchar             *initial_name,
                                NewFolderCallback  callback,
                                gpointer           callback_data)
{
    AmmoniteNewFolderDialog *self = g_object_new (AMMONITE_TYPE_NEW_FOLDER_DIALOG,
                                                  NULL);

    ammonite_filename_validator_set_containing_directory (self->validator,
                                                          destination_directory);

    self->with_selection = with_selection;

    self->callback = callback;
    self->callback_data = callback_data;

    if (initial_name != NULL)
    {
        gtk_editable_set_text (GTK_EDITABLE (self->name_entry), initial_name);
    }

    adw_dialog_present (ADW_DIALOG (self), parent);

    return self;
}

static void
ammonite_new_folder_dialog_init (AmmoniteNewFolderDialog *self)
{
    g_type_ensure (AMMONITE_TYPE_FILENAME_VALIDATOR);
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ammonite_new_folder_dialog_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), AMMONITE_TYPE_NEW_FOLDER_DIALOG);

    G_OBJECT_CLASS (ammonite_new_folder_dialog_parent_class)->dispose (object);
}

static void
ammonite_new_folder_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (ammonite_new_folder_dialog_parent_class)->finalize (object);
}

static void
ammonite_new_folder_dialog_class_init (AmmoniteNewFolderDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ammonite_new_folder_dialog_dispose;
    object_class->finalize = ammonite_new_folder_dialog_finalize;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/ammonite/ammonite/ui/ammonite-new-folder-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, AmmoniteNewFolderDialog, name_entry);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteNewFolderDialog, validator);

    gtk_widget_class_bind_template_callback (widget_class, on_feedback_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_name_accepted);
    gtk_widget_class_bind_template_callback (widget_class, ammonite_filename_validator_try_accept);
    gtk_widget_class_bind_template_callback (widget_class, ammonite_filename_validator_validate);
}
