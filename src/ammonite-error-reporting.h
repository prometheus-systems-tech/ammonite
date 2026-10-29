/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

#pragma once

#include "ammonite-types.h"

#include "ammonite-file.h"

#include <gtk/gtk.h>

#define FAT_FORBIDDEN_CHARACTERS                ":|<>*?\\\"/"

void ammonite_report_error_loading_directory	 (AmmoniteFile   *file,
						  GError         *error,
						  GtkWidget	 *parent);
void ammonite_report_error_renaming_file         (AmmoniteFile *file,
						  const char *new_name,
						  GError *error,
						  GtkWidget *parent);
void ammonite_report_error_setting_permissions (AmmoniteFile   *file,
						GError         *error,
						GtkWidget    *parent);
void ammonite_report_error_setting_owner       (AmmoniteFile   *file,
						GError         *error,  
						GtkWidget    *parent);
void ammonite_report_error_setting_group       (AmmoniteFile   *file,
						GError         *error,
						GtkWidget    *parent);
