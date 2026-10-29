/*
 * ammonite-vfs-directory.c: Subclass of AmmoniteDirectory to help implement the
 * virtual trash directory.
 *
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "ammonite-vfs-directory.h"

#include "ammonite-directory-private.h"
#include "ammonite-file-private.h"

G_DEFINE_TYPE (AmmoniteVFSDirectory, ammonite_vfs_directory, AMMONITE_TYPE_DIRECTORY);

static void
ammonite_vfs_directory_init (AmmoniteVFSDirectory *directory)
{
}

static void
vfs_call_when_ready (AmmoniteDirectory         *directory,
                     AmmoniteAttributes         attributes,
                     AmmoniteDirectoryCallback  callback,
                     gpointer                   callback_data)
{
    g_assert (AMMONITE_IS_VFS_DIRECTORY (directory));

    ammonite_directory_call_when_ready_internal
        (directory,
        NULL,
        attributes,
        callback,
        NULL,
        callback_data);
}

static void
vfs_cancel_callback (AmmoniteDirectory         *directory,
                     AmmoniteDirectoryCallback  callback,
                     gpointer                   callback_data)
{
    g_assert (AMMONITE_IS_VFS_DIRECTORY (directory));

    ammonite_directory_cancel_callback_internal
        (directory,
        NULL,
        callback,
        NULL,
        callback_data);
}

static void
vfs_file_monitor_add (AmmoniteDirectory         *directory,
                      gconstpointer              client,
                      gboolean                   monitor_hidden_files,
                      AmmoniteAttributes         attributes,
                      AmmoniteDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    g_assert (AMMONITE_IS_VFS_DIRECTORY (directory));
    g_assert (client != NULL);

    ammonite_directory_monitor_add_internal
        (directory, NULL,
        client,
        monitor_hidden_files,
        attributes,
        callback, callback_data);
}

static void
vfs_file_monitor_remove (AmmoniteDirectory *directory,
                         gconstpointer      client)
{
    g_assert (AMMONITE_IS_VFS_DIRECTORY (directory));
    g_assert (client != NULL);

    ammonite_directory_monitor_remove_internal (directory, NULL, client);
}

static void
vfs_force_reload (AmmoniteDirectory *directory)
{
    AmmoniteAttributes all_attributes;

    g_assert (AMMONITE_IS_DIRECTORY (directory));

    all_attributes = ammonite_file_get_all_attributes ();
    ammonite_directory_force_reload_internal (directory,
                                              all_attributes);
}

static void
ammonite_vfs_directory_class_init (AmmoniteVFSDirectoryClass *klass)
{
    AmmoniteDirectoryClass *directory_class = AMMONITE_DIRECTORY_CLASS (klass);

    directory_class->call_when_ready = vfs_call_when_ready;
    directory_class->cancel_callback = vfs_cancel_callback;
    directory_class->file_monitor_add = vfs_file_monitor_add;
    directory_class->file_monitor_remove = vfs_file_monitor_remove;
    directory_class->force_reload = vfs_force_reload;
}
