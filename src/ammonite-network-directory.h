/*
 * SPDX-FileCopyrightText: 2024 António Fernandes <antoniof@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#include "ammonite-directory.h"

G_BEGIN_DECLS

#define AMMONITE_NETWORK_DIRECTORY_PROVIDER_NAME "network-directory-provider"

#define AMMONITE_TYPE_NETWORK_DIRECTORY (ammonite_network_directory_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteNetworkDirectory, ammonite_network_directory, AMMONITE, NETWORK_DIRECTORY, AmmoniteDirectory);

AmmoniteNetworkDirectory* ammonite_network_directory_new      (void);

G_END_DECLS
