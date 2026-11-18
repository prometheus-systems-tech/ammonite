/*
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#include "ammonite-info-provider.h"

#include "ammonite-extension-enum-types.h"

/**
 * AmmoniteInfoProvider:
 *
 * Interface to provide additional information about files
 *
 * `AmmoniteInfoProvider` allows extension to provide additional information about
 * files. When [method@InfoProvider.update_file_info] is called by the application,
 * extensions will know that it's time to add extra information to the provided
 * [iface@FileInfo].
 */

G_DEFINE_INTERFACE (AmmoniteInfoProvider, ammonite_info_provider, G_TYPE_OBJECT)

static void
ammonite_info_provider_default_init (AmmoniteInfoProviderInterface *klass)
{
}

/**
 * ammonite_info_provider_update_file_info:
 * @file: a file requesting updated info.
 * @update_complete: the closure to invoke at some later time when returning
 *                   @AMMONITE_OPERATION_IN_PROGRESS.
 * @handle: (transfer none) (nullable) (out): an opaque #AmmoniteOperationHandle
 *           that must be set when returning @AMMONITE_OPERATION_IN_PROGRESS.
 *
 * Called when the Ammonite application is requesting updated file information.
 *
 * Returns: whether the operation finished.
 */
AmmoniteOperationResult
ammonite_info_provider_update_file_info (AmmoniteInfoProvider     *self,
                                         AmmoniteFileInfo         *file,
                                         GClosure                 *update_complete,
                                         AmmoniteOperationHandle **handle)
{
    AmmoniteInfoProviderInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_INFO_PROVIDER (self),
                          AMMONITE_OPERATION_FAILED);
    g_return_val_if_fail (update_complete != NULL,
                          AMMONITE_OPERATION_FAILED);
    g_return_val_if_fail (handle != NULL, AMMONITE_OPERATION_FAILED);

    iface = AMMONITE_INFO_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->update_file_info != NULL,
                          AMMONITE_OPERATION_FAILED);

    return iface->update_file_info (self, file, update_complete, handle);
}

/**
 * ammonite_info_provider_cancel_update:
 * @provider: a #AmmoniteInfoProvider
 * @handle: the opaque #AmmoniteOperationHandle returned from a previous call to
 *          ammonite_info_provider_update_file_info().
 *
 * Called when the Ammonite application has canceled an update.
 *
 * This method is only relevant if [method@InfoProvider.update_file_info] was returned with
 * `AMMONITE_OPERATION_IN_PROGRESS`.
 */
void
ammonite_info_provider_cancel_update (AmmoniteInfoProvider    *self,
                                      AmmoniteOperationHandle *handle)
{
    AmmoniteInfoProviderInterface *iface;

    g_return_if_fail (AMMONITE_IS_INFO_PROVIDER (self));
    g_return_if_fail (handle != NULL);

    iface = AMMONITE_INFO_PROVIDER_GET_IFACE (self);

    g_return_if_fail (iface->cancel_update != NULL);

    iface->cancel_update (self, handle);
}

/**
 * ammonite_info_provider_update_complete_invoke:
 * @update_complete: the closure to call
 * @provider: an info provider
 * @handle: the handle for the given closure
 * @result: either `AMMONITE_OPERATION_COMPLETE` or `AMMONITE_OPERATION_FAILED`
 *
 * Complete an async file info update.
 */
void
ammonite_info_provider_update_complete_invoke (GClosure                *update_complete,
                                               AmmoniteInfoProvider    *provider,
                                               AmmoniteOperationHandle *handle,
                                               AmmoniteOperationResult  result)
{
    GValue args[3] = { { 0, } };
    GValue return_val = { 0, };

    g_return_if_fail (update_complete != NULL);
    g_return_if_fail (AMMONITE_IS_INFO_PROVIDER (provider));

    g_value_init (&args[0], AMMONITE_TYPE_INFO_PROVIDER);
    g_value_init (&args[1], G_TYPE_POINTER);
    g_value_init (&args[2], AMMONITE_TYPE_OPERATION_RESULT);

    g_value_set_object (&args[0], provider);
    g_value_set_pointer (&args[1], handle);
    g_value_set_enum (&args[2], result);

    g_closure_invoke (update_complete, &return_val, 3, args, NULL);

    g_value_unset (&args[0]);
    g_value_unset (&args[1]);
    g_value_unset (&args[2]);
}
