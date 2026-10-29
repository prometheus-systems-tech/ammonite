/*
 * SPDX-FileCopyrightText: 2005 Alexander Larsson <alexl@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

#define AMMONITE_TYPE_PATH_BAR (ammonite_path_bar_get_type ())
G_DECLARE_FINAL_TYPE (AmmonitePathBar, ammonite_path_bar, AMMONITE, PATH_BAR, GtkBox)

void     ammonite_path_bar_set_path                       (AmmonitePathBar *path_bar,
                                                           GFile           *file);

void     ammonite_path_bar_set_extensions_background_menu (AmmonitePathBar *path_bar,
                                                           GMenuModel      *menu);
void     ammonite_path_bar_set_templates_menu             (AmmonitePathBar *path_bar,
                                                           GMenuModel      *menu);
void     ammonite_path_bar_show_current_location_menu     (AmmonitePathBar *path_bar);
