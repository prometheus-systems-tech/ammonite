
/*
 * SPDX-FileCopyrightText: 2004 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Dave Camp <dave@ximian.com>
 */

#pragma once

#include "ammonite-types.h"

#include <adwaita.h>
#include <gtk/gtk.h>

#define AMMONITE_TYPE_COLUMN_CHOOSER ammonite_column_chooser_get_type()

G_DECLARE_FINAL_TYPE (AmmoniteColumnChooser, ammonite_column_chooser, AMMONITE, COLUMN_CHOOSER, AdwDialog);

GtkWidget *ammonite_column_chooser_new             (AmmoniteFile            *file);
