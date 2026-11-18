/*
 * SPDX-FileCopyrightText: 2005 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 */

#pragma once

#include "ammonite-enums.h"

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#define AMMONITE_TYPE_QUERY		(ammonite_query_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteQuery, ammonite_query, AMMONITE, QUERY, GObject)

AmmoniteQuery* ammonite_query_new      (void);
AmmoniteQuery* ammonite_query_copy     (AmmoniteQuery *query);

char *         ammonite_query_get_text           (AmmoniteQuery *query);
gboolean       ammonite_query_set_text           (AmmoniteQuery *query, const char *text);

gboolean       ammonite_query_get_show_hidden_files (AmmoniteQuery *query);
void           ammonite_query_set_show_hidden_files (AmmoniteQuery *query, gboolean show_hidden);

GFile*         ammonite_query_get_location       (AmmoniteQuery *query);
void           ammonite_query_set_location       (AmmoniteQuery *query,
                                                  GFile         *location);

gboolean
ammonite_query_has_mime_types (AmmoniteQuery *self);
gboolean
ammonite_query_matches_mime_type (AmmoniteQuery *self,
                                  const char    *mime_type);
char *
ammonite_query_get_mime_type_str (AmmoniteQuery *query);
gboolean       ammonite_query_set_mime_types     (AmmoniteQuery *query, GPtrArray *mime_types);

gboolean
ammonite_query_can_search_content (AmmoniteQuery *self);
gboolean
ammonite_query_get_search_content (AmmoniteQuery *query);
gboolean
ammonite_query_update_search_content (AmmoniteQuery *self);

AmmoniteSearchTimeType ammonite_query_get_search_type (AmmoniteQuery *query);
gboolean               ammonite_query_set_search_type (AmmoniteQuery           *query,
                                                       AmmoniteSearchTimeType   type);

GPtrArray*     ammonite_query_get_date_range     (AmmoniteQuery *query);
gboolean       ammonite_query_set_date_range     (AmmoniteQuery *query,
                                                  GPtrArray     *date_range);

gboolean
ammonite_query_recursive (AmmoniteQuery *self);
gboolean
ammonite_query_recursive_local_only (AmmoniteQuery *self);
gboolean
ammonite_query_update_recursive_setting (AmmoniteQuery *self);

gdouble        ammonite_query_matches_string     (AmmoniteQuery *query, const gchar *string);

gboolean       ammonite_query_has_active_filter  (AmmoniteQuery *query);
gboolean       ammonite_query_is_empty           (AmmoniteQuery *query);
gboolean       ammonite_query_is_global          (AmmoniteQuery *query);
