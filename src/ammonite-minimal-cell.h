/*
 * SPDX-FileCopyrightText: 2024 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#pragma once

#include <gdk/gdk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_MINIMAL_CELL (ammonite_minimal_cell_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteMinimalCell, ammonite_minimal_cell, AMMONITE, MINIMAL_CELL, GObject)

AmmoniteMinimalCell * ammonite_minimal_cell_new (gchar        *title,
                                                 gchar        *subtitle,
                                                 GdkPaintable *paintable);

const gchar * ammonite_minimal_cell_get_subtitle (AmmoniteMinimalCell *self);
const gchar * ammonite_minimal_cell_get_title (AmmoniteMinimalCell *self);

G_END_DECLS
