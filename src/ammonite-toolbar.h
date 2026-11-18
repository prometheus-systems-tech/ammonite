
/*
 * SPDX-FileCopyrightText: 2011, Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

#include "ammonite-types.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_TOOLBAR ammonite_toolbar_get_type()

G_DECLARE_FINAL_TYPE (AmmoniteToolbar, ammonite_toolbar, AMMONITE, TOOLBAR, AdwBin)

GtkWidget *ammonite_toolbar_new (void);

void       ammonite_toolbar_show_current_location_menu (AmmoniteToolbar *self);

void ammonite_toolbar_set_window_slot (AmmoniteToolbar    *self,
                                       AmmoniteWindowSlot *window_slot);
G_END_DECLS
