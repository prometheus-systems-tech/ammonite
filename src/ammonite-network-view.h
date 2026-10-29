/*
 * SPDX-FileCopyrightText: 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-list-base.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_NETWORK_VIEW (ammonite_network_view_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteNetworkView, ammonite_network_view, AMMONITE, NETWORK_VIEW, AmmoniteListBase)

AmmoniteNetworkView *ammonite_network_view_new (void);

G_END_DECLS
