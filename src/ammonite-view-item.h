/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_VIEW_ITEM (ammonite_view_item_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteViewItem, ammonite_view_item, AMMONITE, VIEW_ITEM, GObject)

AmmoniteViewItem * ammonite_view_item_new           (AmmoniteFile *file);

void               ammonite_view_item_set_cut       (AmmoniteViewItem *self,
                                                     gboolean          is_cut);
void               ammonite_view_item_set_drag_accept (AmmoniteViewItem *self,
                                                       gboolean          drag_accept);
gboolean           ammonite_view_item_get_loading   (AmmoniteViewItem *self);
void               ammonite_view_item_set_loading   (AmmoniteViewItem *self,
                                                     gboolean          is_loading);

AmmoniteFile *     ammonite_view_item_get_file      (AmmoniteViewItem *self);

void               ammonite_view_item_set_item_ui   (AmmoniteViewItem *self,
                                                     GtkWidget        *item_ui);

GtkWidget *        ammonite_view_item_get_item_ui   (AmmoniteViewItem *self);
void               ammonite_view_item_file_changed  (AmmoniteViewItem *self);

G_END_DECLS
