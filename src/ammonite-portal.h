/*
 * SPDX-FileCopyrightText: 2024 GNOME Foundation Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

void
ammonite_portal_setup_gxdp (void);

#define AMMONITE_TYPE_PORTAL (ammonite_portal_get_type())

G_DECLARE_FINAL_TYPE (AmmonitePortal, ammonite_portal, AMMONITE, PORTAL, GObject)

AmmonitePortal * ammonite_portal_new (void);

void ammonite_portal_unregister (AmmonitePortal *self);
gboolean ammonite_portal_register (AmmonitePortal   *self,
                                   GDBusConnection  *connection,
                                   GError          **error);

G_END_DECLS
