/*
 * ammonite-vfs-file.c: Subclass of AmmoniteFile to help implement the
 * virtual trash directory.
 *
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "ammonite-vfs-file.h"

#include "ammonite-directory-notify.h"
#include "ammonite-directory-private.h"
#include "ammonite-file-private.h"
#include <glib/gi18n.h>

G_DEFINE_TYPE (AmmoniteVFSFile, ammonite_vfs_file, AMMONITE_TYPE_FILE);

static void
vfs_file_monitor_add (AmmoniteFile       *file,
                      gconstpointer       client,
                      AmmoniteAttributes  attributes)
{
    AmmoniteDirectory *directory;

    directory = ammonite_file_get_directory (file);

    ammonite_directory_monitor_add_internal (directory, file, client, TRUE,
                                             attributes, NULL, NULL);
}

static void
vfs_file_monitor_remove (AmmoniteFile  *file,
                         gconstpointer  client)
{
    AmmoniteDirectory *directory;

    directory = ammonite_file_get_directory (file);

    ammonite_directory_monitor_remove_internal (directory, file, client);
}

static void
vfs_file_call_when_ready (AmmoniteFile         *file,
                          AmmoniteAttributes    attributes,
                          AmmoniteFileCallback  callback,
                          gpointer              callback_data)
{
    AmmoniteDirectory *directory;

    directory = ammonite_file_get_directory (file);

    ammonite_directory_call_when_ready_internal (directory, file, attributes,
                                                 NULL, callback, callback_data);
}

static void
vfs_file_cancel_call_when_ready (AmmoniteFile         *file,
                                 AmmoniteFileCallback  callback,
                                 gpointer              callback_data)
{
    AmmoniteDirectory *directory;

    directory = ammonite_file_get_directory (file);

    ammonite_directory_cancel_callback_internal (directory, file, NULL,
                                                 callback, callback_data);
}

static gboolean
vfs_file_check_if_ready (AmmoniteFile       *file,
                         AmmoniteAttributes  attributes)
{
    AmmoniteDirectory *directory;

    directory = ammonite_file_get_directory (file);

    return ammonite_directory_check_if_ready_internal (directory, file,
                                                       attributes);
}

static void
set_metadata_get_info_callback (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      callback_data)
{
    AmmoniteFile *file;
    GFileInfo *new_info;
    GError *error;

    file = callback_data;

    error = NULL;
    new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
    if (new_info != NULL)
    {
        if (ammonite_file_update_info (file, new_info))
        {
            ammonite_file_changed (file);
        }
        g_object_unref (new_info);
    }
    ammonite_file_unref (file);
    if (error)
    {
        g_error_free (error);
    }
}

static void
set_metadata_callback (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      callback_data)
{
    AmmoniteFile *file;
    GError *error;
    gboolean res;

    file = callback_data;

    error = NULL;
    res = g_file_set_attributes_finish (G_FILE (source_object),
                                        result,
                                        NULL,
                                        &error);

    if (res)
    {
        g_file_query_info_async (G_FILE (source_object),
                                 AMMONITE_FILE_DEFAULT_ATTRIBUTES,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 set_metadata_get_info_callback, file);
    }
    else
    {
        ammonite_file_unref (file);
        g_error_free (error);
    }
}

static void
vfs_file_set_metadata (AmmoniteFile         *file,
                       const char           *key,
                       AmmoniteMetadataType  type,
                       gpointer              value)
{
    g_autoptr (GFileInfo) info = g_file_info_new ();
    g_autofree char *gio_key = g_strconcat ("metadata::", key, NULL);

    if (value != NULL)
    {
        g_file_info_set_attribute (info, gio_key, (GFileAttributeType) type, value);
    }
    else
    {
        /* Unset the key */
        g_file_info_set_attribute (info, gio_key,
                                   G_FILE_ATTRIBUTE_TYPE_INVALID,
                                   NULL);
    }

    if (g_test_initialized ())
    {
        ammonite_file_update_metadata_from_info (file, info);
        return;
    }

    g_autoptr (GFile) location = ammonite_file_get_location (file);
    g_file_set_attributes_async (location,
                                 info,
                                 0,
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 set_metadata_callback,
                                 ammonite_file_ref (file));
}

