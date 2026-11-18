/*
 * SPDX-FileCopyrightText: 2008 Free Software Foundation, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Christian Neumair <cneumair@gnome.org>
 */

#pragma once

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "ammonite-types.h"

#define AMMONITE_TYPE_WINDOW_SLOT (ammonite_window_slot_get_type ())
G_DECLARE_FINAL_TYPE (AmmoniteWindowSlot, ammonite_window_slot, AMMONITE, WINDOW_SLOT, AdwBin)

typedef struct
{
    GList *back_list;
    GList *forward_list;
    AmmoniteBookmark *current_location_bookmark;
} AmmoniteNavigationState;

GtkWidget *          ammonite_window_slot_get_pane_widget (AmmoniteWindowSlot *slot);
AmmoniteWindowSlot * ammonite_window_slot_get_secondary (AmmoniteWindowSlot *slot);
void                 ammonite_window_slot_set_secondary (AmmoniteWindowSlot *slot,
                                                         AmmoniteWindowSlot *secondary);
AmmoniteWindowSlot * ammonite_window_slot_new              (AmmoniteMode        mode);

void ammonite_window_slot_open_location_full               (AmmoniteWindowSlot *slot,
                                                            GFile              *location,
                                                            AmmoniteFileList   *new_selection);

GtkFilter *ammonite_window_slot_get_filter                 (AmmoniteWindowSlot *slot);
void ammonite_window_slot_set_filter                       (AmmoniteWindowSlot *slot,
                                                            GtkFilter          *filter);
AmmoniteMode ammonite_window_slot_get_mode                 (AmmoniteWindowSlot *slot);
GFile * ammonite_window_slot_get_location		   (AmmoniteWindowSlot *slot);

GList * ammonite_window_slot_get_back_history              (AmmoniteWindowSlot *slot);
GList * ammonite_window_slot_get_forward_history           (AmmoniteWindowSlot *slot);

gboolean ammonite_window_slot_get_allow_stop               (AmmoniteWindowSlot *slot);
void     ammonite_window_slot_stop_loading                 (AmmoniteWindowSlot *slot);

const gchar *ammonite_window_slot_get_title                (AmmoniteWindowSlot *slot);

gboolean ammonite_window_slot_handle_activate_files        (AmmoniteWindowSlot *slot,
                                                            GList              *files);
gboolean ammonite_window_slot_handle_event       	   (AmmoniteWindowSlot    *slot,
							    GtkEventControllerKey *controller,
							    guint                  keyval,
							    GdkModifierType        state);

const gchar*   ammonite_window_slot_get_icon_name                (AmmoniteWindowSlot *slot);

const gchar*   ammonite_window_slot_get_tooltip                  (AmmoniteWindowSlot *slot);
const gchar*   ammonite_window_slot_get_tooltip_with_description (AmmoniteWindowSlot  *slot,
                                                                  const gchar        **description);

GMenuModel* ammonite_window_slot_get_templates_menu (AmmoniteWindowSlot *self);

GMenuModel* ammonite_window_slot_get_extensions_background_menu (AmmoniteWindowSlot *self);

gboolean ammonite_window_slot_get_active                   (AmmoniteWindowSlot *slot);

void     ammonite_window_slot_set_active                   (AmmoniteWindowSlot *slot,
                                                            gboolean            active);
gboolean ammonite_window_slot_get_loading                  (AmmoniteWindowSlot *slot);

gboolean ammonite_window_slot_get_search_visible           (AmmoniteWindowSlot *slot);

gboolean ammonite_window_slot_get_search_global            (AmmoniteWindowSlot *self);

GList* ammonite_window_slot_get_selection                  (AmmoniteWindowSlot *slot);

AmmoniteSelectionSource ammonite_window_slot_get_selection_source (AmmoniteWindowSlot *self);

void     ammonite_window_slot_search                       (AmmoniteWindowSlot *slot,
                                                            AmmoniteQuery      *query);

void ammonite_window_slot_restore_navigation_state (AmmoniteWindowSlot      *self,
                                                    AmmoniteNavigationState *data);

AmmoniteNavigationState* ammonite_window_slot_get_navigation_state (AmmoniteWindowSlot *self);

AmmoniteQueryEditor *ammonite_window_slot_get_query_editor (AmmoniteWindowSlot *self);

AmmoniteFilesView*  ammonite_window_slot_get_current_view  (AmmoniteWindowSlot *slot);

void
ammonite_window_slot_navigate (AmmoniteWindowSlot *self,
                               int                 distance);

void free_navigation_state                                 (gpointer data);
