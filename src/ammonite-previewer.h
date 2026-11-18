/*
 * ammonite-previewer: ammonite previewer DBus wrapper
 *
 * SPDX-FileCopyrightText: 2011, Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>

#include "ammonite-types.h"

G_BEGIN_DECLS

void ammonite_previewer_call_show_file (const gchar        *uri,
                                        AmmoniteWindowSlot *window,
                                        gboolean            close_if_already_visible);

gboolean ammonite_previewer_is_visible (void);

void  ammonite_previewer_setup         (void);
void  ammonite_previewer_teardown      (GDBusConnection *connection);

G_END_DECLS
