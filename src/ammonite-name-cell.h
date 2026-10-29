/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-view-cell.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_NAME_CELL (ammonite_name_cell_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteNameCell, ammonite_name_cell, AMMONITE, NAME_CELL, AmmoniteViewCell)

AmmoniteViewCell * ammonite_name_cell_new (AmmoniteListBase *view);
void ammonite_name_cell_set_path (AmmoniteNameCell *self,
                                  GQuark            path_attribute_q,
                                  GFile            *base_location);
void ammonite_name_cell_set_show_snippet (AmmoniteNameCell *self,
                                          gboolean          show);
GtkTreeExpander * ammonite_name_cell_get_expander (AmmoniteNameCell *self);
GtkWidget * ammonite_name_cell_get_content (AmmoniteNameCell *self);

G_END_DECLS
