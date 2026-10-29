/*
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Pavel Cisler <pavel@eazel.com>
 */

#include "ammonite-trash-monitor.h"

#include "ammonite-scheme.h"

#define UPDATE_RATE_SECONDS 1

struct _AmmoniteTrashMonitor
{
    GObject object;

    gboolean empty;
    GFileMonitor *file_monitor;
    gboolean pending;
    gint timeout_id;
};

enum
{
    TRASH_STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (AmmoniteTrashMonitor, ammonite_trash_monitor, G_TYPE_OBJECT)

static AmmoniteTrashMonitor *ammonite_trash_monitor_singleton = NULL;

static void
ammonite_trash_monitor_finalize (GObject *object)
{
    AmmoniteTrashMonitor *trash_monitor;

    trash_monitor = AMMONITE_TRASH_MONITOR (object);

    if (trash_monitor->timeout_id > 0)
    {
        g_source_remove (trash_monitor->timeout_id);
        trash_monitor->timeout_id = 0;
    }

    if (trash_monitor->file_monitor)
    {
        g_object_unref (trash_monitor->file_monitor);
    }

    G_OBJECT_CLASS (ammonite_trash_monitor_parent_class)->finalize (object);
}

static void
ammonite_trash_monitor_class_init (AmmoniteTrashMonitorClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = ammonite_trash_monitor_finalize;

    signals[TRASH_STATE_CHANGED] = g_signal_new
                                       ("trash-state-changed",
                                       G_TYPE_FROM_CLASS (object_class),
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__BOOLEAN,
                                       G_TYPE_NONE, 1,
                                       G_TYPE_BOOLEAN);
}

static void
update_empty_info (AmmoniteTrashMonitor *trash_monitor,
                   gboolean              is_empty)
{
    if (trash_monitor->empty == is_empty)
    {
        return;
    }

    trash_monitor->empty = is_empty;

    /* trash got empty or full, notify everyone who cares */
    g_signal_emit (trash_monitor,
                   signals[TRASH_STATE_CHANGED], 0,
                   trash_monitor->empty);
}

/* Use G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT since we only want to know whether the
 * trash is empty or not, not access its children. This is available for the
 * trash backend since it uses a cache. In this way we prevent flooding the
 * trash backend with enumeration requests when trashing > 1000 files
 */
static void
trash_query_info_cb (GObject      *source,
                     GAsyncResult *res,
                     gpointer      user_data)
{
    AmmoniteTrashMonitor *trash_monitor = user_data;
    GFileInfo *info;
    guint32 item_count;
    gboolean is_empty = TRUE;

    info = g_file_query_info_finish (G_FILE (source), res, NULL);

    if (info != NULL)
    {
        item_count = g_file_info_get_attribute_uint32 (info,
                                                       G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
        is_empty = item_count == 0;

        g_object_unref (info);
    }

    update_empty_info (trash_monitor, is_empty);

    g_object_unref (trash_monitor);
}

static void schedule_update_info (AmmoniteTrashMonitor *trash_monitor);

static void
schedule_update_info_cb (gpointer data)
{
    AmmoniteTrashMonitor *trash_monitor = data;

    trash_monitor->timeout_id = 0;
    if (trash_monitor->pending)
    {
        trash_monitor->pending = FALSE;
        schedule_update_info (trash_monitor);
    }
}

static void
schedule_update_info (AmmoniteTrashMonitor *trash_monitor)
{
    GFile *location;

    /* Rate limit the updates to not flood the gvfsd-trash when too many changes
     * happened in a short time.
     */
    if (trash_monitor->timeout_id > 0)
    {
        trash_monitor->pending = TRUE;
        return;
    }

    location = g_file_new_for_uri (SCHEME_TRASH ":///");
    g_file_query_info_async (location,
                             G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT,
                             G_FILE_QUERY_INFO_NONE,
                             G_PRIORITY_DEFAULT, NULL,
                             trash_query_info_cb, g_object_ref (trash_monitor));

    trash_monitor->timeout_id =
        g_timeout_add_seconds_once (UPDATE_RATE_SECONDS, schedule_update_info_cb, trash_monitor);
    g_object_unref (location);
}

static void
file_changed (GFileMonitor      *monitor,
              GFile             *child,
              GFile             *other_file,
              GFileMonitorEvent  event_type,
              gpointer           user_data)
{
    AmmoniteTrashMonitor *trash_monitor;

    trash_monitor = AMMONITE_TRASH_MONITOR (user_data);

    schedule_update_info (trash_monitor);
}

static void
ammonite_trash_monitor_init (AmmoniteTrashMonitor *trash_monitor)
{
    GFile *location;

    trash_monitor->empty = TRUE;

    location = g_file_new_for_uri (SCHEME_TRASH ":///");

    trash_monitor->file_monitor = g_file_monitor_file (location, 0, NULL, NULL);
    trash_monitor->pending = FALSE;
    trash_monitor->timeout_id = 0;

    g_signal_connect (trash_monitor->file_monitor, "changed",
                      (GCallback) file_changed, trash_monitor);

    g_object_unref (location);

    schedule_update_info (trash_monitor);
}

AmmoniteTrashMonitor *
ammonite_trash_monitor_get (void)
{
    if (ammonite_trash_monitor_singleton == NULL)
    {
        /* not running yet, start it up */
        ammonite_trash_monitor_singleton = AMMONITE_TRASH_MONITOR
                                               (g_object_new (AMMONITE_TYPE_TRASH_MONITOR, NULL));
    }

    return ammonite_trash_monitor_singleton;
}

gboolean
ammonite_trash_monitor_is_empty (void)
{
    AmmoniteTrashMonitor *monitor;

    monitor = ammonite_trash_monitor_get ();
    return monitor->empty;
}

GIcon *
ammonite_trash_monitor_get_symbolic_icon (void)
{
    gboolean empty;

    empty = ammonite_trash_monitor_is_empty ();

    if (empty)
    {
        return g_themed_icon_new ("user-trash-symbolic");
    }
    else
    {
        return g_themed_icon_new ("user-trash-full-symbolic");
    }
}

void
ammonite_trash_monitor_clear (void)
{
    g_clear_object (&ammonite_trash_monitor_singleton);
}
