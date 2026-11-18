
/*
 * ammonite-file-conflict-dialog: dialog that handles file conflicts
 * during transfer operations.
 *
 * SPDX-FileCopyrightText: 2008, Cosimo Cecchi
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#pragma once

#include <adwaita.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <ammonite-file.h>

G_BEGIN_DECLS

typedef enum
{
    CONFLICT_RESPONSE_CANCEL,
    CONFLICT_RESPONSE_SKIP,
    CONFLICT_RESPONSE_REPLACE,
    CONFLICT_RESPONSE_RENAME,
} ConflictResponse;

#define AMMONITE_TYPE_FILE_CONFLICT_DIALOG (ammonite_file_conflict_dialog_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteFileConflictDialog, ammonite_file_conflict_dialog, AMMONITE, FILE_CONFLICT_DIALOG, AdwWindow)

AmmoniteFileConflictDialog* ammonite_file_conflict_dialog_new (GtkWindow *parent);

void ammonite_file_conflict_dialog_set_text (AmmoniteFileConflictDialog *fcd,
                                             gchar *primary_text,
                                             gchar *secondary_text);
void ammonite_file_conflict_dialog_set_images (AmmoniteFileConflictDialog *fcd,
                                               AmmoniteFile               *destination_file,
                                               AmmoniteFile               *source_file);
void ammonite_file_conflict_dialog_set_file_labels (AmmoniteFileConflictDialog *fcd,
                                                    gchar *destination_label,
                                                    gchar *source_label);
void ammonite_file_conflict_dialog_set_conflict_name (AmmoniteFileConflictDialog *fcd,
                                                      const char *conflict_name);
void ammonite_file_conflict_dialog_set_suggested_name (AmmoniteFileConflictDialog *fcd,
                                                       gchar *suggested_name);
void ammonite_file_conflict_dialog_set_replace_button_label (AmmoniteFileConflictDialog *fcd,
                                                             gchar *label);

void ammonite_file_conflict_dialog_disable_replace (AmmoniteFileConflictDialog *fcd);
void ammonite_file_conflict_dialog_disable_apply_to_all (AmmoniteFileConflictDialog *fcd);

void ammonite_file_conflict_dialog_delay_buttons_activation (AmmoniteFileConflictDialog *fdc);

char*      ammonite_file_conflict_dialog_get_new_name     (AmmoniteFileConflictDialog *dialog);
gboolean   ammonite_file_conflict_dialog_get_apply_to_all (AmmoniteFileConflictDialog *dialog);

ConflictResponse ammonite_file_conflict_dialog_get_response (AmmoniteFileConflictDialog *dialog);

G_END_DECLS
