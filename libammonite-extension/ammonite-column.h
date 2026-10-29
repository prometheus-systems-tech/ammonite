/*
 * ammonite-column.h - Info columns exported by AmmoniteColumnProvider objects.
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_COLUMN (ammonite_column_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteColumn, ammonite_column, AMMONITE, COLUMN, GObject)

AmmoniteColumn *ammonite_column_new  (const char *name,
                                      const char *attribute,
                                      const char *label,
                                      const char *description);

G_END_DECLS
