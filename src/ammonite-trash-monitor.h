
/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Pavel Cisler <pavel@eazel.com>
 */

#pragma once

#include <gio/gio.h>

#define AMMONITE_TYPE_TRASH_MONITOR (ammonite_trash_monitor_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteTrashMonitor, ammonite_trash_monitor,
                      AMMONITE, TRASH_MONITOR,
                      GObject)

AmmoniteTrashMonitor   *ammonite_trash_monitor_get      (void);
gboolean                ammonite_trash_monitor_is_empty (void);
GIcon                  *ammonite_trash_monitor_get_symbolic_icon (void);
void                    ammonite_trash_monitor_clear (void);
