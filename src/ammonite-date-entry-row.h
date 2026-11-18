/*
 * SPDX-FileCopyrightText: 2025 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#pragma once

#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

#define AMMONITE_TYPE_DATE_ENTRY_ROW ammonite_date_entry_row_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteDateEntryRow, ammonite_date_entry_row, AMMONITE, DATE_ENTRY_ROW, AdwEntryRow)

AmmoniteDateEntryRow *
ammonite_date_entry_row_new (GDateTime *initial);

GDateTime *
ammonite_date_entry_row_get_date_time (AmmoniteDateEntryRow *self);

void
ammonite_date_entry_row_set_date_time (AmmoniteDateEntryRow *self,
                                       GDateTime            *date_time);
