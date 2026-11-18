/*
 * SPDX-FileCopyrightText: 2024 GNOME Foundation Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#pragma once

#include "ammonite-file.h"

#define AMMONITE_TYPE_INTERNAL_PLACE_FILE ammonite_internal_place_file_get_type ()
G_DECLARE_FINAL_TYPE (AmmoniteInternalPlaceFile, ammonite_internal_place_file,
                      AMMONITE, INTERNAL_PLACE_FILE,
                      AmmoniteFile)
