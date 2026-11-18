/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_FOLDER_TREE (ammonite_folder_tree_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteFolderTree, ammonite_folder_tree,
                      AMMONITE, FOLDER_TREE, AdwBin)

GtkWidget * ammonite_folder_tree_new (void);

G_END_DECLS
