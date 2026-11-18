/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-view-cell.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_COMPACT_CELL (ammonite_compact_cell_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteCompactCell, ammonite_compact_cell, AMMONITE, COMPACT_CELL, AmmoniteViewCell)

AmmoniteCompactCell * ammonite_compact_cell_new (AmmoniteListBase *view);

G_END_DECLS
