/*
 * SPDX-FileCopyrightText: 2024 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_RECENT_SERVERS (ammonite_recent_servers_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteRecentServers, ammonite_recent_servers, AMMONITE, RECENT_SERVERS, GObject);

AmmoniteRecentServers* ammonite_recent_servers_new (void);

void                    ammonite_recent_servers_force_reload   (AmmoniteRecentServers *self);
gboolean                ammonite_recent_servers_get_loading    (AmmoniteRecentServers *self);

void                    ammonite_add_recent_server             (GFile                 *file);

void                    ammonite_remove_recent_server          (const char            *uri);

G_END_DECLS
