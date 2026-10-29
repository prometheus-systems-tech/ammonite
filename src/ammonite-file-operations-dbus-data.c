/*
 * SPDX-FileCopyrightText: 2020 Alberts Muktupāvels
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include "ammonite-file-operations-dbus-data.h"

struct _AmmoniteFileOperationsDBusData
{
    gatomicrefcount ref_count;

    char *parent_handle;
};

AmmoniteFileOperationsDBusData *
ammonite_file_operations_dbus_data_new (GVariant *platform_data)
{
    AmmoniteFileOperationsDBusData *self;
    GVariantDict dict;

    self = g_new0 (AmmoniteFileOperationsDBusData, 1);
    g_atomic_ref_count_init (&self->ref_count);

    g_variant_dict_init (&dict, platform_data);

    g_variant_dict_lookup (&dict, "parent-handle", "s", &self->parent_handle);

    return self;
}

AmmoniteFileOperationsDBusData *
ammonite_file_operations_dbus_data_ref (AmmoniteFileOperationsDBusData *self)
{
    g_atomic_ref_count_inc (&self->ref_count);

    return self;
}

void
ammonite_file_operations_dbus_data_unref (AmmoniteFileOperationsDBusData *self)
{
    if (g_atomic_ref_count_dec (&self->ref_count))
    {
        g_free (self->parent_handle);
        g_free (self);
    }
}

const char *
ammonite_file_operations_dbus_data_get_parent_handle (AmmoniteFileOperationsDBusData *self)
{
    return self->parent_handle;
}
