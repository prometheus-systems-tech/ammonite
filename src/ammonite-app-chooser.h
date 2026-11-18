/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_APP_CHOOSER (ammonite_app_chooser_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteAppChooser, ammonite_app_chooser, AMMONITE, APP_CHOOSER, AdwDialog)

AmmoniteAppChooser *ammonite_app_chooser_new (GList     *files);

void                ammonite_app_chooser_set_default_only (AmmoniteAppChooser *self);

GAppInfo           *ammonite_app_chooser_get_app_info (AmmoniteAppChooser *self);

G_END_DECLS
