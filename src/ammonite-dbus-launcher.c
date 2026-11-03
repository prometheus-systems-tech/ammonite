/*
 * SPDX-FileCopyrightText: 2022 Corey Berla <corey@berla.me>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-dbus-launcher.h"

#include <glib/gi18n.h>

#include "ammonite-ui-utilities.h"


typedef struct
{
    GDBusProxy *proxy;
    GCancellable *cancellable;
    const char *name;
    gboolean available;
} AmmoniteDBusLauncherData;

struct _AmmoniteDBusLauncher
{
    GObject parent;

    GCancellable *cancellable;
    GDBusProxy *proxy;
    AmmoniteDBusLauncherApp last_app_initialized;
    AmmoniteDBusLauncherData *data[AMMONITE_DBUS_LAUNCHER_N_APPS];
};

G_DEFINE_FINAL_TYPE (AmmoniteDBusLauncher, ammonite_dbus_launcher, G_TYPE_OBJECT)

static AmmoniteDBusLauncher *launcher = NULL;

static void
on_ammonite_dbus_launcher_call_finished   (GObject      *source_object,
                                           GAsyncResult *res,
                                           gpointer      user_data)
{
    GtkWindow *window = user_data;
    g_autoptr (GError) error = NULL;
    g_autofree char *message = NULL;

    g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
    if (error != NULL)
    {
        g_warning ("Error calling proxy %s", error->message);
        message = g_strdup_printf (_("Details: %s"), error->message);
        ammonite_show_ok_dialog (_("There was an error launching the app."),
                                 message,
                                 GTK_WIDGET (window));
    }
}

void
ammonite_dbus_launcher_call (AmmoniteDBusLauncher    *self,
                             AmmoniteDBusLauncherApp  app,
                             const gchar             *method_name,
                             GVariant                *parameters,
                             GtkWindow               *window)
{
    g_autoptr (GVariant) sunk_parameters = g_variant_ref_sink (parameters);

    if (self->data[app]->proxy != NULL)
    {
        g_dbus_proxy_call (self->data[app]->proxy,
                           method_name,
                           sunk_parameters,
                           G_DBUS_CALL_FLAGS_NONE,
                           G_MAXINT,
                           self->cancellable,
                           on_ammonite_dbus_launcher_call_finished,
                           window);
    }
    else if (window != NULL)
    {
        ammonite_show_ok_dialog (_("There was an error launching the app."),
                                 _("Details: The proxy has not been created."),
                                 GTK_WIDGET (window));
    }
}

static void
on_ammonite_dbus_proxy_ready (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
    AmmoniteDBusLauncherData *data = user_data;
    g_autoptr (GError) error = NULL;

    data->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

    if (error != NULL)
    {
        g_warning ("Error creating proxy %s", error->message);
    }
}

static void
ammonite_dbus_launcher_create_proxy (AmmoniteDBusLauncherData *data,
                                     const gchar              *name,
                                     const gchar              *object_path,
                                     const gchar              *interface)
{
    data->name = name;

    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                              G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                              NULL,
                              name,
                              object_path,
                              interface,
                              data->cancellable,
                              on_ammonite_dbus_proxy_ready,
                              data);
}

gboolean
ammonite_dbus_launcher_is_available (AmmoniteDBusLauncher    *self,
                                     AmmoniteDBusLauncherApp  app)
{
    return (self->data[app]->available && self->data[app]->proxy != NULL);
}

static void
activatable_names_received (GObject      *object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    AmmoniteDBusLauncher *self = user_data;
    g_auto (GStrv) activatable_names = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (GVariant) names = NULL;

    names = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);
    if (error == NULL)
    {
        g_variant_get (names, "(^as)", &activatable_names);
    }
    else
    {
        g_warning ("Error receiving activatable names: %s", error->message);
    }

    for (guint i = 1; i < AMMONITE_DBUS_LAUNCHER_N_APPS; i++)
    {
        if (activatable_names != NULL)
        {
            self->data[i]->available = g_strv_contains ((const char * const *) activatable_names,
                                                        self->data[i]->name);
        }
        else
        {
            self->data[i]->available = FALSE;
        }
    }
}

static void
get_activatable_names (AmmoniteDBusLauncher *self)
{
    g_dbus_proxy_call (self->proxy, "ListActivatableNames", NULL,
                       G_DBUS_CALL_FLAGS_NONE, G_MAXINT,
                       self->cancellable, activatable_names_received, self);
}

static void
activatable_services_changed (GDBusConnection *connection,
                              const gchar     *sender_name,
                              const gchar     *object_path,
                              const gchar     *interface_name,
                              const gchar     *signal_name,
                              GVariant        *parameters,
                              gpointer         user_data)
{
    AmmoniteDBusLauncher *self = user_data;

    get_activatable_names (self);
}

static void
proxy_ready (GObject      *object,
             GAsyncResult *res,
             gpointer      user_data)
{
    AmmoniteDBusLauncher *self = user_data;
    GDBusConnection *connection;

    self->proxy = g_dbus_proxy_new_for_bus_finish (res, NULL);
    get_activatable_names (self);
    connection = g_dbus_proxy_get_connection (self->proxy);
    g_dbus_connection_signal_subscribe (connection,
                                        "org.freedesktop.DBus",
                                        "org.freedesktop.DBus",
                                        "ActivatableServicesChanged",
                                        "/org/freedesktop/DBus",
                                        NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                        activatable_services_changed, self, NULL);
}

AmmoniteDBusLauncher *
ammonite_dbus_launcher_get (void)
{
    return launcher;
}

AmmoniteDBusLauncher *
ammonite_dbus_launcher_new (void)
{
    if (launcher != NULL)
    {
        return g_object_ref (launcher);
    }
    launcher = g_object_new (AMMONITE_TYPE_DBUS_LAUNCHER, NULL);
    g_object_add_weak_pointer (G_OBJECT (launcher), (gpointer) & launcher);

    return launcher;
}

static void
ammonite_dbus_launcher_finalize (GObject *object)
{
    AmmoniteDBusLauncher *self = AMMONITE_DBUS_LAUNCHER (object);

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_object (&self->proxy);

    for (guint i = 1; i <= self->last_app_initialized; i++)
    {
        g_clear_object (&self->data[i]->proxy);
        g_free (self->data[i]);
    }

    G_OBJECT_CLASS (ammonite_dbus_launcher_parent_class)->finalize (object);
}

static void
ammonite_dbus_launcher_class_init (AmmoniteDBusLauncherClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = ammonite_dbus_launcher_finalize;
}

static void
ammonite_dbus_launcher_data_init (AmmoniteDBusLauncher    *self,
                                  AmmoniteDBusLauncherApp  app)
{
    AmmoniteDBusLauncherData *data;
    g_assert_true (app == self->last_app_initialized + 1);

    data = g_new0 (AmmoniteDBusLauncherData, 1);

    data->cancellable = self->cancellable;
    self->data[app] = data;
    self->last_app_initialized = app;
}

static void
ammonite_dbus_launcher_init (AmmoniteDBusLauncher *self)
{
    self->cancellable = g_cancellable_new ();

    ammonite_dbus_launcher_data_init (self, AMMONITE_DBUS_LAUNCHER_SETTINGS);
    ammonite_dbus_launcher_data_init (self, AMMONITE_DBUS_LAUNCHER_DISKS);
    ammonite_dbus_launcher_data_init (self, AMMONITE_DBUS_LAUNCHER_CONSOLE);
    ammonite_dbus_launcher_data_init (self, AMMONITE_DBUS_LAUNCHER_PTYXIS);

    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL,
                              "org.freedesktop.DBus",
                              "/org/freedesktop/DBus",
                              "org.freedesktop.DBus",
                              self->cancellable, proxy_ready, self);

    ammonite_dbus_launcher_create_proxy (self->data[AMMONITE_DBUS_LAUNCHER_SETTINGS],
                                         "org.gnome.Settings", "/org/gnome/Settings",
                                         "org.gtk.Actions");

    ammonite_dbus_launcher_create_proxy (self->data[AMMONITE_DBUS_LAUNCHER_DISKS],
                                         "org.gnome.DiskUtility", "/org/gnome/DiskUtility",
                                         "org.gtk.Application");

    ammonite_dbus_launcher_create_proxy (self->data[AMMONITE_DBUS_LAUNCHER_CONSOLE],
                                         "org.gnome.Console", "/org/gnome/Console",
                                         "org.freedesktop.Application");

    ammonite_dbus_launcher_create_proxy (self->data[AMMONITE_DBUS_LAUNCHER_PTYXIS],
                                         "org.gnome.Ptyxis", "/org/gnome/Ptyxis",
                                         "org.freedesktop.Application");
}
