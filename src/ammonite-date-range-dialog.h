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

#define AMMONITE_TYPE_DATE_RANGE_DIALOG ammonite_date_range_dialog_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteDateRangeDialog, ammonite_date_range_dialog, AMMONITE, DATE_RANGE_DIALOG, AdwDialog)

AmmoniteDateRangeDialog *
ammonite_date_range_dialog_new (GPtrArray *initial_date_range);
