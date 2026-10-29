/*
 * ammonite-file-conflict-dialog: dialog that handles file conflicts
 * during transfer operations.
 *
 * SPDX-FileCopyrightText: 2008-2010 Cosimo Cecchi
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#include <config.h>
#include "ammonite-file-conflict-dialog.h"

#include <string.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <pango/pango.h>

#include "ammonite-file.h"
#include <ammonite-image.h>
#include "ammonite-filename-utilities.h"
#include "ammonite-operations-ui-manager.h"

struct _AmmoniteFileConflictDialog
{
    AdwWindow parent_instance;

    gchar *conflict_name;
    gchar *suggested_name;

    ConflictResponse response;
    gint activation_timeout_id;

    /* UI objects */
    GtkWidget *primary_label;
    GtkWidget *secondary_label;
    GtkWidget *dest_label;
    GtkWidget *src_label;
    GtkWidget *expander;
    GtkWidget *entry;
    GtkWidget *checkbox;
    GtkWidget *cancel_button;
    GtkWidget *skip_button;
    GtkWidget *rename_button;
    GtkWidget *replace_button;
    AmmoniteImage *dest_icon;
    AmmoniteImage *src_icon;
};

G_DEFINE_FINAL_TYPE (AmmoniteFileConflictDialog, ammonite_file_conflict_dialog, ADW_TYPE_WINDOW);

void
ammonite_file_conflict_dialog_set_text (AmmoniteFileConflictDialog *fcd,
                                        gchar                      *primary_text,
                                        gchar                      *secondary_text)
{
    gtk_label_set_text (GTK_LABEL (fcd->primary_label), primary_text);
    gtk_label_set_text (GTK_LABEL (fcd->secondary_label), secondary_text);
}

void
ammonite_file_conflict_dialog_set_images (AmmoniteFileConflictDialog *fcd,
                                          AmmoniteFile               *destination_file,
                                          AmmoniteFile               *source_file)
{
    int scale = gtk_widget_get_scale_factor (GTK_WIDGET (fcd));
    g_autoptr (GFile) dest_location = ammonite_file_get_location (destination_file);
    g_autoptr (GFile) src_location = ammonite_file_get_location (source_file);
    g_autoptr (GdkPaintable) destination_paintable
        = ammonite_file_get_icon_paintable (destination_file,
                                            AMMONITE_GRID_ICON_SIZE_SMALL,
                                            scale,
                                            AMMONITE_FILE_ICON_FLAGS_NONE);
    g_autoptr (GdkPaintable) source_paintable
        = ammonite_file_get_icon_paintable (source_file,
                                            AMMONITE_GRID_ICON_SIZE_SMALL,
                                            scale,
                                            AMMONITE_FILE_ICON_FLAGS_NONE);
    gboolean show_dest_thumbnail = ammonite_file_should_show_thumbnail (destination_file);
    gboolean show_src_thumbnail = ammonite_file_should_show_thumbnail (source_file);

    ammonite_image_set_fallback (fcd->dest_icon, destination_paintable);
    ammonite_image_set_source (fcd->dest_icon, NULL);
    ammonite_image_set_source (fcd->dest_icon, show_dest_thumbnail ? dest_location : NULL);

    ammonite_image_set_fallback (fcd->src_icon, source_paintable);
    ammonite_image_set_source (fcd->src_icon, NULL);
    ammonite_image_set_source (fcd->src_icon, show_src_thumbnail ? src_location : NULL);
}

void
ammonite_file_conflict_dialog_set_file_labels (AmmoniteFileConflictDialog *fcd,
                                               gchar                      *destination_label,
                                               gchar                      *source_label)
{
    gtk_label_set_markup (GTK_LABEL (fcd->dest_label), destination_label);
    gtk_label_set_markup (GTK_LABEL (fcd->src_label), source_label);
}

void
ammonite_file_conflict_dialog_set_conflict_name (AmmoniteFileConflictDialog *fcd,
                                                 const char                 *conflict_name)
{
    fcd->conflict_name = g_strdup (conflict_name);
}

