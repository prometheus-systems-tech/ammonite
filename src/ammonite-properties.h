
/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include "ammonite-types.h"

#include <gtk/gtk.h>

void
ammonite_properties_present_dialog (AmmoniteFileList *files,
                                    GtkWidget        *parent_widget,
                                    GFile            *current_view_location);

GtkWindow *
ammonite_properties_present_window (AmmoniteFileList *files,
                                    const char       *startup_id);
