/*
 * SPDX-FileCopyrightText: 2015 Carlos Soriano <csoriano@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.1-or-later
 */
#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_SIDEBAR_ROW (ammonite_sidebar_row_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteSidebarRow,
                      ammonite_sidebar_row,
                      AMMONITE, SIDEBAR_ROW,
                      GtkListBoxRow)

AmmoniteSidebarRow *ammonite_sidebar_row_new_placeholder (void);

AmmoniteSidebarRow *ammonite_sidebar_row_clone  (AmmoniteSidebarRow *self);

/* Use these methods instead of gtk_widget_hide/show to use an animation */
void           ammonite_sidebar_row_hide   (AmmoniteSidebarRow *self,
                                       gboolean       immediate);
void           ammonite_sidebar_row_reveal (AmmoniteSidebarRow *self);

GtkWidget     *ammonite_sidebar_row_get_eject_button (AmmoniteSidebarRow *self);
void           ammonite_sidebar_row_set_start_icon   (AmmoniteSidebarRow *self,
                                                 GIcon         *icon);
void           ammonite_sidebar_row_set_busy         (AmmoniteSidebarRow *row,
                                                 gboolean       is_busy);

G_END_DECLS
