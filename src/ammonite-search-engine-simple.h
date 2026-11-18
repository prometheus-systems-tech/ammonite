/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "ammonite-search-provider.h"

#include <glib-object.h>

#pragma once

G_BEGIN_DECLS

#define AMMONITE_TYPE_SEARCH_ENGINE_SIMPLE (ammonite_search_engine_simple_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteSearchEngineSimple, ammonite_search_engine_simple,
                      AMMONITE, SEARCH_ENGINE_SIMPLE, AmmoniteSearchProvider);

AmmoniteSearchEngineSimple* ammonite_search_engine_simple_new (void);

G_END_DECLS
