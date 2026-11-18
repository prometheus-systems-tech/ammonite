/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include <gtk/gtk.h>

/*
 * Private header to be included only by subclasses.
 */

G_BEGIN_DECLS

/* Methods */
void               ammonite_list_base_activate_selection (AmmoniteListBase *self,
                                                          gboolean          open_in_new_tab);
AmmoniteFile      *ammonite_list_base_get_directory_as_file (AmmoniteListBase *self);
AmmoniteViewModel *ammonite_list_base_get_model     (AmmoniteListBase *self);
GtkWidget         *ammonite_list_base_get_scrolled_window (AmmoniteListBase *self);
void               ammonite_list_base_setup_gestures (AmmoniteListBase *self);

/* Shareable helpers */
void                          setup_cell_common                 (GObject          *listitem,
                                                                 AmmoniteViewCell *cell,
                                                                 GtkWidget        *hover_target);

void                          setup_cell_controllers            (AmmoniteViewCell *cell,
                                                                 GtkWidget        *hover_target);

G_END_DECLS
