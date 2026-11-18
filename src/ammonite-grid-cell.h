/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-view-cell.h"

G_BEGIN_DECLS

enum
{
    AMMONITE_GRID_CELL_FIRST_CAPTION,
    AMMONITE_GRID_CELL_SECOND_CAPTION,
    AMMONITE_GRID_CELL_THIRD_CAPTION,
    AMMONITE_GRID_CELL_N_CAPTIONS
};

#define AMMONITE_TYPE_GRID_CELL (ammonite_grid_cell_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteGridCell, ammonite_grid_cell, AMMONITE, GRID_CELL, AmmoniteViewCell)

AmmoniteGridCell * ammonite_grid_cell_new (AmmoniteListBase *view,
                                           GQuark           *attrs);

G_END_DECLS
