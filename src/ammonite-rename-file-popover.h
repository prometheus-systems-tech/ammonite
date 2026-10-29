/*
 * SPDX-FileCopyrightText: 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>
#include <gtk/gtk.h>

typedef void (*AmmoniteRenameCallback) (AmmoniteFile *target_file,
                                        const char   *new_name,
                                        gpointer      user_data);

#define AMMONITE_TYPE_RENAME_FILE_POPOVER ammonite_rename_file_popover_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteRenameFilePopover, ammonite_rename_file_popover, AMMONITE, RENAME_FILE_POPOVER, GtkPopover)

GtkWidget * ammonite_rename_file_popover_new (void);

void        ammonite_rename_file_popover_show_for_file (AmmoniteRenameFilePopover *self,
                                                        AmmoniteFile              *target_file,
                                                        GdkRectangle              *pointing_to,
                                                        AmmoniteRenameCallback     callback,
                                                        gpointer                   callback_data);
