/*
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

#include <config.h>
#include "ammonite-global-preferences.h"

#include "ammonite-file-utilities.h"

GSettings *ammonite_preferences;
GSettings *ammonite_compression_preferences;
GSettings *ammonite_icon_view_preferences;
GSettings *ammonite_list_view_preferences;
GSettings *ammonite_window_state;
GSettings *gtk_filechooser_preferences;
GSettings *gnome_lockdown_preferences;
GSettings *gnome_interface_preferences;
GSettings *gnome_privacy_preferences;
GSettings *gnome_user_share_preferences = NULL;
GSettings *localsearch_preferences;

#define FILE_SHARING_SCHEMA_ID "org.gnome.desktop.file-sharing"
#define FILE_SHARING_SERVICE_PATH "/org/gnome/settings-daemon/plugins/sharing/gnome-user-share-webdav/"
#define FILE_SHARING_SERVICE_SCHEMA_ID "org.gnome.settings-daemon.plugins.sharing.service"
#define UPPER_MOUSE_LIMIT 14
/* Forward and back buttons on the mouse */
static gboolean mouse_extra_buttons = TRUE;
static guint mouse_forward_button = 9;
static guint mouse_back_button = 8;

static void
use_extra_mouse_buttons_changed (gpointer callback_data)
{
    mouse_extra_buttons = g_settings_get_boolean (ammonite_preferences, AMMONITE_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS);
}

static void
mouse_back_button_changed (gpointer callback_data)
{
    int new_back_button;

    new_back_button = g_settings_get_uint (ammonite_preferences, AMMONITE_PREFERENCES_MOUSE_BACK_BUTTON);

    /* Bounds checking */
    if (new_back_button < 6 || new_back_button > UPPER_MOUSE_LIMIT)
    {
        return;
    }

    mouse_back_button = new_back_button;
}

static void
mouse_forward_button_changed (gpointer callback_data)
{
    int new_forward_button;

    new_forward_button = g_settings_get_uint (ammonite_preferences, AMMONITE_PREFERENCES_MOUSE_FORWARD_BUTTON);

    /* Bounds checking */
    if (new_forward_button < 6 || new_forward_button > UPPER_MOUSE_LIMIT)
    {
        return;
    }

    mouse_forward_button = new_forward_button;
}

void
ammonite_global_preferences_init (void)
{
    static gboolean initialized = FALSE;

    if (initialized)
    {
        return;
    }

    initialized = TRUE;

    ammonite_preferences = g_settings_new ("org.ammonite.ammonite.preferences");
    ammonite_compression_preferences = g_settings_new ("org.ammonite.ammonite.compression");
    ammonite_window_state = g_settings_new ("org.ammonite.ammonite.window-state");
    ammonite_icon_view_preferences = g_settings_new ("org.ammonite.ammonite.icon-view");
    ammonite_list_view_preferences = g_settings_new ("org.ammonite.ammonite.list-view");
    /* Some settings such as show hidden files are shared between Ammonite and GTK file chooser */
    gtk_filechooser_preferences = g_settings_new_with_path ("org.gtk.gtk4.Settings.FileChooser",
                                                            "/org/gtk/gtk4/settings/file-chooser/");
    gnome_lockdown_preferences = g_settings_new ("org.gnome.desktop.lockdown");
    gnome_interface_preferences = g_settings_new ("org.gnome.desktop.interface");
    gnome_privacy_preferences = g_settings_new ("org.gnome.desktop.privacy");
    localsearch_preferences = g_settings_new ("org.freedesktop.Tracker3.Miner.Files");

    if (check_schema_available (FILE_SHARING_SCHEMA_ID) &&
        check_schema_available (FILE_SHARING_SERVICE_SCHEMA_ID))
    {
        gnome_user_share_preferences = g_settings_new_with_path (FILE_SHARING_SERVICE_SCHEMA_ID,
                                                                 FILE_SHARING_SERVICE_PATH);
    }

    g_signal_connect_swapped (ammonite_preferences,
                              "changed::" AMMONITE_PREFERENCES_MOUSE_BACK_BUTTON,
                              G_CALLBACK (mouse_back_button_changed),
                              NULL);

    g_signal_connect_swapped (ammonite_preferences,
                              "changed::" AMMONITE_PREFERENCES_MOUSE_FORWARD_BUTTON,
                              G_CALLBACK (mouse_forward_button_changed),
                              NULL);

    g_signal_connect_swapped (ammonite_preferences,
                              "changed::" AMMONITE_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS,
                              G_CALLBACK (use_extra_mouse_buttons_changed),
                              NULL);
}

guint
ammonite_global_preferences_get_back_button (void)
{
    return mouse_back_button;
}

guint
ammonite_global_preferences_get_forward_button (void)
{
    return mouse_forward_button;
}

gboolean
ammonite_global_preferences_get_use_extra_buttons (void)
{
    return mouse_extra_buttons;
}
