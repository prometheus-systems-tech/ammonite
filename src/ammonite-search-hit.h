/*
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once


#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_SEARCH_HIT (ammonite_search_hit_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteSearchHit, ammonite_search_hit, AMMONITE, SEARCH_HIT, GObject);

AmmoniteSearchHit * ammonite_search_hit_new                   (const char        *uri);

void                ammonite_search_hit_set_fts_rank          (AmmoniteSearchHit *hit,
							       gdouble            fts_rank);
void                ammonite_search_hit_set_modification_time (AmmoniteSearchHit *hit,
							       GDateTime         *date);
void                ammonite_search_hit_set_access_time       (AmmoniteSearchHit *hit,
							       GDateTime         *date);
void                ammonite_search_hit_set_creation_time     (AmmoniteSearchHit *hit,
							       GDateTime         *date);
void                ammonite_search_hit_set_fts_snippet       (AmmoniteSearchHit *hit,
                                                               const gchar       *snippet);
void                ammonite_search_hit_compute_scores        (AmmoniteSearchHit *hit,
                                                               GDateTime         *now,
                                                               GFile             *query_location);

const char *        ammonite_search_hit_get_uri               (AmmoniteSearchHit *hit);
gdouble             ammonite_search_hit_get_relevance         (AmmoniteSearchHit *hit);
const gchar *       ammonite_search_hit_get_fts_snippet       (AmmoniteSearchHit *hit);

G_END_DECLS
