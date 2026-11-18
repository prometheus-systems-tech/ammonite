/*
 * ammonite-monitor.c: file and directory change monitoring for ammonite
 *
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2016 Red Hat
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Seth Nickell <seth@eazel.com>
 *          Darin Adler <darin@bentspoon.com>
 *          Alex Graveley <alex@ximian.com>
 *          Carlos Soriano <csoriano@gnome.org>
 */

#include <config.h>
#include "ammonite-monitor.h"
#include "ammonite-file-changes-queue.h"

#include <gio/gio.h>

struct AmmoniteMonitor
{
    GFileMonitor *monitor;
    GVolumeMonitor *volume_monitor;
    GFile *location;
};

static gboolean call_consume_changes_idle_id = 0;

static void
call_consume_changes_idle_cb (gpointer not_used)
{
    ammonite_file_changes_consume_changes ();
    call_consume_changes_idle_id = 0;
}

static void
schedule_call_consume_changes (void)
{
    if (call_consume_changes_idle_id == 0)
    {
        call_consume_changes_idle_id =
            g_idle_add_once (call_consume_changes_idle_cb, NULL);
    }
}

static void
mount_removed (GVolumeMonitor *volume_monitor,
               GMount         *mount,
               gpointer        user_data)
{
    AmmoniteMonitor *monitor = user_data;
    GFile *mount_location;

    mount_location = g_mount_get_root (mount);
    if (g_file_equal (monitor->location, mount_location) ||
        g_file_has_prefix (monitor->location, mount_location))
    {
        ammonite_file_changes_queue_file_unmounted (monitor->location);
        schedule_call_consume_changes ();
    }

    g_object_unref (mount_location);
}

static void
dir_changed (GFileMonitor      *monitor,
             GFile             *child,
             GFile             *other_file,
             GFileMonitorEvent  event_type,
             gpointer           user_data)
{
    switch (event_type)
    {
        default:
        case G_FILE_MONITOR_EVENT_CHANGED:
        {
            /* ignore */
        }
        break;

        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        {
            ammonite_file_changes_queue_file_changed (child);
        }
        break;

        case G_FILE_MONITOR_EVENT_UNMOUNTED:
        {
            ammonite_file_changes_queue_file_unmounted (child);
        }
        break;

        case G_FILE_MONITOR_EVENT_DELETED:
        {
            ammonite_file_changes_queue_file_removed (child);
        }
        break;

        case G_FILE_MONITOR_EVENT_CREATED:
        {
            ammonite_file_changes_queue_file_added (child);
        }
        break;

        case G_FILE_MONITOR_EVENT_MOVED_IN:
        {
            if (other_file != NULL)
            {
                ammonite_file_changes_queue_file_moved (other_file, child);
            }
            else
            {
                ammonite_file_changes_queue_file_added (child);
            }
        }
        break;

        case G_FILE_MONITOR_EVENT_MOVED_OUT:
        {
            if (other_file != NULL)
            {
                ammonite_file_changes_queue_file_moved (child, other_file);
            }
            else
            {
                ammonite_file_changes_queue_file_removed (child);
            }
        }
        break;

        case G_FILE_MONITOR_EVENT_RENAMED:
        {
            ammonite_file_changes_queue_file_moved (child, other_file);
        }
        break;
    }

    schedule_call_consume_changes ();
}

AmmoniteMonitor *
ammonite_monitor_directory (GFile *location)
{
    GFileMonitor *dir_monitor;
    AmmoniteMonitor *ret;

    ret = g_slice_new0 (AmmoniteMonitor);
    dir_monitor = g_file_monitor_directory (location,
                                            G_FILE_MONITOR_WATCH_MOUNTS | G_FILE_MONITOR_WATCH_MOVES,
                                            NULL, NULL);

    if (dir_monitor != NULL)
    {
        ret->monitor = dir_monitor;
    }

    /* Currently, some GVfs backends which support monitoring never emit
     * G_FILE_MONITOR_EVENT_UNMOUNTED, nor _DELETED events when the location
     * is unmounted. Use GVolumeMonitor in addition to GFileMonitor.
     */
    if (!g_file_is_native (location))
    {
        ret->location = g_object_ref (location);
        ret->volume_monitor = g_volume_monitor_get ();
    }

    if (ret->monitor != NULL)
    {
        g_signal_connect (ret->monitor, "changed",
                          G_CALLBACK (dir_changed), ret);
    }

    if (ret->volume_monitor != NULL)
    {
        g_signal_connect (ret->volume_monitor, "mount-removed",
                          G_CALLBACK (mount_removed), ret);
    }

    /* We return a monitor even on failure, so we can avoid later trying again */
    return ret;
}

void
ammonite_monitor_cancel (AmmoniteMonitor *monitor)
{
    if (monitor->monitor != NULL)
    {
        g_signal_handlers_disconnect_by_func (monitor->monitor, dir_changed, monitor);
        g_file_monitor_cancel (monitor->monitor);
        g_object_unref (monitor->monitor);
    }

    if (monitor->volume_monitor != NULL)
    {
        g_signal_handlers_disconnect_by_func (monitor->volume_monitor, mount_removed, monitor);
        g_object_unref (monitor->volume_monitor);
    }

    g_clear_object (&monitor->location);
    g_slice_free (AmmoniteMonitor, monitor);
}
