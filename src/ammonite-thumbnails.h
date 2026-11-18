/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

#pragma once

#include <gdk/gdk.h>

guint      ammonite_thumbnail_get_max_size          (void);

void       thumbnail_load_from_stream_async         (GInputStream        *stream,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);

GdkTexture *thumbnail_load_from_stream_finish       (GAsyncResult  *result,
                                                     time_t        *mtime,
                                                     GError       **error);

/* Returns NULL if there's no thumbnail yet. */
void       ammonite_create_thumbnail_async          (const gchar         *uri,
                                                     const gchar         *mime_type,
                                                     time_t               modified_time,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);
GdkPixbuf *ammonite_create_thumbnail_finish         (GAsyncResult  *res,
                                                     GError       **error);
gboolean   ammonite_can_thumbnail                   (const gchar *uri,
                                                     const gchar *mime_type,
                                                     time_t       modified_time);
gboolean   ammonite_thumbnail_is_mimetype_limited_by_size
						    (const char *mime_type);
char *     ammonite_thumbnail_get_path_for_uri      (const char *uri);

