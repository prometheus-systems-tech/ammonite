/*
 * SPDX-FileCopyrightText: 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#define AMMONITE_EXTENSION_H

#include <libammonite-extension/ammonite-column-provider.h>
#include <libammonite-extension/ammonite-column.h>
#include <libammonite-extension/ammonite-extension-enum-types.h>
#include <libammonite-extension/ammonite-file-info.h>
#include <libammonite-extension/ammonite-info-provider.h>
#include <libammonite-extension/ammonite-menu.h>
#include <libammonite-extension/ammonite-menu-provider.h>
#include <libammonite-extension/ammonite-properties-model.h>
#include <libammonite-extension/ammonite-properties-model-provider.h>
#include <libammonite-extension/ammonite-properties-item.h>

G_BEGIN_DECLS

/**
 * ammonite_module_initialize: (skip)
 * @module: a module used in type registration
 *
 * Called when the extension is begin loaded to register the types it exports
 * and to perform other initializations.
 */
void ammonite_module_initialize (GTypeModule  *module);
/**
 * ammonite_module_shutdown: (skip)
 *
 * Called when the extension is being unloaded.
 */
void ammonite_module_shutdown   (void);
/**
 * ammonite_module_list_types: (skip)
 * @types: (out) (transfer none) (array length=num_types): array of GType *
 * @num_types: the number of types in the array
 *
 * Called after the extension has been initialized and has registered all the
 * types it exports, to load them into Ammonite.
 */
void ammonite_module_list_types (const GType **types,
                                 int          *num_types);

G_END_DECLS