void
ammonite_file_conflict_dialog_set_suggested_name (AmmoniteFileConflictDialog *fcd,
                                                  gchar                      *suggested_name)
{
    fcd->suggested_name = g_strdup (suggested_name);
    gtk_editable_set_text (GTK_EDITABLE (fcd->entry), suggested_name);
}

void
ammonite_file_conflict_dialog_set_replace_button_label (AmmoniteFileConflictDialog *fcd,
                                                        gchar                      *label)
{
    gtk_button_set_label (GTK_BUTTON (fcd->replace_button), label);
}

void
ammonite_file_conflict_dialog_disable_replace (AmmoniteFileConflictDialog *fcd)
{
    gtk_widget_set_sensitive (fcd->replace_button, FALSE);
}

void
ammonite_file_conflict_dialog_disable_apply_to_all (AmmoniteFileConflictDialog *fcd)
{
    gtk_widget_set_visible (fcd->checkbox, FALSE);
}

static void
cancel_button_cb (AmmoniteFileConflictDialog *dialog)
{
    dialog->response = CONFLICT_RESPONSE_CANCEL;
    gtk_window_close (GTK_WINDOW (dialog));
}

static void
rename_button_cb (AmmoniteFileConflictDialog *dialog)
{
    dialog->response = CONFLICT_RESPONSE_RENAME;
    gtk_window_close (GTK_WINDOW (dialog));
}

static void
replace_button_cb (AmmoniteFileConflictDialog *dialog)
{
    dialog->response = CONFLICT_RESPONSE_REPLACE;
    gtk_window_close (GTK_WINDOW (dialog));
}

static void
skip_button_cb (AmmoniteFileConflictDialog *dialog)
{
    dialog->response = CONFLICT_RESPONSE_SKIP;
    gtk_window_close (GTK_WINDOW (dialog));
}

static void
entry_text_changed_cb (GtkEditable                *entry,
                       AmmoniteFileConflictDialog *dialog)
{
    if (g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (entry)), "") != 0 &&
        g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (entry)), dialog->conflict_name) != 0)
    {
        gtk_widget_set_sensitive (dialog->rename_button, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive (dialog->rename_button, FALSE);
    }
}

static int
get_character_position_after_basename (const gchar *filename)
{
    const gchar *extension = ammonite_filename_get_extension (filename);
    return g_utf8_pointer_to_offset (filename, extension);
}

static void
on_expanded_notify (GtkExpander                *w,
                    GParamSpec                 *pspec,
                    AmmoniteFileConflictDialog *dialog)
{
    if (gtk_expander_get_expanded (w))
    {
        gtk_widget_set_visible (dialog->replace_button, FALSE);
        gtk_widget_set_visible (dialog->rename_button, TRUE);
        gtk_window_set_default_widget (GTK_WINDOW (dialog), dialog->rename_button);

        gtk_widget_set_sensitive (dialog->checkbox, FALSE);

        gtk_widget_grab_focus (dialog->entry);
        if (g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (dialog->entry)), dialog->suggested_name) == 0)
        {
            /* The suggested name is in the form "original (1).txt", if the
             * the conflicting name was "original.txt". The user may want to
             * replace the "(1)" bits with with something more meaningful, so
             * select this region for convenience. */

            int start_pos;
            int end_pos;

            start_pos = get_character_position_after_basename (dialog->conflict_name);
            end_pos = get_character_position_after_basename (dialog->suggested_name);

            gtk_editable_select_region (GTK_EDITABLE (dialog->entry), start_pos, end_pos);
        }
    }
    else
    {
        gtk_widget_set_visible (dialog->rename_button, FALSE);
        gtk_widget_set_visible (dialog->replace_button, TRUE);
        gtk_window_set_default_widget (GTK_WINDOW (dialog), dialog->replace_button);

        gtk_widget_set_sensitive (dialog->checkbox, TRUE);
    }
}

static void
checkbox_toggled_cb (GtkCheckButton             *t,
                     AmmoniteFileConflictDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->expander, !gtk_check_button_get_active (t));
}

