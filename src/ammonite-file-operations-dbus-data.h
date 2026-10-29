/*
 * SPDX-FileCopyrightText: 2020 Alberts Muktupāvels
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>

typedef struct _AmmoniteFileOperationsDBusData AmmoniteFileOperationsDBusData;

AmmoniteFileOperationsDBusData *ammonite_file_operations_dbus_data_new               (GVariant                       *platform_data);

AmmoniteFileOperationsDBusData *ammonite_file_operations_dbus_data_ref               (AmmoniteFileOperationsDBusData *self);

void                            ammonite_file_operations_dbus_data_unref             (AmmoniteFileOperationsDBusData *self);

const char                     *ammonite_file_operations_dbus_data_get_parent_handle (AmmoniteFileOperationsDBusData *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AmmoniteFileOperationsDBusData, ammonite_file_operations_dbus_data_unref)
