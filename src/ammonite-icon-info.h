/*
 * SPDX-FileCopyrightText: 2007 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2007 Alexander Larsson <alexl@redhat.com>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_ICON_INFO (ammonite_icon_info_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteIconInfo, ammonite_icon_info, AMMONITE, ICON_INFO, GObject)

GIcon *
ammonite_icon_info_get_default_file_icon (void);

GdkPaintable *        ammonite_icon_info_lookup                       (GIcon             *icon,
								       int                size,
								       int                scale);

void                  ammonite_icon_info_clear_caches                 (void);

G_END_DECLS
