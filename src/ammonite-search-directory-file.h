/*
 * SPDX-FileCopyrightText: 2003 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include "ammonite-file.h"

#define AMMONITE_TYPE_SEARCH_DIRECTORY_FILE ammonite_search_directory_file_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteSearchDirectoryFile, ammonite_search_directory_file,
                      AMMONITE, SEARCH_DIRECTORY_FILE,
                      AmmoniteFile)

void ammonite_search_directory_file_update_display_name (AmmoniteSearchDirectoryFile *search_file);
