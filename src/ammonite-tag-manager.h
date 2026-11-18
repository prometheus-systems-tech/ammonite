/*
 * SPDX-FileCopyrightText: 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_TAG_MANAGER (ammonite_tag_manager_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteTagManager, ammonite_tag_manager, AMMONITE, TAG_MANAGER, GObject);

AmmoniteTagManager* ammonite_tag_manager_new                (void);
AmmoniteTagManager* ammonite_tag_manager_new_dummy          (void);
AmmoniteTagManager* ammonite_tag_manager_get                (void);

GList*              ammonite_tag_manager_get_starred_files (AmmoniteTagManager *self);

void                ammonite_tag_manager_announce_unstarred_cb (GObject      *object,
                                                                GAsyncResult *result,
                                                                gpointer      user_data);
void                ammonite_tag_manager_announce_starred_cb   (GObject      *object,
                                                                GAsyncResult *result,
                                                                gpointer      user_data);

void                ammonite_tag_manager_star_files         (AmmoniteTagManager  *self,
                                                             GObject             *object,
                                                             GList               *selection,
                                                             GAsyncReadyCallback  callback,
                                                             gpointer             user_data,
                                                             GCancellable        *cancellable);

void                ammonite_tag_manager_unstar_files       (AmmoniteTagManager  *self,
                                                             GObject             *object,
                                                             GList               *selection,
                                                             GAsyncReadyCallback  callback,
                                                             gpointer             user_data,
                                                             GCancellable        *cancellable);


gboolean            ammonite_tag_manager_file_is_starred   (AmmoniteTagManager *self,
                                                            const gchar        *file_uri);

gboolean            ammonite_tag_manager_can_star_contents (AmmoniteTagManager *self,
                                                            GFile              *directory);
gboolean            ammonite_tag_manager_can_star_location (AmmoniteTagManager *self,
                                                            GFile              *directory);

void                ammonite_tag_manager_update_moved_uris  (AmmoniteTagManager *tag_manager,
                                                             GFile              *src,
                                                             GFile              *dest);

G_END_DECLS
