/*
 * SPDX-FileCopyrightText: 2005 Mr Jamie McCracken
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Jamie McCracken (jamiemcc@gnome.org)
 */

#pragma once

#include "ammonite-search-provider.h"

#include <glib-object.h>

#define AMMONITE_TYPE_SEARCH_ENGINE_LOCALSEARCH (ammonite_search_engine_localsearch_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteSearchEngineLocalsearch, ammonite_search_engine_localsearch,
                      AMMONITE, SEARCH_ENGINE_LOCALSEARCH, AmmoniteSearchProvider)

AmmoniteSearchEngineLocalsearch* ammonite_search_engine_localsearch_new (void);
