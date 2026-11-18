/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_NETWORK_ADDRESS_BAR (ammonite_network_address_bar_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteNetworkAddressBar, ammonite_network_address_bar, AMMONITE, NETWORK_ADDRESS_BAR, GtkBox)

AmmoniteNetworkAddressBar * ammonite_network_address_bar_new (void);

G_END_DECLS
