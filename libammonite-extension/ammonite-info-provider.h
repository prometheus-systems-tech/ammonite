/*
 * ammonite-info-provider.h - Interface for Ammonite extensions that provide info about files.
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

/* This interface is implemented by Ammonite extensions that want to 
 * provide information about files.  Extensions are called when Ammonite 
 * needs information about a file.  They are passed a AmmoniteFileInfo 
 * object which should be filled with relevant information */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include "ammonite-file-info.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_INFO_PROVIDER (ammonite_info_provider_get_type ())

G_DECLARE_INTERFACE (AmmoniteInfoProvider, ammonite_info_provider,
                     AMMONITE, INFO_PROVIDER,
                     GObject)

/**
 * AmmoniteOperationHandle:
 *
 * Handle for asynchronous interfaces.
 *
 * These are opaque handles that must be unique within an extension object.
 * These are set by operations that return `AMMONITE_OPERATION_IN_PROGRESS`.
 */
typedef struct _AmmoniteOperationHandle AmmoniteOperationHandle;

/**
 * AmmoniteOperationResult:
 * @AMMONITE_OPERATION_COMPLETE: the operation succeeded, and the extension
 *  is done with the request.
 * @AMMONITE_OPERATION_FAILED: the operation failed.
 * @AMMONITE_OPERATION_IN_PROGRESS: the extension has begin an async operation.
 *  When this value is returned, the extension must set the handle parameter
 *  and call the callback closure when the operation is complete.
 *
 * Return values for asynchronous operations performed by the extension.
 */
typedef enum
{
    AMMONITE_OPERATION_COMPLETE,
    AMMONITE_OPERATION_FAILED,
    AMMONITE_OPERATION_IN_PROGRESS
} AmmoniteOperationResult;

struct _AmmoniteInfoProviderInterface
{
    GTypeInterface g_iface;

    AmmoniteOperationResult (*update_file_info) (AmmoniteInfoProvider     *provider,
                                                 AmmoniteFileInfo         *file,
                                                 GClosure                 *update_complete,
                                                 AmmoniteOperationHandle **handle);
    void                    (*cancel_update)    (AmmoniteInfoProvider     *provider,
                                                 AmmoniteOperationHandle  *handle);
};

AmmoniteOperationResult ammonite_info_provider_update_file_info       (AmmoniteInfoProvider     *provider,
                                                                       AmmoniteFileInfo         *file,
                                                                       GClosure                 *update_complete,
                                                                       AmmoniteOperationHandle **handle);

void                    ammonite_info_provider_cancel_update          (AmmoniteInfoProvider     *provider,
                                                                       AmmoniteOperationHandle  *handle);

void                    ammonite_info_provider_update_complete_invoke (GClosure                 *update_complete,
                                                                       AmmoniteInfoProvider     *provider,
                                                                       AmmoniteOperationHandle  *handle,
                                                                       AmmoniteOperationResult   result);

G_END_DECLS
