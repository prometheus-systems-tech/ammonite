/*
 * SPDX-FileCopyrightText: 2019 Carlos Soriano <csoriano@redhat.com>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#include <gio/gio.h>
#include <tinysparql.h>

TrackerSparqlConnection * ammonite_localsearch_get_miner_fs_connection (GError **error);
void ammonite_localsearch_setup_miner_fs_connection (void);

/* ammonite_localsearch_setup_host_miner_fs_connection_sync() is for testing purposes only */
void ammonite_localsearch_setup_host_miner_fs_connection_sync (void);

gboolean ammonite_localsearch_directory_is_tracked (GFile *directory);
gboolean ammonite_localsearch_directory_is_single (GFile *directory);
