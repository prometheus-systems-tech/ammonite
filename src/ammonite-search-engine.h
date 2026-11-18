/*
 * SPDX-FileCopyrightText: 2005 Novell, Inc.
 * SPDX-FileCopyrightText: 2025 The Files contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */

#pragma once

#include "ammonite-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
    AMMONITE_SEARCH_TYPE_LOCALSEARCH = 1 << 0,
    AMMONITE_SEARCH_TYPE_MODEL       = 1 << 1,
    AMMONITE_SEARCH_TYPE_RECENT      = 1 << 2,
    AMMONITE_SEARCH_TYPE_SIMPLE      = 1 << 3,

    AMMONITE_SEARCH_TYPE_FOLDER = AMMONITE_SEARCH_TYPE_LOCALSEARCH |
                                  AMMONITE_SEARCH_TYPE_MODEL |
                                  AMMONITE_SEARCH_TYPE_SIMPLE,
    /* This is used for both "Search Everywhere" and shell search provider. */
    AMMONITE_SEARCH_TYPE_GLOBAL = AMMONITE_SEARCH_TYPE_LOCALSEARCH |
                                  AMMONITE_SEARCH_TYPE_RECENT,

    AMMONITE_SEARCH_TYPE_ALL    = AMMONITE_SEARCH_TYPE_FOLDER | AMMONITE_SEARCH_TYPE_GLOBAL,
} AmmoniteSearchType;

#define AMMONITE_TYPE_SEARCH_ENGINE		(ammonite_search_engine_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteSearchEngine, ammonite_search_engine, AMMONITE, SEARCH_ENGINE, GObject)

AmmoniteSearchEngine *
ammonite_search_engine_new (AmmoniteSearchType search_type);

void
ammonite_search_engine_set_search_type (AmmoniteSearchEngine *self,
                                        AmmoniteSearchType search_type);

void
ammonite_search_engine_start (AmmoniteSearchEngine *self,
                              AmmoniteQuery        *query);
void
ammonite_search_engine_stop (AmmoniteSearchEngine *self);

G_END_DECLS