/*
 * SPDX-FileCopyrightText: 2009 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <config.h>
#include "ammonite-metadata.h"
#include <glib.h>

static char *used_metadata_names[] =
{
    AMMONITE_METADATA_KEY_ICON_VIEW_SORT_BY,
    AMMONITE_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
    AMMONITE_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
    AMMONITE_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
    AMMONITE_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
    AMMONITE_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
    AMMONITE_METADATA_KEY_CUSTOM_ICON,
    AMMONITE_METADATA_KEY_CUSTOM_ICON_NAME,
    AMMONITE_METADATA_KEY_EMBLEMS,
    NULL
};

guint
ammonite_metadata_get_id (const char *metadata)
{
    static GHashTable *hash;
    int i;

    if (hash == NULL)
    {
        hash = g_hash_table_new (g_str_hash, g_str_equal);
        for (i = 0; used_metadata_names[i] != NULL; i++)
        {
            g_hash_table_insert (hash,
                                 used_metadata_names[i],
                                 GINT_TO_POINTER (i + 1));
        }
    }

    return GPOINTER_TO_INT (g_hash_table_lookup (hash, metadata));
}
