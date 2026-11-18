/*
 * ammonite-application: main Ammonite application class.
 *
 * SPDX-FileCopyrightText: 2000 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2010 Cosimo Cecchi <cosimoc@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "ammonite-types.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_APPLICATION (ammonite_application_get_type())
G_DECLARE_FINAL_TYPE (AmmoniteApplication, ammonite_application,
                      AMMONITE, APPLICATION, AdwApplication)

AmmoniteApplication * ammonite_application_new (void);

AmmoniteWindow *
ammonite_application_create_window (AmmoniteApplication *self);

void ammonite_application_set_accelerator (GApplication *app,
					   const gchar  *action_name,
					   const gchar  *accel);

GList * ammonite_application_get_windows (AmmoniteApplication *application);

void ammonite_application_open_location (AmmoniteApplication *application,
					 GFile *location,
					 GFile *selection,
					 const char *startup_id);

AmmoniteWindow *
ammonite_application_open_location_full (AmmoniteApplication *application,
                                         GFile               *location,
                                         AmmoniteOpenFlags    flags,
                                         AmmoniteFileList    *selection,
                                         const char          *startup_id);

AmmoniteApplication *ammonite_application_get_default (void);

/* Notification category strings to use with g_notification_set_catergory ().
 * From https://specifications.freedesktop.org/notification/latest/categories.html */

#define XDG_NOTIFICATION_CATEGORY_TRANSFER "transfer"
#define XDG_NOTIFICATION_CATEGORY_TRANSFER_COMPLETE "transfer.complete"
#define XDG_NOTIFICATION_CATEGORY_DEVICE "device"
#define XDG_NOTIFICATION_CATEGORY_DEVICE_REMOVED "device.removed"

void ammonite_application_send_notification (AmmoniteApplication *self,
                                             const gchar         *notification_id,
                                             GNotification       *notification);
void ammonite_application_withdraw_notification (AmmoniteApplication *self,
                                                 const gchar         *notification_id);

AmmoniteBookmarkList *
     ammonite_application_get_bookmarks  (AmmoniteApplication *application);

void ammonite_application_search (AmmoniteApplication *application,
                                  AmmoniteQuery       *query);
gboolean ammonite_application_is_sandboxed (void);
G_END_DECLS
