/*
 * SPDX-FileCopyrightText: 2016 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 *          António Fernandes <antoniof@gnome.org>
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_VIEW_MODEL (ammonite_view_model_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteViewModel, ammonite_view_model, AMMONITE, VIEW_MODEL, GObject)

AmmoniteViewModel * ammonite_view_model_new (gboolean single_selection);

GtkFilter *ammonite_view_model_get_filter (AmmoniteViewModel *self);
void ammonite_view_model_set_filter (AmmoniteViewModel *self,
                                     GtkFilter         *filter);
gboolean ammonite_view_model_get_single_selection (AmmoniteViewModel *self);
GtkSorter *ammonite_view_model_get_sorter (AmmoniteViewModel *self);
void ammonite_view_model_set_sorter (AmmoniteViewModel *self,
                                     GtkSorter         *sorter);
void ammonite_view_model_set_section_sorter (AmmoniteViewModel *self,
                                             GtkSorter         *section_sorter);
void ammonite_view_model_sort (AmmoniteViewModel *self);
AmmoniteViewItem * ammonite_view_model_get_item_for_file (AmmoniteViewModel *self,
                                                          AmmoniteFile      *file);
/* Don't use inside a loop, use ammonite_view_model_remove_all_items instead. */
void ammonite_view_model_remove_items (AmmoniteViewModel     *self,
                                       GHashTable            *items,
                                       AmmoniteDirectory     *directory);
void ammonite_view_model_remove_all_items (AmmoniteViewModel *self);
void ammonite_view_model_add_items (AmmoniteViewModel *self,
                                    GList             *items);
void ammonite_view_model_clear_subdirectory (AmmoniteViewModel *self,
                                             AmmoniteViewItem  *item);
void ammonite_view_model_expand_as_a_tree (AmmoniteViewModel *self,
                                           gboolean           expand_as_a_tree);
void ammonite_view_model_set_cut_files (AmmoniteViewModel *self,
                                        GList             *cut_files);

G_END_DECLS
