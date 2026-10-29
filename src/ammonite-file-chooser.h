/*
 * SPDX-FileCopyrightText: 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#pragma once

#include <gio/gio.h>
#include <adwaita.h>

#include "ammonite-enums.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_FILE_CHOOSER (ammonite_file_chooser_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteFileChooser, ammonite_file_chooser, AMMONITE, FILE_CHOOSER, AdwWindow)

AmmoniteFileChooser *
ammonite_file_chooser_new (AmmoniteMode mode);

void
ammonite_file_chooser_set_accept_label (AmmoniteFileChooser *self,
                                        const char          *accept_label);
void
ammonite_file_chooser_set_current_filter (AmmoniteFileChooser *self,
                                          guint                position);
void
ammonite_file_chooser_set_filters (AmmoniteFileChooser *self,
                                   GListModel          *filters);
void
ammonite_file_chooser_set_starting_location (AmmoniteFileChooser *self,
                                             GFile               *starting_location);
void
ammonite_file_chooser_set_suggested_name (AmmoniteFileChooser *self,
                                          const char          *suggested_name);

void
ammonite_file_chooser_add_choices        (AmmoniteFileChooser *self,
                                          GActionGroup        *action_group,
                                          GMenuModel          *menu);

G_END_DECLS
