/*
 * SPDX-FileCopyrightText: 2023 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <adwaita.h>

typedef void (*CompressCallback) (const char *new_name,
                                  const char *passphrase,
                                  gpointer    user_data);

#define AMMONITE_TYPE_COMPRESS_DIALOG ammonite_compress_dialog_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteCompressDialog, ammonite_compress_dialog, AMMONITE, COMPRESS_DIALOG, AdwDialog)

AmmoniteCompressDialog * ammonite_compress_dialog_new (GtkWindow         *parent_window,
                                                       AmmoniteDirectory *destination_directory,
                                                       const char        *initial_name,
                                                       CompressCallback   callback,
                                                       gpointer           callback_data);
