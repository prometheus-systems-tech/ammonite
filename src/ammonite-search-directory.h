/*
 * ammonite-search-directory.h: Subclass of AmmoniteDirectory to implement
 * a virtual directory consisting of the search directory and the search icons
 *
 * SPDX-FileCopyrightText: 2005 Novell, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ammonite-directory.h"

#include "ammonite-types.h"

#define AMMONITE_SEARCH_DIRECTORY_PROVIDER_NAME "search-directory-provider"
#define AMMONITE_TYPE_SEARCH_DIRECTORY (ammonite_search_directory_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteSearchDirectory, ammonite_search_directory,
                      AMMONITE, SEARCH_DIRECTORY, AmmoniteDirectory)

char   *ammonite_search_directory_generate_new_uri     (void);

GFile *
ammonite_search_directory_get_search_location (AmmoniteSearchDirectory *self);

AmmoniteQuery *ammonite_search_directory_get_query       (AmmoniteSearchDirectory *self);
void           ammonite_search_directory_set_query       (AmmoniteSearchDirectory *self,
							  AmmoniteQuery           *query);
