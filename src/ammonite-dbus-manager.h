/*
 * ammonite-dbus-manager: ammonite DBus interface
 *
 * SPDX-FileCopyrightText: 2010, Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#define AMMONITE_TYPE_DBUS_MANAGER (ammonite_dbus_manager_get_type())
G_DECLARE_FINAL_TYPE (AmmoniteDBusManager, ammonite_dbus_manager, AMMONITE, DBUS_MANAGER, GObject)

AmmoniteDBusManager * ammonite_dbus_manager_new (void);

gboolean ammonite_dbus_manager_register   (AmmoniteDBusManager *self,
                                           GDBusConnection     *connection,
                                           GError             **error);
void     ammonite_dbus_manager_unregister (AmmoniteDBusManager *self);
