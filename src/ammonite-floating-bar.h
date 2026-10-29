
/*
 * SPDX-FileCopyrightText: 2011 Red Hat Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>

#define AMMONITE_FLOATING_BAR_ACTION_ID_STOP 1

#define AMMONITE_TYPE_FLOATING_BAR ammonite_floating_bar_get_type()
G_DECLARE_FINAL_TYPE (AmmoniteFloatingBar, ammonite_floating_bar, AMMONITE, FLOATING_BAR, GtkBox)

GtkWidget * ammonite_floating_bar_new              (const gchar *primary_label,
						    const gchar *details_label,
						    gboolean show_spinner);

void       ammonite_floating_bar_set_primary_label (AmmoniteFloatingBar *self,
						    const gchar *label);
void       ammonite_floating_bar_set_details_label (AmmoniteFloatingBar *self,
						    const gchar *label);
void        ammonite_floating_bar_set_labels        (AmmoniteFloatingBar *self,
						     const gchar *primary,
						     const gchar *detail);
void        ammonite_floating_bar_set_show_stop (AmmoniteFloatingBar *self,
						    gboolean show_spinner);

void        ammonite_floating_bar_remove_hover_timeout (AmmoniteFloatingBar *self);
