/*
 * SPDX-FileCopyrightText: 2024 GNOME Foundation Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_VIEW_ITEM_FILTER (ammonite_view_item_filter_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteViewItemFilter, ammonite_view_item_filter, AMMONITE, VIEW_ITEM_FILTER, GtkFilter)

AmmoniteViewItemFilter *
ammonite_view_item_filter_new (void);

void
ammonite_view_item_filter_set_file_filter (AmmoniteViewItemFilter *self,
                                           GtkFileFilter          *file_filter);

G_END_DECLS
