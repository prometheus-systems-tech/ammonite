/*
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

void ammonite_location_banner_load (AdwBanner *banner,
                                    GFile     *location);

G_END_DECLS
