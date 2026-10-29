/*
 * SPDX-FileCopyrightText: 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_GRID_VIEW_CAPTIONS_DIALOG (ammonite_grid_view_captions_dialog_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteGridViewCaptionsDialog, ammonite_grid_view_captions_dialog, AMMONITE, GRID_VIEW_CAPTIONS_DIALOG, AdwDialog)

void
ammonite_grid_view_captions_dialog_present (GtkWidget *parent);

G_END_DECLS
