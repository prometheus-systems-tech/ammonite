/*
 * ammonite-vfs-directory.h: Subclass of AmmoniteDirectory to implement the
 * the case of a VFS directory.
 *
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include "ammonite-directory.h"

#define AMMONITE_TYPE_VFS_DIRECTORY ammonite_vfs_directory_get_type()
#define AMMONITE_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), AMMONITE_TYPE_VFS_DIRECTORY, AmmoniteVFSDirectory))
#define AMMONITE_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), AMMONITE_TYPE_VFS_DIRECTORY, AmmoniteVFSDirectoryClass))
#define AMMONITE_IS_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AMMONITE_TYPE_VFS_DIRECTORY))
#define AMMONITE_IS_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), AMMONITE_TYPE_VFS_DIRECTORY))
#define AMMONITE_VFS_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), AMMONITE_TYPE_VFS_DIRECTORY, AmmoniteVFSDirectoryClass))

typedef struct AmmoniteVFSDirectoryDetails AmmoniteVFSDirectoryDetails;

typedef struct {
	AmmoniteDirectory parent_slot;
} AmmoniteVFSDirectory;

typedef struct {
	AmmoniteDirectoryClass parent_slot;
} AmmoniteVFSDirectoryClass;

GType   ammonite_vfs_directory_get_type (void);
