/*
 * SPDX-FileCopyrightText: 2024 António Fernandes <antoniof@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-view-cell.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_NETWORK_CELL (ammonite_network_cell_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteNetworkCell, ammonite_network_cell, AMMONITE, NETWORK_CELL, AmmoniteViewCell)

AmmoniteViewCell * ammonite_network_cell_new (AmmoniteListBase *view);

G_END_DECLS
