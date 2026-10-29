/*
 * SPDX-FileCopyrightText: 2018 Canonical Ltd.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */

#pragma once

#include "ammonite-search-provider.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_SEARCH_ENGINE_RECENT (ammonite_search_engine_recent_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteSearchEngineRecent, ammonite_search_engine_recent,
                      AMMONITE, SEARCH_ENGINE_RECENT, AmmoniteSearchProvider);

AmmoniteSearchEngineRecent *ammonite_search_engine_recent_new (void);

G_END_DECLS
