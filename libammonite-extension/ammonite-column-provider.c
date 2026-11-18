/*
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#include "ammonite-column-provider.h"

/**
 * AmmoniteColumnProvider:
 *
 * Interface to provide additional list view columns
 *
 * Allows extension to provide additional columns in the file manager list view.
 */

G_DEFINE_INTERFACE (AmmoniteColumnProvider, ammonite_column_provider, G_TYPE_OBJECT)

static void
ammonite_column_provider_default_init (AmmoniteColumnProviderInterface *klass)
{
}

/**
 * ammonite_column_provider_get_columns:
 *
 * Provide a list of [class@Column].
 *
 * The `AmmoniteColumnProvider` only provides the metadata of the columns
 * (importantly the [property@Column:attribute]).  You will very likely also
 * implement [iface@InfoProvider] in order to call [method@FileInfo.add_string_attribute]
 * on all necessary files.
 *
 * This method should return immediately without any blocking i/o.
 *
 * Returns: (nullable) (element-type AmmoniteColumn) (transfer full): the provided `AmmoniteColumn` objects
 */
GList *
ammonite_column_provider_get_columns (AmmoniteColumnProvider *self)
{
    AmmoniteColumnProviderInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_COLUMN_PROVIDER (self), NULL);

    iface = AMMONITE_COLUMN_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->get_columns != NULL, NULL);

    return iface->get_columns (self);
}
