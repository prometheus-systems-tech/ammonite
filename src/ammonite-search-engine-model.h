/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include "ammonite-search-provider.h"

#include <glib-object.h>

#define AMMONITE_TYPE_SEARCH_ENGINE_MODEL (ammonite_search_engine_model_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteSearchEngineModel, ammonite_search_engine_model,
                      AMMONITE, SEARCH_ENGINE_MODEL, AmmoniteSearchProvider);

AmmoniteSearchEngineModel* ammonite_search_engine_model_new       (void);
