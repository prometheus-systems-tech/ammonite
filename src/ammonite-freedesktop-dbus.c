/*
 * ammonite-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
 *
 * SPDX-FileCopyrightText: 2011 Federico Mena Quintero <federico@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 */
#define G_LOG_DOMAIN "ammonite-dbus"

#include "ammonite-freedesktop-dbus.h"

#include "ammonite-application.h"
#include "ammonite-file.h"
#include "ammonite-freedesktop-generated.h"
#include "ammonite-properties.h"

#include <gio/gio.h>

struct _AmmoniteFreedesktopDBus
{
    GObject parent;

    /* Id from g_dbus_own_name() */
    guint owner_id;

    /* Our DBus implementation skeleton */
    AmmoniteFreedesktopFileManager1 *skeleton;
};

G_DEFINE_FINAL_TYPE (AmmoniteFreedesktopDBus, ammonite_freedesktop_dbus, G_TYPE_OBJECT);

static gboolean
skeleton_handle_show_items_cb (AmmoniteFreedesktopFileManager1 *object,
                               GDBusMethodInvocation           *invocation,
                               const gchar * const             *uris,
                               const gchar                     *startup_id,
                               gpointer                         data)
{
    AmmoniteApplication *application;
    int i;

    application = AMMONITE_APPLICATION (g_application_get_default ());

    for (i = 0; uris[i] != NULL; i++)
    {
        g_autoptr (GFile) file = NULL;
        g_autoptr (GFile) parent = NULL;

        file = g_file_new_for_uri (uris[i]);
        parent = g_file_get_parent (file);

        if (parent != NULL)
        {
            ammonite_application_open_location (application, parent, file, startup_id);
        }
        else
        {
            ammonite_application_open_location (application, file, NULL, startup_id);
        }
    }

    ammonite_freedesktop_file_manager1_complete_show_items (object, invocation);
    return TRUE;
}

static gboolean
skeleton_handle_show_folders_cb (AmmoniteFreedesktopFileManager1 *object,
                                 GDBusMethodInvocation           *invocation,
                                 const gchar * const             *uris,
                                 const gchar                     *startup_id,
                                 gpointer                         data)
{
    AmmoniteApplication *application;
    int i;

    application = AMMONITE_APPLICATION (g_application_get_default ());

    for (i = 0; uris[i] != NULL; i++)
    {
        g_autoptr (GFile) file = NULL;

        file = g_file_new_for_uri (uris[i]);

        ammonite_application_open_location (application, file, NULL, startup_id);
    }

    ammonite_freedesktop_file_manager1_complete_show_folders (object, invocation);
    return TRUE;
}

static gboolean
skeleton_handle_show_item_properties_cb (AmmoniteFreedesktopFileManager1 *object,
                                         GDBusMethodInvocation           *invocation,
                                         const gchar * const             *uris,
                                         const gchar                     *startup_id,
                                         gpointer                         data)
{
    GApplication *application = g_application_get_default ();
    g_autolist (AmmoniteFile) files = ammonite_file_list_from_uris ((GStrv) uris);
    GtkWindow *window = ammonite_properties_present_window (files, startup_id);

    g_application_hold (application);
    g_signal_connect_swapped (window, "destroyed",
                              G_CALLBACK (g_application_release), application);

    ammonite_freedesktop_file_manager1_complete_show_item_properties (object, invocation);
    return TRUE;
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
    g_debug ("Acquired the name %s on the session message bus", name);
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
    g_debug ("Lost (or failed to acquire) the name %s on the session message bus", name);
}

static void
ammonite_freedesktop_dbus_dispose (GObject *object)
{
    AmmoniteFreedesktopDBus *fdb = (AmmoniteFreedesktopDBus *) object;

    if (fdb->owner_id != 0)
    {
        g_bus_unown_name (fdb->owner_id);
        fdb->owner_id = 0;
    }

    g_clear_object (&fdb->skeleton);

    G_OBJECT_CLASS (ammonite_freedesktop_dbus_parent_class)->dispose (object);
}

static void
ammonite_freedesktop_dbus_class_init (AmmoniteFreedesktopDBusClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = ammonite_freedesktop_dbus_dispose;
}

static void
ammonite_freedesktop_dbus_init (AmmoniteFreedesktopDBus *fdb)
{
    fdb->skeleton = ammonite_freedesktop_file_manager1_skeleton_new ();
}

void
ammonite_freedesktop_dbus_set_open_locations (AmmoniteFreedesktopDBus  *fdb,
                                              const gchar             **locations)
{
    g_return_if_fail (AMMONITE_IS_FREEDESKTOP_DBUS (fdb));

    ammonite_freedesktop_file_manager1_set_open_locations (fdb->skeleton, locations);
}

/**
 * ammonite_freedesktop_dbus_set_open_windows_with_locations:
 * fdb: The skeleton for the dbus interface
 * locations: Mapping of windows to locations open in each window
 *
 * This allows the application to publish the locations that are open in each window.
 * It is used by shell extensions like dash-to-dock/ubuntu-dock to match special dock
 * icons to the windows where the icon's location is open. For example, the Trash or
 * a removable device.
 */
void
ammonite_freedesktop_dbus_set_open_windows_with_locations (AmmoniteFreedesktopDBus *fdb,
                                                           GVariant                *locations)
{
    g_return_if_fail (AMMONITE_IS_FREEDESKTOP_DBUS (fdb));

    ammonite_freedesktop_file_manager1_set_open_windows_with_locations (fdb->skeleton,
                                                                        locations);
}

AmmoniteFreedesktopDBus *
ammonite_freedesktop_dbus_new (void)
{
    return g_object_new (AMMONITE_TYPE_FREEDESKTOP_DBUS, NULL);
}

/* Tries to own the org.freedesktop.FileManager1 service name */
gboolean
ammonite_freedesktop_dbus_register (AmmoniteFreedesktopDBus  *fdb,
                                    GDBusConnection          *connection,
                                    GError                  **error)
{
    gboolean success;

    success = g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (fdb->skeleton),
                                                connection,
                                                AMMONITE_FDO_DBUS_PATH,
                                                error);

    if (success)
    {
        g_signal_connect (fdb->skeleton, "handle-show-items",
                          G_CALLBACK (skeleton_handle_show_items_cb), fdb);
        g_signal_connect (fdb->skeleton, "handle-show-folders",
                          G_CALLBACK (skeleton_handle_show_folders_cb), fdb);
        g_signal_connect (fdb->skeleton, "handle-show-item-properties",
                          G_CALLBACK (skeleton_handle_show_item_properties_cb), fdb);
    }

    fdb->owner_id = g_bus_own_name_on_connection (connection,
                                                  AMMONITE_FDO_DBUS_NAME,
                                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                                  name_acquired_cb,
                                                  name_lost_cb,
                                                  fdb,
                                                  NULL);

    return success;
}

void
ammonite_freedesktop_dbus_unregister (AmmoniteFreedesktopDBus *fdb)
{
    if (fdb->owner_id != 0)
    {
        g_bus_unown_name (fdb->owner_id);
        fdb->owner_id = 0;
    }

    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (fdb->skeleton));

    g_signal_handlers_disconnect_by_data (fdb->skeleton, fdb);
}
