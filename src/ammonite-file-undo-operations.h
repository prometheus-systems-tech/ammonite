
/*
 * SPDX-FileCopyrightText: 2007-2011 Amos Brocco
 * SPDX-FileCopyrightText: 2010 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include "ammonite-types.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gnome-autoar/gnome-autoar.h>

typedef enum
{
    AMMONITE_FILE_UNDO_OP_INVALID,
    AMMONITE_FILE_UNDO_OP_COPY,
    AMMONITE_FILE_UNDO_OP_DUPLICATE,
    AMMONITE_FILE_UNDO_OP_MOVE,
    AMMONITE_FILE_UNDO_OP_RENAME,
    AMMONITE_FILE_UNDO_OP_BATCH_RENAME,
    AMMONITE_FILE_UNDO_OP_STARRED,
    AMMONITE_FILE_UNDO_OP_CREATE_EMPTY_FILE,
    AMMONITE_FILE_UNDO_OP_CREATE_FILE_FROM_TEMPLATE,
    AMMONITE_FILE_UNDO_OP_CREATE_FOLDER,
    AMMONITE_FILE_UNDO_OP_EXTRACT,
    AMMONITE_FILE_UNDO_OP_COMPRESS,
    AMMONITE_FILE_UNDO_OP_MOVE_TO_TRASH,
    AMMONITE_FILE_UNDO_OP_RESTORE_FROM_TRASH,
    AMMONITE_FILE_UNDO_OP_CREATE_LINK,
    AMMONITE_FILE_UNDO_OP_RECURSIVE_SET_PERMISSIONS,
    AMMONITE_FILE_UNDO_OP_SET_PERMISSIONS,
    AMMONITE_FILE_UNDO_OP_CHANGE_GROUP,
    AMMONITE_FILE_UNDO_OP_CHANGE_OWNER,
    AMMONITE_FILE_UNDO_OP_NUM_TYPES,
} AmmoniteFileUndoOp;

#define AMMONITE_TYPE_FILE_UNDO_INFO ammonite_file_undo_info_get_type ()
G_DECLARE_DERIVABLE_TYPE (AmmoniteFileUndoInfo, ammonite_file_undo_info,
                          AMMONITE, FILE_UNDO_INFO,
                          GObject)

struct _AmmoniteFileUndoInfoClass
{
    GObjectClass parent_class;

    void (* undo_func) (AmmoniteFileUndoInfo           *self,
                        GtkWindow                      *parent_window,
                        AmmoniteFileOperationsDBusData *dbus_data);
    void (* redo_func) (AmmoniteFileUndoInfo           *self,
                        GtkWindow                      *parent_window,
                        AmmoniteFileOperationsDBusData *dbus_data);

    gboolean (* is_noop_func) (AmmoniteFileUndoInfo *self);

    void (* strings_func) (AmmoniteFileUndoInfo *self,
                           gchar **undo_label,
                           gchar **undo_description,
                           gchar **redo_label,
                           gchar **redo_description);
};

void ammonite_file_undo_info_apply_async (AmmoniteFileUndoInfo           *self,
                                          gboolean                        undo,
                                          GtkWindow                      *parent_window,
                                          AmmoniteFileOperationsDBusData *dbus_data,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
gboolean ammonite_file_undo_info_apply_finish (AmmoniteFileUndoInfo *self,
                                               GAsyncResult *res,
                                               gboolean *user_cancel,
                                               GError **error);

void ammonite_file_undo_info_get_strings (AmmoniteFileUndoInfo *self,
                                          gchar **undo_label,
                                          gchar **undo_description,
                                          gchar **redo_label,
                                          gchar **redo_description);

gboolean ammonite_file_undo_info_is_noop (AmmoniteFileUndoInfo *self);

AmmoniteFileUndoOp ammonite_file_undo_info_get_op_type (AmmoniteFileUndoInfo *self);

/* copy/move/duplicate/link/restore from trash */
#define AMMONITE_TYPE_FILE_UNDO_INFO_EXT ammonite_file_undo_info_ext_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoExt, ammonite_file_undo_info_ext,
                      AMMONITE, FILE_UNDO_INFO_EXT,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_ext_new (AmmoniteFileUndoOp op_type,
                                                       gint item_count,
                                                       GFile *src_dir,
                                                       GFile *target_dir);
void ammonite_file_undo_info_ext_add_origin_target_pair (AmmoniteFileUndoInfoExt *self,
                                                         GFile                   *origin,
                                                         GFile                   *target);

/* create new file/folder */
#define AMMONITE_TYPE_FILE_UNDO_INFO_CREATE ammonite_file_undo_info_create_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoCreate, ammonite_file_undo_info_create,
                      AMMONITE, FILE_UNDO_INFO_CREATE,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_create_new (AmmoniteFileUndoOp op_type);
void ammonite_file_undo_info_create_set_data (AmmoniteFileUndoInfoCreate *self,
                                              GFile                      *file,
                                              const void                 *template,
                                              gsize                       length);