static void
vfs_file_mount_callback (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    GFile *mounted_on;
    GError *error;

    op = callback_data;

    error = NULL;
    mounted_on = g_file_mount_mountable_finish (G_FILE (source_object),
                                                res, &error);
    ammonite_file_operation_complete (op, mounted_on, error);
    if (mounted_on)
    {
        g_object_unref (mounted_on);
    }
    if (error)
    {
        g_error_free (error);
    }
}


static void
vfs_file_mount (AmmoniteFile                  *file,
                GMountOperation               *mount_op,
                GCancellable                  *cancellable,
                AmmoniteFileOperationCallback  callback,
                gpointer                       callback_data)
{
    GFileType type;
    AmmoniteFileOperation *op;
    GError *error;
    GFile *location;

    type = ammonite_file_get_file_type (file);
    if (type != G_FILE_TYPE_MOUNTABLE)
    {
        if (callback)
        {
            error = NULL;
            g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                 _("This file cannot be mounted"));
            callback (file, NULL, error, callback_data);
            g_error_free (error);
        }
        return;
    }

    op = ammonite_file_operation_new (file, callback, callback_data);
    if (cancellable)
    {
        g_set_object (&op->cancellable, cancellable);
    }

    location = ammonite_file_get_location (file);
    g_file_mount_mountable (location,
                            0,
                            mount_op,
                            op->cancellable,
                            vfs_file_mount_callback,
                            op);
    g_object_unref (location);
}

static void
vfs_file_unmount_callback (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    gboolean unmounted;
    g_autoptr (GError) error = NULL;

    op = callback_data;

    unmounted = g_file_unmount_mountable_with_operation_finish (G_FILE (source_object),
                                                                res, &error);

    if (!unmounted &&
        error->domain == G_IO_ERROR &&
        (error->code == G_IO_ERROR_FAILED_HANDLED ||
         error->code == G_IO_ERROR_CANCELLED))
    {
        g_clear_error (&error);
    }

    ammonite_file_operation_complete (op, G_FILE (source_object), error);
}

static void
vfs_file_unmount (AmmoniteFile                  *file,
                  GMountOperation               *mount_op,
                  GCancellable                  *cancellable,
                  AmmoniteFileOperationCallback  callback,
                  gpointer                       callback_data)
{
    AmmoniteFileOperation *op;
    GFile *location;

    op = ammonite_file_operation_new (file, callback, callback_data);
    if (cancellable)
    {
        g_set_object (&op->cancellable, cancellable);
    }

    location = ammonite_file_get_location (file);
    g_file_unmount_mountable_with_operation (location,
                                             G_MOUNT_UNMOUNT_NONE,
                                             mount_op,
                                             op->cancellable,
                                             vfs_file_unmount_callback,
                                             op);
    g_object_unref (location);
}

static void
vfs_file_eject_callback (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    gboolean ejected;
    GError *error;

    op = callback_data;

    error = NULL;
    ejected = g_file_eject_mountable_with_operation_finish (G_FILE (source_object),
                                                            res, &error);

    if (!ejected &&
        error->domain == G_IO_ERROR &&
        (error->code == G_IO_ERROR_FAILED_HANDLED ||
         error->code == G_IO_ERROR_CANCELLED))
    {
        g_clear_error (&error);
    }

    ammonite_file_operation_complete (op, G_FILE (source_object), error);
    if (error)
    {
        g_error_free (error);
    }
}

static void
vfs_file_eject (AmmoniteFile                  *file,
                GMountOperation               *mount_op,
                GCancellable                  *cancellable,
                AmmoniteFileOperationCallback  callback,
                gpointer                       callback_data)
{
    AmmoniteFileOperation *op;
    GFile *location;

    op = ammonite_file_operation_new (file, callback, callback_data);
    if (cancellable)
    {
        g_set_object (&op->cancellable, cancellable);
    }

    location = ammonite_file_get_location (file);
    g_file_eject_mountable_with_operation (location,
                                           G_MOUNT_UNMOUNT_NONE,
                                           mount_op,
                                           op->cancellable,
                                           vfs_file_eject_callback,
                                           op);
    g_object_unref (location);
}

static void
vfs_file_start_callback (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    gboolean started;
    g_autoptr (GError) error = NULL;

    op = callback_data;

    started = g_file_start_mountable_finish (G_FILE (source_object),
                                             res, &error);

    if (!started &&
        error->domain == G_IO_ERROR &&
        (error->code == G_IO_ERROR_FAILED_HANDLED ||
         error->code == G_IO_ERROR_CANCELLED))
    {
        g_clear_error (&error);
    }

    ammonite_file_operation_complete (op, G_FILE (source_object), error);
}


