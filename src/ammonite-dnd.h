/*
 * ammonite-dnd.h - Common Drag & drop handling code
 *
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Pavel Cisler <pavel@eazel.com>,
 *          Ettore Perazzoli <ettore@gnu.org>
 */

#pragma once

#include "ammonite-types.h"

#include <gtk/gtk.h>

#define HOVER_TIMEOUT 500

#define AMMONITE_DRAG_SURFACE_ICON_SIZE 64

GdkDragAction      ammonite_drag_drop_action_ask                 (GtkWidget        *widget,
                                                                  GdkDragAction     possible_actions);

GdkDragAction      ammonite_dnd_get_preferred_action             (AmmoniteFile     *target_file,
                                                                  GFile            *dropped);
GdkPaintable *     get_paintable_for_drag_selection              (GList            *selection,
                                                                  GtkWidget        *widget,
                                                                  int               scale);

gboolean           ammonite_dnd_perform_drop                     (AmmoniteFilesView *view,
                                                                  const GValue      *value,
                                                                  GdkDragAction      action,
                                                                  GFile             *target_location);
