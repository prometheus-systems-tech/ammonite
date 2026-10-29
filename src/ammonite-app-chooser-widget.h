/*
 * SPDX-FileCopyrightText: 2004 Novell, Inc.
 * SPDX-FileCopyrightText: 2007, 2010 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 GNOME Files Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Dave Camp <dave@novell.com>
 *          Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <ccecchi@redhat.com>
 *          Khalid Abu Shawarib <kas@gnome.org>
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_APP_CHOOSER_WIDGET (ammonite_app_chooser_widget_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteAppChooserWidget, ammonite_app_chooser_widget,
                      AMMONITE, APP_CHOOSER_WIDGET, AdwBin)

AmmoniteAppChooserWidget *ammonite_app_chooser_widget_new      (const char          *content_type);

void          ammonite_app_chooser_widget_set_search_entry     (AmmoniteAppChooserWidget *self,
                                                                GtkEditable         *editable);

GAppInfo *    ammonite_app_chooser_widget_get_app_info (AmmoniteAppChooserWidget *self);
void          ammonite_app_chooser_widget_refresh (AmmoniteAppChooserWidget *self);

G_END_DECLS