/* rename */
#define AMMONITE_TYPE_FILE_UNDO_INFO_RENAME ammonite_file_undo_info_rename_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoRename, ammonite_file_undo_info_rename,
                      AMMONITE, FILE_UNDO_INFO_RENAME,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_rename_new (void);
void ammonite_file_undo_info_rename_set_data_pre (AmmoniteFileUndoInfoRename *self,
                                                  GFile                      *old_file,
                                                  const char                 *old_display_name,
                                                  const char                 *new_display_name);
void ammonite_file_undo_info_rename_set_data_post (AmmoniteFileUndoInfoRename *self,
                                                   GFile                      *new_file);

/* batch rename */
#define AMMONITE_TYPE_FILE_UNDO_INFO_BATCH_RENAME ammonite_file_undo_info_batch_rename_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoBatchRename, ammonite_file_undo_info_batch_rename,
                      AMMONITE, FILE_UNDO_INFO_BATCH_RENAME,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_batch_rename_new (gint item_count);
void ammonite_file_undo_info_batch_rename_set_data_pre (AmmoniteFileUndoInfoBatchRename *self,
                                                        GList                           *old_files);
void ammonite_file_undo_info_batch_rename_set_data_post (AmmoniteFileUndoInfoBatchRename *self,
                                                         GList                           *new_files);

/* starred files */
#define AMMONITE_TYPE_FILE_UNDO_INFO_STARRED         (ammonite_file_undo_info_starred_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoStarred, ammonite_file_undo_info_starred,
                      AMMONITE, FILE_UNDO_INFO_STARRED,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_starred_new (GList   *files,
                                                           gboolean starred);
GList *ammonite_file_undo_info_starred_get_files (AmmoniteFileUndoInfoStarred *self);
gboolean ammonite_file_undo_info_starred_is_starred (AmmoniteFileUndoInfoStarred *self);

/* trash */
#define AMMONITE_TYPE_FILE_UNDO_INFO_TRASH         (ammonite_file_undo_info_trash_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoTrash, ammonite_file_undo_info_trash,
                      AMMONITE, FILE_UNDO_INFO_TRASH,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_trash_new (gint item_count);
void ammonite_file_undo_info_trash_add_file (AmmoniteFileUndoInfoTrash *self,
                                             GFile                     *file);
GList *ammonite_file_undo_info_trash_get_files (AmmoniteFileUndoInfoTrash *self);

/* recursive permissions */
#define AMMONITE_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS ammonite_file_undo_info_rec_permissions_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoRecPermissions, ammonite_file_undo_info_rec_permissions,
                      AMMONITE, FILE_UNDO_INFO_REC_PERMISSIONS,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_rec_permissions_new (GFile   *dest,
                                                                   guint32 file_permissions,
                                                                   guint32 file_mask,
                                                                   guint32 dir_permissions,
                                                                   guint32 dir_mask);
void ammonite_file_undo_info_rec_permissions_add_file (AmmoniteFileUndoInfoRecPermissions *self,
                                                       GFile                              *file,
                                                       guint32                             permission);

/* single file change permissions */
#define AMMONITE_TYPE_FILE_UNDO_INFO_PERMISSIONS ammonite_file_undo_info_permissions_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoPermissions, ammonite_file_undo_info_permissions,
                      AMMONITE, FILE_UNDO_INFO_PERMISSIONS,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_permissions_new (GFile   *file,
                                                               guint32  current_permissions,
                                                               guint32  new_permissions);

/* group and owner change */
#define AMMONITE_TYPE_FILE_UNDO_INFO_OWNERSHIP ammonite_file_undo_info_ownership_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoOwnership, ammonite_file_undo_info_ownership,
                      AMMONITE, FILE_UNDO_INFO_OWNERSHIP,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo *ammonite_file_undo_info_ownership_new (AmmoniteFileUndoOp  op_type,
                                                             GFile              *file,
                                                             const char         *current_data,
                                                             const char         *new_data);

/* extract */
#define AMMONITE_TYPE_FILE_UNDO_INFO_EXTRACT ammonite_file_undo_info_extract_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoExtract, ammonite_file_undo_info_extract,
                      AMMONITE, FILE_UNDO_INFO_EXTRACT,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo * ammonite_file_undo_info_extract_new (GList *sources,
                                                            GFile *destination_directory);
void ammonite_file_undo_info_extract_set_outputs (AmmoniteFileUndoInfoExtract *self,
                                                  GList                       *outputs);

/* compress */
#define AMMONITE_TYPE_FILE_UNDO_INFO_COMPRESS ammonite_file_undo_info_compress_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFileUndoInfoCompress, ammonite_file_undo_info_compress,
                      AMMONITE, FILE_UNDO_INFO_COMPRESS,
                      AmmoniteFileUndoInfo)

AmmoniteFileUndoInfo * ammonite_file_undo_info_compress_new (GList        *sources,
                                                             GFile        *output,
                                                             AutoarFormat  format,
                                                             AutoarFilter  filter,
                                                             const gchar  *passphrase);
