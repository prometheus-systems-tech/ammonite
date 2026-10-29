/*
 * SPDX-FileCopyrightText: 2007-2011 Amos Brocco
 * SPDX-FileCopyrightText: 2010, 2012 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */
#define G_LOG_DOMAIN "ammonite-undo"

#include <config.h>

#include "ammonite-file-undo-manager.h"

#include "ammonite-file-undo-operations.h"
#include "ammonite-trash-monitor.h"


enum
{
    SIGNAL_UNDO_CHANGED,
    NUM_SIGNALS,
};

static guint signals[NUM_SIGNALS] = { 0, };

struct _AmmoniteFileUndoManager
{
    GObject parent_instance;
    AmmoniteFileUndoInfo *info;
    AmmoniteFileUndoManagerState state;
    AmmoniteFileUndoManagerState last_state;

    guint is_operating : 1;

    gulong trash_signal_id;
};

G_DEFINE_FINAL_TYPE (AmmoniteFileUndoManager, ammonite_file_undo_manager, G_TYPE_OBJECT)

static AmmoniteFileUndoManager *undo_singleton = NULL;

AmmoniteFileUndoManager *
ammonite_file_undo_manager_new (void)
{
    if (undo_singleton != NULL)
    {
        return g_object_ref (undo_singleton);
    }

    undo_singleton = g_object_new (AMMONITE_TYPE_FILE_UNDO_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (undo_singleton), (gpointer) & undo_singleton);

    return undo_singleton;
}

static gboolean
file_undo_manager_clear (AmmoniteFileUndoManager *self)
{
    gboolean ret = self->info != NULL;

    if (self->trash_signal_id != 0)
    {
        g_clear_signal_handler (&self->trash_signal_id, ammonite_trash_monitor_get ());
    }

    g_clear_object (&self->info);
    self->state = AMMONITE_FILE_UNDO_MANAGER_STATE_NONE;

    return ret;
}

static void
trash_state_changed_cb (AmmoniteTrashMonitor *monitor,
                        gboolean              is_empty,
                        gpointer              user_data)
{
    AmmoniteFileUndoManager *self = user_data;

    /* A trash operation cannot be undone if the trash is empty */
    if (is_empty &&
        self->state == AMMONITE_FILE_UNDO_MANAGER_STATE_UNDO &&
        AMMONITE_IS_FILE_UNDO_INFO_TRASH (self->info))
    {
        file_undo_manager_clear (self);
        g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
    }
}

static void
ammonite_file_undo_manager_init (AmmoniteFileUndoManager *self)
{
}

static void
ammonite_file_undo_manager_finalize (GObject *object)
{
    AmmoniteFileUndoManager *self = AMMONITE_FILE_UNDO_MANAGER (object);

    file_undo_manager_clear (self);

    G_OBJECT_CLASS (ammonite_file_undo_manager_parent_class)->finalize (object);
}

static void
ammonite_file_undo_manager_class_init (AmmoniteFileUndoManagerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ammonite_file_undo_manager_finalize;

    signals[SIGNAL_UNDO_CHANGED] =
        g_signal_new ("undo-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static void
undo_info_apply_ready (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
    AmmoniteFileUndoManager *self = user_data;
    AmmoniteFileUndoInfo *info = AMMONITE_FILE_UNDO_INFO (source);
    gboolean success, user_cancel;

    success = ammonite_file_undo_info_apply_finish (info, res, &user_cancel, NULL);

    self->is_operating = FALSE;

    /* just return in case we got another another operation set */
    if ((self->info != NULL) &&
        (self->info != info))
    {
        return;
    }

    if (success)
    {
        if (self->last_state == AMMONITE_FILE_UNDO_MANAGER_STATE_UNDO)
        {
            self->state = AMMONITE_FILE_UNDO_MANAGER_STATE_REDO;
        }
        else if (self->last_state == AMMONITE_FILE_UNDO_MANAGER_STATE_REDO)
        {
            self->state = AMMONITE_FILE_UNDO_MANAGER_STATE_UNDO;
        }

        self->info = g_object_ref (info);
    }
    else if (user_cancel)
    {
        self->state = self->last_state;
        self->info = g_object_ref (info);
    }
    else
    {
        file_undo_manager_clear (self);
    }

    g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
}

static void
do_undo_redo (AmmoniteFileUndoManager        *self,
              GtkWindow                      *parent_window,
              AmmoniteFileOperationsDBusData *dbus_data)
{
    gboolean undo = self->state == AMMONITE_FILE_UNDO_MANAGER_STATE_UNDO;

    self->last_state = self->state;

    self->is_operating = TRUE;
    ammonite_file_undo_info_apply_async (self->info, undo, parent_window,
                                         dbus_data,
                                         undo_info_apply_ready, self);

    /* clear actions while undoing */
    file_undo_manager_clear (self);
    g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
}

void
ammonite_file_undo_manager_redo (GtkWindow                      *parent_window,
                                 AmmoniteFileOperationsDBusData *dbus_data)
{
    if (undo_singleton->state != AMMONITE_FILE_UNDO_MANAGER_STATE_REDO)
    {
        g_warning ("Called redo, but state is %s!", undo_singleton->state == 0 ?
                   "none" : "undo");
        return;
    }

    do_undo_redo (undo_singleton, parent_window, dbus_data);
}

void
ammonite_file_undo_manager_undo (GtkWindow                      *parent_window,
                                 AmmoniteFileOperationsDBusData *dbus_data)
{
    if (undo_singleton->state != AMMONITE_FILE_UNDO_MANAGER_STATE_UNDO)
    {
        g_warning ("Called undo, but state is %s!", undo_singleton->state == 0 ?
                   "none" : "redo");
        return;
    }

    do_undo_redo (undo_singleton, parent_window, dbus_data);
}

void
ammonite_file_undo_manager_set_action (AmmoniteFileUndoInfo *info)
{
    gboolean changed;

    g_debug ("Setting undo information %p", info);

    changed = file_undo_manager_clear (undo_singleton);

    if (info != NULL &&
        !ammonite_file_undo_info_is_noop (info))
    {
        changed = TRUE;
        undo_singleton->info = g_object_ref (info);
        undo_singleton->state = AMMONITE_FILE_UNDO_MANAGER_STATE_UNDO;
        undo_singleton->last_state = AMMONITE_FILE_UNDO_MANAGER_STATE_NONE;

        /* Connect to the trash monitor lazily instead of in _init() to
         * avoid mounting the trash backend when Ammonite is only running
         * as a D-Bus service e.g. for the shell search provider. */
        if (AMMONITE_IS_FILE_UNDO_INFO_TRASH (info) &&
            undo_singleton->trash_signal_id == 0)
        {
            undo_singleton->trash_signal_id =
                g_signal_connect (ammonite_trash_monitor_get (),
                                  "trash-state-changed",
                                  G_CALLBACK (trash_state_changed_cb),
                                  undo_singleton);
        }
    }

    if (changed)
    {
        g_signal_emit (undo_singleton, signals[SIGNAL_UNDO_CHANGED], 0);
    }
}

AmmoniteFileUndoInfo *
ammonite_file_undo_manager_get_action (void)
{
    return undo_singleton->info;
}

AmmoniteFileUndoManagerState
ammonite_file_undo_manager_get_state (void)
{
    return undo_singleton->state;
}


gboolean
ammonite_file_undo_manager_is_operating (void)
{
    return undo_singleton->is_operating;
}

AmmoniteFileUndoManager *
ammonite_file_undo_manager_get (void)
{
    return undo_singleton;
}
