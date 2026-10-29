/*
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <string.h>

#include "ammonite-dbus-launcher.h"
#include "ammonite-file.h"
#include "ammonite-file-operations.h"
#include "ammonite-file-utilities.h"
#include "ammonite-global-preferences.h"
#include "ammonite-location-banner.h"
#include "ammonite-trash-monitor.h"

#define USER_SHARE_CONNECTIONS "enabled-connections"

typedef enum
{
    AMMONITE_LOCATION_BANNER_NONE,
    AMMONITE_LOCATION_BANNER_SCRIPTS,
    AMMONITE_LOCATION_BANNER_SHARING,
    AMMONITE_LOCATION_BANNER_TEMPLATES,
    AMMONITE_LOCATION_BANNER_TRASH,
    AMMONITE_LOCATION_BANNER_TRASH_AUTO_EMPTIED,
} AmmoniteLocationBannerMode;

static void set_mode (AdwBanner                 *banner,
                      AmmoniteLocationBannerMode mode);

static void
on_sharing_clicked (AdwBanner *banner)
{
    GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (banner)));
    GVariant *parameters = g_variant_new_parsed (
        "('launch-panel', [<('sharing', @av [])>], @a{sv} {})");

    ammonite_dbus_launcher_call (ammonite_dbus_launcher_get (),
                                 AMMONITE_DBUS_LAUNCHER_SETTINGS,
                                 "Activate",
                                 parameters, window);
}

static void
on_template_clicked (AdwBanner *banner)
{
    GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (banner)));
    g_autoptr (GtkUriLauncher) launcher = gtk_uri_launcher_new ("help:gnome-help/files-templates");
    gtk_uri_launcher_launch (launcher, window, NULL, NULL, NULL);
}

static void
on_trash_auto_emptied_clicked (AdwBanner *banner)
{
    GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (banner)));
    const gchar *parameters = "('launch-panel', [<('privacy', [<'usage'>])>], @a{sv} {})";

    ammonite_dbus_launcher_call (ammonite_dbus_launcher_get (),
                                 AMMONITE_DBUS_LAUNCHER_SETTINGS,
                                 "Activate",
                                 g_variant_new_parsed (parameters),
                                 window);
}

static void
on_trash_clear_clicked (AdwBanner *banner)
{
    GtkWidget *window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (banner)));

    ammonite_file_operations_empty_trash (window, TRUE, NULL);
}

static gchar *
parse_old_files_age_preferences_value (void)
{
    guint old_files_age = g_settings_get_uint (gnome_privacy_preferences, "old-files-age");

    switch (old_files_age)
    {
        case 0:
        {
            return g_strdup (_("Files in the trash are permanently deleted after 1 hour"));
        }

        default:
        {
            return g_strdup_printf (ngettext ("Files in the trash are permanently deleted after %d day",
                                              "Files in the trash are permanently deleted after %d days",
                                              old_files_age),
                                    old_files_age);
        }
    }
}

static void
set_auto_emptied_message (AdwBanner *banner)
{
    g_autofree gchar *message = parse_old_files_age_preferences_value ();

    adw_banner_set_title (banner, message);
}

static void
on_remove_old_trash_files_changed (GSettings *settings,
                                   gchar     *key,
                                   gpointer   callback_data)
{
    AdwBanner *banner = ADW_BANNER (callback_data);
    gboolean auto_emptied = g_settings_get_boolean (settings, key);

    set_mode (banner, auto_emptied ? AMMONITE_LOCATION_BANNER_TRASH_AUTO_EMPTIED :
                                     AMMONITE_LOCATION_BANNER_TRASH);
}

static void
on_user_share_setting_changed (GSettings *settings,
                               gchar     *key,
                               gpointer   callback_data)
{
    AdwBanner *banner = ADW_BANNER (callback_data);

    set_mode (banner, AMMONITE_LOCATION_BANNER_SHARING);
}

static gboolean
is_scripts_location (GFile *location)
{
    g_autofree char *scripts_path = ammonite_get_scripts_directory_path ();
    g_autoptr (GFile) scripts_file = g_file_new_for_path (scripts_path);

    return g_file_equal (location, scripts_file);
}

static AmmoniteLocationBannerMode
get_mode_for_location (GFile *location)
{
    g_autoptr (AmmoniteFile) file = NULL;

    if (location == NULL)
    {
        return AMMONITE_LOCATION_BANNER_NONE;
    }

    file = ammonite_file_get (location);

    if (ammonite_should_use_templates_directory () &&
        ammonite_file_is_user_special_directory (file, G_USER_DIRECTORY_TEMPLATES))
    {
        return AMMONITE_LOCATION_BANNER_TEMPLATES;
    }
    else if (is_scripts_location (location))
    {
        return AMMONITE_LOCATION_BANNER_SCRIPTS;
    }
    else if (gnome_user_share_preferences != NULL && ammonite_file_is_public_share_folder (file))
    {
        return AMMONITE_LOCATION_BANNER_SHARING;
    }
    else if (ammonite_file_is_in_trash (file))
    {
        gboolean auto_emptied = g_settings_get_boolean (gnome_privacy_preferences, "remove-old-trash-files");
        return (auto_emptied ?
                AMMONITE_LOCATION_BANNER_TRASH_AUTO_EMPTIED :
                AMMONITE_LOCATION_BANNER_TRASH);
    }
    else
    {
        return AMMONITE_LOCATION_BANNER_NONE;
    }
}

static void
update_trash_banner_visibility (AdwBanner *banner)
{
    adw_banner_set_revealed (banner, !ammonite_trash_monitor_is_empty ());
}

static void
set_mode (AdwBanner                  *banner,
          AmmoniteLocationBannerMode  mode)
{
    const char *button_label = NULL;
    GCallback callback = NULL;
    gboolean banner_is_revealed = TRUE;

    /* Reset signal handlers. */
    g_signal_handlers_disconnect_by_data (banner, &ammonite_location_banner_load);
    g_signal_handlers_disconnect_by_data (gnome_privacy_preferences, banner);
    g_signal_handlers_disconnect_by_data (ammonite_trash_monitor_get (), banner);
    if (gnome_user_share_preferences != NULL)
    {
        g_signal_handlers_disconnect_by_data (gnome_user_share_preferences, banner);
    }

    switch (mode)
    {
        case AMMONITE_LOCATION_BANNER_NONE:
        {
            adw_banner_set_revealed (banner, FALSE);
            return;
        }
        break;

        case AMMONITE_LOCATION_BANNER_SCRIPTS:
        {
            adw_banner_set_title (banner, _("Executable files in this folder will appear in the Scripts menu"));
        }
        break;

        case AMMONITE_LOCATION_BANNER_SHARING:
        {
            /* mode is only set when gnome_user_share_preferences exist */
            g_auto (GStrv) connections = g_settings_get_strv (gnome_user_share_preferences,
                                                              USER_SHARE_CONNECTIONS);

            g_signal_connect_object (gnome_user_share_preferences,
                                     "changed::" USER_SHARE_CONNECTIONS,
                                     G_CALLBACK (on_user_share_setting_changed),
                                     banner, G_CONNECT_DEFAULT);

            if (connections != NULL && connections[0] != NULL)
            {
                adw_banner_set_title (
                    banner, _("This folder is shared over the network"));
            }
            else
            {
                adw_banner_set_title (
                    banner, _("Turn on File Sharing to share this folder over the network"));
            }
            button_label = _("Sharing Settings");
            callback = G_CALLBACK (on_sharing_clicked);
        }
        break;

        case AMMONITE_LOCATION_BANNER_TEMPLATES:
        {
            adw_banner_set_title (banner, _("Put files in this folder to use them as templates for new documents"));
            button_label = _("_Learn More");
            callback = G_CALLBACK (on_template_clicked);
        }
        break;

        case AMMONITE_LOCATION_BANNER_TRASH:
        {
            adw_banner_set_title (banner, "");
            if (ammonite_trash_monitor_is_empty ())
            {
                banner_is_revealed = FALSE;
            }

            button_label = _("_Empty Trash…");
            callback = G_CALLBACK (on_trash_clear_clicked);

            g_signal_connect_object (ammonite_trash_monitor_get (),
                                     "trash-state-changed",
                                     G_CALLBACK (update_trash_banner_visibility),
                                     banner, G_CONNECT_SWAPPED);
            g_signal_connect_object (gnome_privacy_preferences,
                                     "changed::remove-old-trash-files",
                                     G_CALLBACK (on_remove_old_trash_files_changed),
                                     banner, G_CONNECT_DEFAULT);
        }
        break;

        case AMMONITE_LOCATION_BANNER_TRASH_AUTO_EMPTIED:
        {
            set_auto_emptied_message (banner);
            button_label = _("_Trash Settings");
            callback = G_CALLBACK (on_trash_auto_emptied_clicked);

            g_signal_connect_object (gnome_privacy_preferences,
                                     "changed::old-files-age",
                                     G_CALLBACK (set_auto_emptied_message),
                                     banner, G_CONNECT_SWAPPED);
            g_signal_connect_object (gnome_privacy_preferences,
                                     "changed::remove-old-trash-files",
                                     G_CALLBACK (on_remove_old_trash_files_changed),
                                     banner, G_CONNECT_DEFAULT);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }

    adw_banner_set_button_label (banner, button_label);
    adw_banner_set_revealed (banner, banner_is_revealed);

    if (callback != NULL)
    {
        g_signal_connect (banner, "button-clicked", callback, &ammonite_location_banner_load);
    }
}

void
ammonite_location_banner_load (AdwBanner *banner,
                               GFile     *location)
{
    set_mode (banner, get_mode_for_location (location));
}
