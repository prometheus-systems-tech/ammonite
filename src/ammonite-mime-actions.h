
/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Maciej Stachowiak <mjs@eazel.com>
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "ammonite-types.h"

AmmoniteAttributes     ammonite_mime_actions_get_required_attributes      (void);

GAppInfo *             ammonite_mime_get_default_application_for_file     (AmmoniteFile            *file);

GAppInfo *             ammonite_mime_get_default_application_for_files    (GList                   *files);

gboolean               ammonite_mime_is_video                             (const char              *content_type);
gboolean               ammonite_mime_file_extracts                        (AmmoniteFile            *file);
gboolean               ammonite_mime_file_opens_in_external_app           (AmmoniteFile            *file);
gboolean               ammonite_mime_file_launches                        (AmmoniteFile            *file);
void                   ammonite_mime_activate_files                       (GtkWindow               *parent_window,
									   AmmoniteWindowSlot *slot,
									   GList              *files,
									   const char         *launch_directory,
									   AmmoniteOpenFlags   flags,
									   gboolean            user_confirmation);
GPtrArray*             ammonite_mime_types_group_get_mimetypes            (guint                    group_index);
