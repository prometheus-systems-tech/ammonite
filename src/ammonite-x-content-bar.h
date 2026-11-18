/*
 * SPDX-FileCopyrightText: 2008 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2006 Paolo Borelli <pborelli@katamail.com>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Paolo Borelli <pborelli@katamail.com>
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_X_CONTENT_BAR (ammonite_x_content_bar_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteXContentBar, ammonite_x_content_bar, AMMONITE, X_CONTENT_BAR, AdwBin)

GtkWidget *ammonite_x_content_bar_new (GMount             *mount,
                                       const char * const *x_content_types);

G_END_DECLS
