/*
 * SPDX-FileCopyrightText: 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_SHORTCUT_MANAGER (ammonite_shortcut_manager_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteShortcutManager, ammonite_shortcut_manager, AMMONITE, SHORTCUT_MANAGER, AdwBin)

AmmoniteShortcutManager * ammonite_shortcut_manager_new (void);

G_END_DECLS
