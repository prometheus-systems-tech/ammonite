/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/* Needed for AmmoniteColumn (typedef only). */
#include "ammonite-types.h"

#include "ammonite-view-cell.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_LABEL_CELL (ammonite_label_cell_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteLabelCell, ammonite_label_cell, AMMONITE, LABEL_CELL, AmmoniteViewCell)

AmmoniteViewCell * ammonite_label_cell_new (AmmoniteListBase *view,
                                            AmmoniteColumn   *column);

G_END_DECLS
