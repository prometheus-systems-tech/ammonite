/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_HISTORY_CONTROLS ammonite_history_controls_get_type()

G_DECLARE_FINAL_TYPE (AmmoniteHistoryControls, ammonite_history_controls, AMMONITE, HISTORY_CONTROLS, AdwBin)

G_END_DECLS