static void
reset_button_clicked_cb (GtkButton                  *w,
                         AmmoniteFileConflictDialog *dialog)
{
    gtk_editable_set_text (GTK_EDITABLE (dialog->entry), dialog->conflict_name);
    gtk_widget_grab_focus (dialog->entry);
    gtk_editable_select_region (GTK_EDITABLE (dialog->entry), 0,
                                ammonite_filename_get_extension_char_offset (dialog->conflict_name));
}

static void
ammonite_file_conflict_dialog_init (AmmoniteFileConflictDialog *fcd)
{
    gtk_widget_init_template (GTK_WIDGET (fcd));
    /* Treat closing window as cancel action */
    fcd->response = CONFLICT_RESPONSE_CANCEL;
}

static void
ammonite_file_conflict_dialog_dispose (GObject *object)
{
    AmmoniteFileConflictDialog *self = AMMONITE_FILE_CONFLICT_DIALOG (object);

    gtk_widget_dispose_template (GTK_WIDGET (self), AMMONITE_TYPE_FILE_CONFLICT_DIALOG);

    G_OBJECT_CLASS (ammonite_file_conflict_dialog_parent_class)->dispose (object);
}

static void
do_finalize (GObject *self)
{
    AmmoniteFileConflictDialog *dialog = AMMONITE_FILE_CONFLICT_DIALOG (self);

    g_free (dialog->conflict_name);
    g_free (dialog->suggested_name);
    g_clear_handle_id (&dialog->activation_timeout_id, g_source_remove);

    G_OBJECT_CLASS (ammonite_file_conflict_dialog_parent_class)->finalize (self);
}

static void
ammonite_file_conflict_dialog_class_init (AmmoniteFileConflictDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/ammonite/ammonite/ui/ammonite-file-conflict-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, primary_label);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, secondary_label);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, dest_label);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, src_label);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, expander);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, entry);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, checkbox);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, cancel_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, rename_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, replace_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, skip_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, dest_icon);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteFileConflictDialog, src_icon);
    gtk_widget_class_bind_template_callback (widget_class, cancel_button_cb);
    gtk_widget_class_bind_template_callback (widget_class, rename_button_cb);
    gtk_widget_class_bind_template_callback (widget_class, replace_button_cb);
    gtk_widget_class_bind_template_callback (widget_class, skip_button_cb);
    gtk_widget_class_bind_template_callback (widget_class, entry_text_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_expanded_notify);
    gtk_widget_class_bind_template_callback (widget_class, checkbox_toggled_cb);
    gtk_widget_class_bind_template_callback (widget_class, reset_button_clicked_cb);

    G_OBJECT_CLASS (klass)->dispose = ammonite_file_conflict_dialog_dispose;
    G_OBJECT_CLASS (klass)->finalize = do_finalize;
}

static void
activate_buttons (AmmoniteFileConflictDialog *fcd)
{
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->cancel_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->skip_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->rename_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->replace_button), TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->expander), TRUE);
}

void
ammonite_file_conflict_dialog_delay_buttons_activation (AmmoniteFileConflictDialog *fcd)
{
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->cancel_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->skip_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->rename_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->replace_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (fcd->expander), FALSE);

    fcd->activation_timeout_id = g_timeout_add_seconds_once (BUTTON_ACTIVATION_DELAY_IN_SECONDS,
                                                             (GSourceOnceFunc) activate_buttons,
                                                             fcd);
}

char *
ammonite_file_conflict_dialog_get_new_name (AmmoniteFileConflictDialog *dialog)
{
    return g_strdup (gtk_editable_get_text (GTK_EDITABLE (dialog->entry)));
}

ConflictResponse
ammonite_file_conflict_dialog_get_response (AmmoniteFileConflictDialog *dialog)
{
    return dialog->response;
}

gboolean
ammonite_file_conflict_dialog_get_apply_to_all (AmmoniteFileConflictDialog *dialog)
{
    return gtk_check_button_get_active (GTK_CHECK_BUTTON (dialog->checkbox));
}

AmmoniteFileConflictDialog *
ammonite_file_conflict_dialog_new (GtkWindow *parent)
{
    return AMMONITE_FILE_CONFLICT_DIALOG (g_object_new (AMMONITE_TYPE_FILE_CONFLICT_DIALOG,
                                                        "transient-for", parent,
                                                        NULL));
}
