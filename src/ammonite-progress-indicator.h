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

#define AMMONITE_TYPE_PROGRESS_INDICATOR (ammonite_progress_indicator_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteProgressIndicator, ammonite_progress_indicator, AMMONITE, PROGRESS_INDICATOR, AdwBin)

G_END_DECLS
