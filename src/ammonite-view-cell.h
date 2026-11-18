/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_VIEW_CELL (ammonite_view_cell_get_type())

G_DECLARE_DERIVABLE_TYPE (AmmoniteViewCell, ammonite_view_cell, AMMONITE, VIEW_CELL, GtkWidget)

struct _AmmoniteViewCellClass
{
    GtkWidgetClass parent_class;
};

AmmoniteListBase *ammonite_view_cell_get_view (AmmoniteViewCell *self);
AmmoniteViewItem *ammonite_view_cell_get_item (AmmoniteViewCell *self);
guint ammonite_view_cell_get_position (AmmoniteViewCell *self);

G_END_DECLS
