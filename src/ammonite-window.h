
/*
 * SPDX-FileCopyrightText: 1999, 2000 Red Hat, Inc.
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Elliot Lee <sopwith@redhat.com>
 *          Darin Adler <darin@bentspoon.com>
 */
/* ammonite-window.h: Interface of the main window object */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

#include "ammonite-types.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_WINDOW (ammonite_window_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteWindow, ammonite_window, AMMONITE, WINDOW, AdwApplicationWindow);

typedef gboolean (* AmmoniteWindowGoToCallback) (AmmoniteWindow *window,
                                                 GFile *location,
                                                 GError *error,
                                                 gpointer user_data);

#define AMMONITE_NAVIGATION_DIRECTION_BACK -1
#define AMMONITE_NAVIGATION_DIRECTION_FORWARD 1

AmmoniteWindow * ammonite_window_new                  (void);
void             ammonite_window_close                (AmmoniteWindow    *window);

void ammonite_window_open_location_full               (AmmoniteWindow     *window,
                                                       GFile              *location,
                                                       AmmoniteOpenFlags   flags,
                                                       AmmoniteFileList   *selection);

void             ammonite_window_new_tab              (AmmoniteWindow    *window);

gboolean
ammonite_window_has_open_location (AmmoniteWindow *self,
                                   GFile          *location);
GFile *
ammonite_window_get_active_location (AmmoniteWindow *self);
GList *
ammonite_window_get_locations (AmmoniteWindow *self);

void     ammonite_window_show_about_dialog    (AmmoniteWindow *window);

void ammonite_window_show_operation_notification (AmmoniteWindow *window,
                                                  gchar          *main_label,
                                                  GFile          *folder_to_open,
                                                  gboolean        was_quick);

void ammonite_window_search (AmmoniteWindow *window,
                             AmmoniteQuery  *query);

void ammonite_window_back_or_forward_in_new_tab (AmmoniteWindow *window,
                                                 int             distance);

G_END_DECLS
