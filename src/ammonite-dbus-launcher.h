/*
 * SPDX-FileCopyrightText: 2022 Corey Berla <corey@berla.me>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#define AMMONITE_TYPE_DBUS_LAUNCHER (ammonite_dbus_launcher_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteDBusLauncher, ammonite_dbus_launcher, AMMONITE, DBUS_LAUNCHER, GObject)


typedef enum {
    AMMONITE_DBUS_APP_0,
    AMMONITE_DBUS_LAUNCHER_SETTINGS,
    AMMONITE_DBUS_LAUNCHER_DISKS,
    AMMONITE_DBUS_LAUNCHER_CONSOLE,
    AMMONITE_DBUS_LAUNCHER_PTYXIS,
    AMMONITE_DBUS_LAUNCHER_N_APPS
} AmmoniteDBusLauncherApp;

AmmoniteDBusLauncher * ammonite_dbus_launcher_new (void); //to be called on `AmmoniteApplication::startup` only

AmmoniteDBusLauncher * ammonite_dbus_launcher_get (void); //to be called by consumers; doesn't change reference count.

gboolean ammonite_dbus_launcher_is_available (AmmoniteDBusLauncher    *self,
                                              AmmoniteDBusLauncherApp  app);

void ammonite_dbus_launcher_call (AmmoniteDBusLauncher    *self,
                                  AmmoniteDBusLauncherApp  app,
                                  const gchar             *method_name,
                                  GVariant                *parameters,
                                  GtkWindow               *window);
