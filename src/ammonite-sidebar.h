/*
 * SPDX-FileCopyrightText: 2015 Red Hat
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.1-or-later
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */
#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "ammonite-enums.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_PLACES_SIDEBAR (ammonite_sidebar_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteSidebar,
                      ammonite_sidebar,
                      AMMONITE, PLACES_SIDEBAR,
                      GtkWidget)

GtkWidget *        ammonite_sidebar_new                        (void);

AmmoniteOpenFlags  ammonite_sidebar_get_open_flags             (AmmoniteSidebar   *sidebar);
void               ammonite_sidebar_set_open_flags             (AmmoniteSidebar   *sidebar,
                                                                           AmmoniteOpenFlags           flags);

GFile *            ammonite_sidebar_get_location               (AmmoniteSidebar   *sidebar);
void               ammonite_sidebar_set_location               (AmmoniteSidebar   *sidebar,
                                                                  GFile              *location);

GFile *            ammonite_sidebar_get_nth_bookmark           (AmmoniteSidebar   *sidebar,
                                                                  int                 n);
void               ammonite_sidebar_set_drop_targets_visible   (AmmoniteSidebar   *sidebar,
                                                                  gboolean            visible);

void               ammonite_sidebar_set_show_trash             (AmmoniteSidebar   *sidebar,
                                                                           gboolean                    show_trash);

G_END_DECLS
