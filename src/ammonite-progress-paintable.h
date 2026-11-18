/*
 * SPDX-FileCopyrightText: 2022 Purism SPC
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_PROGRESS_PAINTABLE (ammonite_progress_paintable_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteProgressPaintable, ammonite_progress_paintable, AMMONITE, PROGRESS_PAINTABLE, GObject)

GdkPaintable *ammonite_progress_paintable_new          (GtkWidget              *widget);

void          ammonite_progress_paintable_animate_done (AmmoniteProgressPaintable *self);

G_END_DECLS
