
/*
 * ammonite-bookmark.h - implementation of individual bookmarks.
 *
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2011, Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_BOOKMARK ammonite_bookmark_get_type()

G_DECLARE_FINAL_TYPE (AmmoniteBookmark, ammonite_bookmark, AMMONITE, BOOKMARK, GObject)

AmmoniteBookmark *    ammonite_bookmark_new                    (GFile *location,
                                                                const char *custom_name);
const char *          ammonite_bookmark_get_name               (AmmoniteBookmark      *bookmark);
GFile *               ammonite_bookmark_get_location           (AmmoniteBookmark      *bookmark);
char *                ammonite_bookmark_get_uri                (AmmoniteBookmark      *bookmark);
GIcon *               ammonite_bookmark_get_icon               (AmmoniteBookmark      *bookmark);
GIcon *               ammonite_bookmark_get_symbolic_icon      (AmmoniteBookmark      *bookmark);

void                  ammonite_bookmark_take_selected_uris     (AmmoniteBookmark      *bookmark,
								GStrv                  selected_uris);
GStrv                 ammonite_bookmark_get_selected_uris      (AmmoniteBookmark      *bookmark);
void                  ammonite_bookmark_set_name               (AmmoniteBookmark      *bookmark,
                                                                const char            *new_name);

G_END_DECLS
