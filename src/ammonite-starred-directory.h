/*
 * SPDX-FileCopyrightText: 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ammonite-directory.h"

G_BEGIN_DECLS

#define AMMONITE_STARRED_DIRECTORY_PROVIDER_NAME "starred-directory-provider"

#define AMMONITE_TYPE_STARRED_DIRECTORY (ammonite_starred_directory_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteFavoriteDirectory, ammonite_starred_directory, AMMONITE, STARRED_DIRECTORY, AmmoniteDirectory);

AmmoniteFavoriteDirectory* ammonite_starred_directory_new      (void);

G_END_DECLS
