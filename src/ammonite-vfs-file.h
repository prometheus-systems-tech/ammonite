/*
 * ammonite-vfs-file.h: Subclass of AmmoniteFile to implement the
 * the case of a VFS file.
 *
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include "ammonite-file.h"

#define AMMONITE_TYPE_VFS_FILE ammonite_vfs_file_get_type()
#define AMMONITE_VFS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), AMMONITE_TYPE_VFS_FILE, AmmoniteVFSFile))
#define AMMONITE_VFS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), AMMONITE_TYPE_VFS_FILE, AmmoniteVFSFileClass))
#define AMMONITE_IS_VFS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AMMONITE_TYPE_VFS_FILE))
#define AMMONITE_IS_VFS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), AMMONITE_TYPE_VFS_FILE))
#define AMMONITE_VFS_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), AMMONITE_TYPE_VFS_FILE, AmmoniteVFSFileClass))

typedef struct AmmoniteVFSFileDetails AmmoniteVFSFileDetails;

typedef struct {
	AmmoniteFile parent_slot;
} AmmoniteVFSFile;

typedef struct {
	AmmoniteFileClass parent_slot;
} AmmoniteVFSFileClass;

GType   ammonite_vfs_file_get_type (void);
