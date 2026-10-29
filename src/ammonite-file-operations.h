
/*
 * SPDX-FileCopyrightText: 1999, 2000 Free Software Foundation
 * SPDX-FileCopyrightText: 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Ettore Perazzoli <ettore@gnu.org>,
 *          Pavel Cisler <pavel@eazel.com>
 */

#pragma once

#include "ammonite-types.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gnome-autoar/gnome-autoar.h>

#define SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE 1

typedef void (* AmmoniteCopyCallback)      (GHashTable *debuting_uris,
					    gboolean    success,
					    gpointer    callback_data);
typedef void (* AmmoniteCreateCallback)    (GFile      *new_file,
					    gboolean    success,
					    gpointer    callback_data);
typedef void (* AmmoniteOpCallback)        (gboolean    success,
					    gpointer    callback_data);
typedef void (* AmmoniteOpRenameCallback)  (GFile      *renamed_file,
					    gboolean    success,
					    gpointer    callback_data);
typedef void (* AmmoniteDeleteCallback)    (GHashTable *debuting_uris,
					    gboolean    user_cancel,
					    gpointer    callback_data);
typedef void (* AmmoniteUnmountCallback)   (gpointer    callback_data);
typedef void (* AmmoniteExtractCallback)   (GList    *outputs,
                                            gpointer  callback_data);

void ammonite_file_operations_copy_move   (const GList                    *item_uris,
                                           const char                     *target_dir_uri,
                                           GdkDragAction                   copy_action,
                                           GtkWidget                      *parent_view,
                                           AmmoniteFileOperationsDBusData *dbus_data,
                                           AmmoniteCopyCallback            done_callback,
                                           gpointer                        done_callback_data);
void ammonite_file_operations_empty_trash (GtkWidget                      *parent_view,
                                           gboolean                        ask_confirmation,
                                           AmmoniteFileOperationsDBusData *dbus_data);
void ammonite_file_operations_new_folder  (GtkWidget                      *parent_view,
                                           AmmoniteFileOperationsDBusData *dbus_data,
                                           const char                     *parent_dir_uri,
                                           const char                     *folder_name,
                                           AmmoniteCreateCallback          done_callback,
                                           gpointer                        done_callback_data);
void ammonite_file_operations_new_file    (GtkWidget                 *parent_view,
					   const char                *parent_dir,
					   const char                *target_filename,
					   const void                *initial_contents,
					   gsize                      length,
					   AmmoniteCreateCallback     done_callback,
					   gpointer                   data);
void ammonite_file_operations_new_file_from_template (GtkWidget               *parent_view,
						      const char              *parent_dir,
						      const char              *target_filename,
						      const char              *template_uri,
						      AmmoniteCreateCallback   done_callback,
						      gpointer                 data);

void ammonite_file_operations_trash_or_delete_async (GList                          *files,
                                                     GtkWindow                      *parent_window,
                                                     AmmoniteFileOperationsDBusData *dbus_data,
                                                     AmmoniteDeleteCallback          done_callback,
                                                     gpointer                        done_callback_data);
void ammonite_file_operations_delete_async (GList                          *files,
                                            GtkWindow                      *parent_window,
                                            AmmoniteFileOperationsDBusData *dbus_data,
                                            AmmoniteDeleteCallback          done_callback,
                                            gpointer                        done_callback_data);

void ammonite_file_set_permissions_recursive (const char                     *directory,
					      guint32                         file_permissions,
					      guint32                         file_mask,
					      guint32                         folder_permissions,
					      guint32                         folder_mask,
					      AmmoniteOpCallback              callback,
					      gpointer                        callback_data);

void ammonite_file_operations_unmount_mount_full (GtkWindow                 *parent_window,
						  GMount                    *mount,
						  GMountOperation           *mount_operation,
						  gboolean                   eject,
						  AmmoniteUnmountCallback    callback,
						  gpointer                   callback_data);

void ammonite_file_operations_copy_async (GList                          *files,
                                          GFile                          *target_dir,
                                          GtkWindow                      *parent_window,
                                          AmmoniteFileOperationsDBusData *dbus_data,
                                          AmmoniteCopyCallback            done_callback,
                                          gpointer                        done_callback_data);

void ammonite_file_operations_move_async (GList                          *files,
                                          GFile                          *target_dir,
                                          GtkWindow                      *parent_window,
                                          AmmoniteFileOperationsDBusData *dbus_data,
                                          AmmoniteCopyCallback            done_callback,
                                          gpointer                        done_callback_data);
void ammonite_file_operations_move_sync (GList                *files,
                                         GFile                *target_dir);

void ammonite_file_operations_duplicate (GList                          *files,
                                         GtkWindow                      *parent_window,
                                         AmmoniteFileOperationsDBusData *dbus_data,
                                         AmmoniteCopyCallback            done_callback,
                                         gpointer                        done_callback_data);
void ammonite_file_operations_link      (GList                          *files,
                                         GFile                          *target_dir,
                                         GtkWindow                      *parent_window,
                                         AmmoniteFileOperationsDBusData *dbus_data,
                                         AmmoniteCopyCallback            done_callback,
                                         gpointer                        done_callback_data);
void ammonite_file_operations_extract_files (GList                          *files,
                                             GFile                          *destination_directory,
                                             GtkWindow                      *parent_window,
                                             AmmoniteFileOperationsDBusData *dbus_data,
                                             AmmoniteExtractCallback         done_callback,
                                             gpointer                        done_callback_data);
void ammonite_file_operations_compress (GList                          *files,
                                        GFile                          *output,
                                        AutoarFormat                    format,
                                        AutoarFilter                    filter,
                                        const gchar                    *passphrase,
                                        GtkWindow                      *parent_window,
                                        AmmoniteFileOperationsDBusData *dbus_data,
                                        AmmoniteCreateCallback          done_callback,
                                        gpointer                        done_callback_data);

void
ammonite_file_operations_rename (GFile                          *location,
                                 const char                     *new_name,
                                 GtkWidget                      *parent_view,
                                 AmmoniteFileOperationsDBusData *dbus_data,
                                 AmmoniteOpRenameCallback        done_callback,
                                 gpointer                        done_callback_data);

void
ammonite_file_operations_paste_image_from_clipboard (GtkWidget                      *parent_view,
                                                    AmmoniteFileOperationsDBusData *dbus_data,
                                                    const char                     *parent_dir_uri,
                                                    AmmoniteCopyCallback            done_callback,
                                                    gpointer                        done_callback_data);

void
ammonite_file_operations_save_image_from_texture (GtkWidget                      *parent_view,
                                                  AmmoniteFileOperationsDBusData *dbus_data,
                                                  const char                     *parent_dir_uri,
                                                  const char                     *base_name,
                                                  GdkTexture                     *texture,
                                                  AmmoniteCopyCallback            done_callback,
                                                  gpointer                        done_callback_data);
