/*
 * SPDX-FileCopyrightText: 1999, 2000 Free Software Foundaton
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Ettore Perazzoli
 *          Darin Adler <darin@bentspoon.com>
 *          John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#pragma once

#include "ammonite-types.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_FILES_VIEW ammonite_files_view_get_type()
G_DECLARE_FINAL_TYPE (AmmoniteFilesView, ammonite_files_view, AMMONITE, FILES_VIEW, AdwBin)

AmmoniteFilesView *      ammonite_files_view_new                         (guint               id,
                                                                          AmmoniteWindowSlot *slot);

guint
ammonite_files_view_get_view_id (AmmoniteFilesView *self);
void                ammonite_files_view_change                           (AmmoniteFilesView  *self,
                                                                          guint               id);

const char *
ammonite_files_view_get_toggle_icon_name (AmmoniteFilesView *self);
const char *
ammonite_files_view_get_toggle_tooltip (AmmoniteFilesView  *self,
                                        const char        **description);

GFile *
ammonite_files_view_get_location (AmmoniteFilesView *self);
void
ammonite_files_view_set_location (AmmoniteFilesView *self,
                                  GFile             *location);

AmmoniteQuery *
ammonite_files_view_get_search_query (AmmoniteFilesView *self);
void
ammonite_files_view_set_search_query (AmmoniteFilesView *self,
                                      AmmoniteQuery     *query);

AmmoniteFileList *
ammonite_files_view_get_selection (AmmoniteFilesView *self);
AmmoniteSelectionSource
ammonite_files_view_get_selection_source (AmmoniteFilesView *self);
void
ammonite_files_view_set_selection (AmmoniteFilesView       *self,
                                   AmmoniteFileList        *selection,
                                   AmmoniteSelectionSource  selection_source);

gboolean
ammonite_files_view_is_loading (AmmoniteFilesView *self);
gboolean
ammonite_files_view_is_searching (AmmoniteFilesView *self);

/* Wrappers for signal emitters. These are normally called
 * only by AmmoniteFilesView itself. They have corresponding signals
 * that observers might want to connect with.
 */
gboolean            ammonite_files_view_get_loading                      (AmmoniteFilesView *view);

/* Hooks for subclasses to call. These are normally called only by
 * AmmoniteFilesView and its subclasses
 */
void                ammonite_files_view_activate_file                    (AmmoniteFilesView *view,
                                                                          AmmoniteFile      *file,
                                                                          AmmoniteOpenFlags  flags);

gboolean            ammonite_files_view_has_subdirectory                (AmmoniteFilesView *view,
                                                                         AmmoniteDirectory *directory);
void                ammonite_files_view_add_subdirectory                (AmmoniteFilesView *view,
                                                                         AmmoniteDirectory *directory);
void                ammonite_files_view_remove_subdirectory             (AmmoniteFilesView *view,
                                                                         AmmoniteDirectory *directory);

/* file operations */
char *            ammonite_files_view_get_backing_uri            (AmmoniteFilesView      *view);
void              ammonite_files_view_move_copy_items            (AmmoniteFilesView      *view,
                                                                  const GList            *item_uris,
                                                                  const char             *target_uri,
                                                                  int                     copy_action);
void              ammonite_file_view_save_image_from_texture    (AmmoniteFilesView       *view,
                                                                 GdkTexture              *texture,
                                                                 const char              *target_uri,
                                                                 const char              *base_name);
void              ammonite_files_view_new_file_with_initial_contents (AmmoniteFilesView  *view,
                                                                      const char         *parent_uri,
                                                                      const char         *filename,
                                                                      const void         *initial_contents,
                                                                      gsize               length);
/* selection handling */
void              ammonite_files_view_activate_selection         (AmmoniteFilesView      *view,
                                                                  AmmoniteOpenFlags       flags);
void              ammonite_files_view_preview_selection_event    (AmmoniteFilesView      *view,
                                                                  GtkDirectionType        direction);
void              ammonite_files_view_stop_loading               (AmmoniteFilesView      *view);

/* testing-only */
AmmoniteViewModel *
ammonite_files_view_get_private_model (AmmoniteFilesView *self);
GActionGroup *
ammonite_files_view_get_private_action_group (AmmoniteFilesView *self);
AmmoniteListBase *
ammonite_files_view_get_private_list_base (AmmoniteFilesView *self);

G_END_DECLS
