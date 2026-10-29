/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-list-base.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_GRID_VIEW (ammonite_grid_view_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteGridView, ammonite_grid_view, AMMONITE, GRID_VIEW, AmmoniteListBase)

AmmoniteGridView *ammonite_grid_view_new (void);

G_END_DECLS
