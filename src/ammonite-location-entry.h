
/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

#pragma once

#include <gtk/gtk.h>

#define AMMONITE_TYPE_LOCATION_ENTRY ammonite_location_entry_get_type()
G_DECLARE_FINAL_TYPE (AmmoniteLocationEntry, ammonite_location_entry,
                      AMMONITE, LOCATION_ENTRY, GtkEntry)

GtkWidget* ammonite_location_entry_new          	(void);
void       ammonite_location_entry_set_special_text     (AmmoniteLocationEntry *entry,
							 const char            *special_text);
void       ammonite_location_entry_set_location         (AmmoniteLocationEntry *entry,
							 GFile                 *location);
