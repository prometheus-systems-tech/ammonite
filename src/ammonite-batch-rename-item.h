/*
 * SPDX-FileCopyrightText: 2024–2025 Markus Göllnitz <camelcasenick@bewares.it>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_BATCH_RENAME_ITEM (ammonite_batch_rename_item_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteBatchRenameItem, ammonite_batch_rename_item, AMMONITE, BATCH_RENAME_ITEM, GObject);

AmmoniteBatchRenameItem* ammonite_batch_rename_item_new (const gchar *name_before,
                                                         const gchar *name_after,
                                                         AmmoniteBatchRenameDialog *dialog);

void ammonite_batch_rename_item_set_name_before (AmmoniteBatchRenameItem *item,
                                                 const gchar *name_before);

void ammonite_batch_rename_item_set_name_after (AmmoniteBatchRenameItem *item,
                                                const gchar *name_after);

void ammonite_batch_rename_item_set_has_conflict (AmmoniteBatchRenameItem *item,
                                                  gboolean has_conflict);

G_END_DECLS
