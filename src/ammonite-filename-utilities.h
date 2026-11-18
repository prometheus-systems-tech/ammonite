/*
 * SPDX-FileCopyrightText: 2023 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>

char *
ammonite_filename_for_conflict (const char *name,
                                int         count_increment,
                                int         max_length,
                                gboolean    ignore_extension);

char *
ammonite_filename_for_copy (const char *name,
                            int         count_increment,
                            int         max_length,
                            gboolean    ignore_extension);

char *
ammonite_filename_for_link (const char *name,
                            size_t      count,
                            int         max_length);

char *
ammonite_filename_get_common_prefix (const char * const *strv,
                                     int                 min_required_len);

const char *
ammonite_filename_get_extension (const char *filename);

int
ammonite_filename_get_extension_char_offset (const char *filename);

gboolean
ammonite_filename_shorten_base (char       **filename,
                                const char  *base,
                                size_t       max_length);

char *
ammonite_filename_strip_extension (const char *filename);

char *
ammonite_filename_strip (char *filename);
