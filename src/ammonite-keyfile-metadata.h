/*
 * SPDX-FileCopyrightText: 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <glib.h>

#include "ammonite-types.h"

void ammonite_keyfile_metadata_set_string (AmmoniteFile *file,
                                           const char *keyfile_filename,
                                           const gchar *name,
                                           const gchar *key,
                                           const gchar *string);

void ammonite_keyfile_metadata_set_stringv (AmmoniteFile *file,
                                            const char *keyfile_filename,
                                            const char *name,
                                            const char *key,
                                            const char * const *stringv);

gboolean ammonite_keyfile_metadata_update_from_keyfile (AmmoniteFile *file,
                                                        const char *keyfile_filename,
                                                        const gchar *name);
