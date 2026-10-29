/*
 * SPDX-FileCopyrightText: 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once


#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
  AMMONITE_SEARCH_FILTER_CONTENT, /* Full text or filename */
  AMMONITE_SEARCH_FILTER_DATE,    /* When */
  AMMONITE_SEARCH_FILTER_LAST,    /* Last modified or last used */
  AMMONITE_SEARCH_FILTER_TYPE     /* What */
} AmmoniteSearchFilter;

#define AMMONITE_TYPE_SEARCH_POPOVER (ammonite_search_popover_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteSearchPopover, ammonite_search_popover, AMMONITE, SEARCH_POPOVER, GtkPopover)

GtkWidget*           ammonite_search_popover_new                 (void);

void
ammonite_search_popover_set_date_range (AmmoniteSearchPopover *popover,
                                        GPtrArray             *date_range);
void                 ammonite_search_popover_reset_mime_types    (AmmoniteSearchPopover *popover);

void                 ammonite_search_popover_set_fts_available   (AmmoniteSearchPopover *popover,
                                                                  gboolean               sensitive);

G_END_DECLS
