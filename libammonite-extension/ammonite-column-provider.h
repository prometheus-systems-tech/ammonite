/*
 * ammonite-column-provider.h - Interface for Ammonite extensions that provide column descriptions.
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

/* This interface is implemented by Ammonite extensions that want to
 * add columns to the list view and details to the icon view.
 * Extensions are asked for a list of columns to display.  Each
 * returned column refers to a string attribute which can be filled in
 * by AmmoniteInfoProvider */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_COLUMN_PROVIDER (ammonite_column_provider_get_type ())

G_DECLARE_INTERFACE (AmmoniteColumnProvider, ammonite_column_provider,
                     AMMONITE, COLUMN_PROVIDER,
                     GObject)

struct _AmmoniteColumnProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_columns) (AmmoniteColumnProvider *provider);
};

GList *ammonite_column_provider_get_columns (AmmoniteColumnProvider *provider);

G_END_DECLS
