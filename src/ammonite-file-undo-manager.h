
/*
 * SPDX-FileCopyrightText: 2007-2011 Amos Brocco
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Amos Brocco <amos.brocco@gmail.com>
 */

#pragma once

#include "ammonite-types.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#define AMMONITE_TYPE_FILE_UNDO_MANAGER\
	(ammonite_file_undo_manager_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteFileUndoManager, ammonite_file_undo_manager, AMMONITE, FILE_UNDO_MANAGER, GObject)

typedef enum {
	AMMONITE_FILE_UNDO_MANAGER_STATE_NONE,
	AMMONITE_FILE_UNDO_MANAGER_STATE_UNDO,
	AMMONITE_FILE_UNDO_MANAGER_STATE_REDO
} AmmoniteFileUndoManagerState;

AmmoniteFileUndoManager *ammonite_file_undo_manager_new (void);
AmmoniteFileUndoManager * ammonite_file_undo_manager_get (void);

void ammonite_file_undo_manager_set_action (AmmoniteFileUndoInfo *info);
AmmoniteFileUndoInfo *ammonite_file_undo_manager_get_action (void);

AmmoniteFileUndoManagerState ammonite_file_undo_manager_get_state (void);

void ammonite_file_undo_manager_undo (GtkWindow                      *parent_window,
                                      AmmoniteFileOperationsDBusData *dbus_data);
void ammonite_file_undo_manager_redo (GtkWindow                      *parent_window,
                                      AmmoniteFileOperationsDBusData *dbus_data);

gboolean ammonite_file_undo_manager_is_operating (void);
