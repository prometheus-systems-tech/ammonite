/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_VIEW_CONTROLS (ammonite_view_controls_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteViewControls, ammonite_view_controls, AMMONITE, VIEW_CONTROLS, AdwBin)

G_END_DECLS