static void
vfs_file_start (AmmoniteFile                  *file,
                GMountOperation               *mount_op,
                GCancellable                  *cancellable,
                AmmoniteFileOperationCallback  callback,
                gpointer                       callback_data)
{
    GFileType type;
    AmmoniteFileOperation *op;
    GError *error;
    GFile *location;

    type = ammonite_file_get_file_type (file);
    if (type != G_FILE_TYPE_MOUNTABLE)
    {
        if (callback)
        {
            error = NULL;
            g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                 _("This file cannot be started"));
            callback (file, NULL, error, callback_data);
            g_error_free (error);
        }
        return;
    }

    op = ammonite_file_operation_new (file, callback, callback_data);
    if (cancellable)
    {
        g_set_object (&op->cancellable, cancellable);
    }

    location = ammonite_file_get_location (file);
    g_file_start_mountable (location,
                            0,
                            mount_op,
                            op->cancellable,
                            vfs_file_start_callback,
                            op);
    g_object_unref (location);
}

static void
vfs_file_stop_callback (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    gboolean stopped;
    g_autoptr (GError) error = NULL;

    op = callback_data;

    stopped = g_file_stop_mountable_finish (G_FILE (source_object),
                                            res, &error);

    if (!stopped &&
        error->domain == G_IO_ERROR &&
        (error->code == G_IO_ERROR_FAILED_HANDLED ||
         error->code == G_IO_ERROR_CANCELLED))
    {
        g_clear_error (&error);
    }

    ammonite_file_operation_complete (op, G_FILE (source_object), error);
}

static void
vfs_file_stop (AmmoniteFile                  *file,
               GMountOperation               *mount_op,
               GCancellable                  *cancellable,
               AmmoniteFileOperationCallback  callback,
               gpointer                       callback_data)
{
    AmmoniteFileOperation *op;
    GFile *location;

    op = ammonite_file_operation_new (file, callback, callback_data);
    if (cancellable)
    {
        g_set_object (&op->cancellable, cancellable);
    }

    location = ammonite_file_get_location (file);
    g_file_stop_mountable (location,
                           G_MOUNT_UNMOUNT_NONE,
                           mount_op,
                           op->cancellable,
                           vfs_file_stop_callback,
                           op);
    g_object_unref (location);
}

static void
vfs_file_poll_callback (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      callback_data)
{
    AmmoniteFileOperation *op;
    gboolean stopped;
    g_autoptr (GError) error = NULL;

    op = callback_data;

    stopped = g_file_poll_mountable_finish (G_FILE (source_object),
                                            res, &error);

    if (!stopped &&
        error->domain == G_IO_ERROR &&
        (error->code == G_IO_ERROR_FAILED_HANDLED ||
         error->code == G_IO_ERROR_CANCELLED))
    {
        g_clear_error (&error);
    }

    ammonite_file_operation_complete (op, G_FILE (source_object), error);
}

static void
vfs_file_poll_for_media (AmmoniteFile *file)
{
    AmmoniteFileOperation *op;
    GFile *location;

    op = ammonite_file_operation_new (file, NULL, NULL);

    location = ammonite_file_get_location (file);
    g_file_poll_mountable (location,
                           op->cancellable,
                           vfs_file_poll_callback,
                           op);
    g_object_unref (location);
}

static void
ammonite_vfs_file_init (AmmoniteVFSFile *file)
{
}

static void
ammonite_vfs_file_class_init (AmmoniteVFSFileClass *klass)
{
    AmmoniteFileClass *file_class = AMMONITE_FILE_CLASS (klass);

    file_class->monitor_add = vfs_file_monitor_add;
    file_class->monitor_remove = vfs_file_monitor_remove;
    file_class->call_when_ready = vfs_file_call_when_ready;
    file_class->cancel_call_when_ready = vfs_file_cancel_call_when_ready;
    file_class->check_if_ready = vfs_file_check_if_ready;
    file_class->set_metadata = vfs_file_set_metadata;
    file_class->mount = vfs_file_mount;
    file_class->unmount = vfs_file_unmount;
    file_class->eject = vfs_file_eject;
    file_class->start = vfs_file_start;
    file_class->stop = vfs_file_stop;
    file_class->poll_for_media = vfs_file_poll_for_media;
}
