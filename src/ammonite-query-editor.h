/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>

#include "ammonite-types.h"

#define AMMONITE_TYPE_QUERY_EDITOR ammonite_query_editor_get_type()

G_DECLARE_FINAL_TYPE (AmmoniteQueryEditor, ammonite_query_editor, AMMONITE, QUERY_EDITOR, GtkWidget)

GtkWidget     *ammonite_query_editor_new          (void);

/**
 * ammonite_query_editor_set_query:
 *
 * @editor: A #AmmoniteQueryEditor instance.
 * @query: (nullable) (transfer full): The #AmmoniteQuery for the search.
 */
void           ammonite_query_editor_set_query    (AmmoniteQueryEditor *editor,
                                                   AmmoniteQuery       *query);
/**
 * ammonite_query_editor_set_location:
 *
 * @editor: A #AmmoniteQueryEditor instance.
 * @location: (nullable) (transfer full): The location in which the search will take place.
 */
void           ammonite_query_editor_set_location (AmmoniteQueryEditor *editor,
                                                   GFile               *location);
/**
 * ammonite_query_editor_select_all_text:
 *
 * @editor: A #AmmoniteQueryEditor instance.
 */
void           ammonite_query_editor_select_all_text (AmmoniteQueryEditor   *editor);

gboolean       ammonite_query_editor_handle_event    (AmmoniteQueryEditor   *self,
                                                      GtkEventControllerKey *controller,
                                                      guint                  keyval,
                                                      GdkModifierType        state);
