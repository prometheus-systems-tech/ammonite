/*
 * SPDX-FileCopyrightText: 2016 the Ammonite developers
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>
#include <glib-object.h>

#define AMMONITE_TYPE_FILENAME_VALIDATOR ammonite_filename_validator_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteFilenameValidator, ammonite_filename_validator, AMMONITE, FILENAME_VALIDATOR, GObject)

gboolean ammonite_filename_validator_get_has_feedback        (AmmoniteFilenameValidator *self);
gchar * ammonite_filename_validator_get_new_name             (AmmoniteFilenameValidator *self);
gboolean ammonite_filename_validator_get_will_overwrite      (AmmoniteFilenameValidator *self);

void    ammonite_filename_validator_set_containing_directory (AmmoniteFilenameValidator *self,
                                                              AmmoniteDirectory         *directory);
void    ammonite_filename_validator_set_target_is_folder     (AmmoniteFilenameValidator *self,
                                                              gboolean                   is_folder);
void    ammonite_filename_validator_set_original_name        (AmmoniteFilenameValidator *self,
                                                              const char                *original_name);
void    ammonite_filename_validator_validate                 (AmmoniteFilenameValidator *self);
void    ammonite_filename_validator_try_accept               (AmmoniteFilenameValidator *self);
