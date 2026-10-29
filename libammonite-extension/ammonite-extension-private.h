/*
 * ammonite-extension-private.h - Type definitions for Ammonite extensions
 *
 * SPDX-FileCopyrightText: 2009 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include <libammonite-extension/ammonite-file-info.h>

G_BEGIN_DECLS

extern AmmoniteFileInfo *(*ammonite_file_info_getter) (GFile *location, gboolean create);

G_END_DECLS
