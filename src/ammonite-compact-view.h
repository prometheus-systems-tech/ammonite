/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-list-base.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_COMPACT_VIEW (ammonite_compact_view_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteCompactView, ammonite_compact_view, AMMONITE, COMPACT_VIEW, AmmoniteListBase)

AmmoniteCompactView *ammonite_compact_view_new (void);

G_END_DECLS
