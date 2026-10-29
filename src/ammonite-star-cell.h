/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-view-cell.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_STAR_CELL (ammonite_star_cell_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteStarCell, ammonite_star_cell, AMMONITE, STAR_CELL, AmmoniteViewCell)

AmmoniteViewCell * ammonite_star_cell_new (AmmoniteListBase *view);

G_END_DECLS
