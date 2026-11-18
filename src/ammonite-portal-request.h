/*
 * SPDX-FileCopyrightText: 2016 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include "xdg-desktop-portal-dbus.h"

#define AMMONITE_TYPE_PORTAL_REQUEST (ammonite_portal_request_get_type ())
G_DECLARE_FINAL_TYPE (AmmonitePortalRequest,
                      ammonite_portal_request,
                      AMMONITE, PORTAL_REQUEST,
                      XdpImplRequestSkeleton)

AmmonitePortalRequest *
ammonite_portal_request_new (const char *sender,
                             const char *app_id,
                             const char *id);

void
ammonite_portal_request_export (AmmonitePortalRequest *request,
                                GDBusConnection       *connection);
void
ammonite_portal_request_unexport (AmmonitePortalRequest *request);
