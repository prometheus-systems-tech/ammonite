/*
 * SPDX-FileCopyrightText: 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

#include "ammonite-types.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_SCRIPTS_MONITOR (ammonite_scripts_monitor_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteScriptsMonitor, ammonite_scripts_monitor, AMMONITE, SCRIPTS_MONITOR, GObject)

AmmoniteScriptsMonitor *ammonite_scripts_monitor_get (void);

typedef void (*AddScriptClosure) (AmmoniteFile *, GMenu *, GHashTable *, gpointer);
GMenu *ammonite_scripts_monitor_get_menu (AmmoniteScriptsMonitor *self,
                                          AddScriptClosure        closure,
                                          gpointer                data);

G_END_DECLS
