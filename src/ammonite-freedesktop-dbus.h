/*
 * ammonite-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
 *
 * SPDX-FileCopyrightText: 2010 Cosimo Cecchi <cosimoc@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#define AMMONITE_FDO_DBUS_IFACE "org.freedesktop.FileManager1"
#define AMMONITE_FDO_DBUS_NAME  "org.freedesktop.FileManager1"
#define AMMONITE_FDO_DBUS_PATH  "/org/freedesktop/FileManager1"

#define AMMONITE_TYPE_FREEDESKTOP_DBUS ammonite_freedesktop_dbus_get_type()

G_DECLARE_FINAL_TYPE (AmmoniteFreedesktopDBus, ammonite_freedesktop_dbus, AMMONITE, FREEDESKTOP_DBUS, GObject);

AmmoniteFreedesktopDBus * ammonite_freedesktop_dbus_new (void);

gboolean ammonite_freedesktop_dbus_register (AmmoniteFreedesktopDBus *fdb, GDBusConnection *connection, GError **error);
void ammonite_freedesktop_dbus_unregister (AmmoniteFreedesktopDBus *fdb);

void ammonite_freedesktop_dbus_set_open_locations (AmmoniteFreedesktopDBus *fdb, const gchar **locations);

void ammonite_freedesktop_dbus_set_open_windows_with_locations (AmmoniteFreedesktopDBus *fdb, GVariant *locations);
