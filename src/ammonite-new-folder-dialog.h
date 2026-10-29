/*
 * SPDX-FileCopyrightText: 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <adwaita.h>

typedef void (*NewFolderCallback) (const char *new_name,
                                   gboolean    from_selection,
                                   gpointer    user_data);

#define AMMONITE_TYPE_NEW_FOLDER_DIALOG ammonite_new_folder_dialog_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteNewFolderDialog, ammonite_new_folder_dialog, AMMONITE, NEW_FOLDER_DIALOG, AdwDialog)

AmmoniteNewFolderDialog * ammonite_new_folder_dialog_new (GtkWidget         *parent,
                                                          AmmoniteDirectory *destination_directory,
                                                          gboolean           with_selection,
                                                          gchar             *initial_name,
                                                          NewFolderCallback  callback,
                                                          gpointer           callback_data);
