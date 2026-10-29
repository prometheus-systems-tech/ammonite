
/*
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* ammonite-bookmark-list.h - interface for centralized list of bookmarks.
 */

#pragma once

#include "ammonite-types.h"

#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_BOOKMARK_LIST (ammonite_bookmark_list_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteBookmarkList, ammonite_bookmark_list, AMMONITE, BOOKMARK_LIST, GObject)

AmmoniteBookmarkList *
ammonite_bookmark_list_new (void);

gboolean
ammonite_bookmark_list_contains (AmmoniteBookmarkList *bookmarks,
                                 GFile                *location);
gboolean
ammonite_bookmark_list_can_bookmark (AmmoniteBookmarkList *list,
                                     GFile                *location);
AmmoniteBookmark *
ammonite_bookmark_list_get_bookmark (AmmoniteBookmarkList *bookmarks,
                                     GFile                *location);
GList *
ammonite_bookmark_list_get_all (AmmoniteBookmarkList *bookmarks);

void
ammonite_bookmark_list_add (AmmoniteBookmarkList *bookmarks,
                            GFile                *location,
                            int                   index);
void
ammonite_bookmark_list_move_item (AmmoniteBookmarkList *bookmarks,
                                  GFile                *location,
                                  guint                 destination);
void
ammonite_bookmark_list_remove (AmmoniteBookmarkList *bookmarks,
                               GFile                *location);

G_END_DECLS
