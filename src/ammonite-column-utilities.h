
/*
 * SPDX-FileCopyrightText: 2004 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Dave Camp <dave@ximian.com>
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>

GList *ammonite_get_all_columns       (void);
GList *ammonite_get_common_columns    (void);
GList *ammonite_get_columns_for_file (AmmoniteFile *file);
GList *ammonite_column_list_copy      (GList       *columns);
void   ammonite_column_list_free      (GList       *columns);

GList *ammonite_sort_columns          (GList       *columns,
				       char       **column_order);
void   ammonite_column_save_metadata  (AmmoniteFile *file,
                                       GStrv         column_order,
                                       GStrv         visible_column);

GStrv  ammonite_column_get_default_visible_columns (AmmoniteFile *file);
GStrv  ammonite_column_get_visible_columns         (AmmoniteFile *file);
GStrv  ammonite_column_get_default_column_order    (AmmoniteFile *file);
GStrv  ammonite_column_get_column_order            (AmmoniteFile *file);
