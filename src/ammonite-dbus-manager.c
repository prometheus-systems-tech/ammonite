/*
 * ammonite-dbus-manager: ammonite DBus interface
 *
 * SPDX-FileCopyrightText: 2010, Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */
#define G_LOG_DOMAIN "ammonite-dbus-manager"

#include <config.h>

#include "ammonite-dbus-manager.h"
#include "ammonite-generated2.h"

#include "ammonite-file-operations.h"
#include "ammonite-file-operations-dbus-data.h"
#include "ammonite-file-undo-manager.h"

struct _AmmoniteDBusManager
{
    GObject parent;

    AmmoniteDBusFileOperations2 *file_operations2;
};

G_DEFINE_FINAL_TYPE (AmmoniteDBusManager, ammonite_dbus_manager, G_TYPE_OBJECT);

static void
ammonite_dbus_manager_dispose (GObject *object)
{
    AmmoniteDBusManager *self = (AmmoniteDBusManager *) object;

    if (self->file_operations2)
    {
        g_object_unref (self->file_operations2);
        self->file_operations2 = NULL;
    }

    G_OBJECT_CLASS (ammonite_dbus_manager_parent_class)->dispose (object);
}

static void
undo_redo_on_finished (gpointer user_data)
{
    AmmoniteFileUndoManager *undo_manager = NULL;
    int *handler_id = (int *) user_data;

    undo_manager = ammonite_file_undo_manager_get ();
    g_signal_handler_disconnect (undo_manager, *handler_id);
    g_application_release (g_application_get_default ());
    g_free (handler_id);
}

static void
handle_redo_internal (AmmoniteFileOperationsDBusData *dbus_data)
{
    AmmoniteFileUndoManager *undo_manager = NULL;
    gint *handler_id = g_new0 (int, 1);

    g_application_hold (g_application_get_default ());

    undo_manager = ammonite_file_undo_manager_get ();
    *handler_id = g_signal_connect_swapped (undo_manager, "undo-changed",
                                            G_CALLBACK (undo_redo_on_finished),
                                            handler_id);
    ammonite_file_undo_manager_redo (NULL, dbus_data);
}

static gboolean
handle_redo2 (AmmoniteDBusFileOperations2 *object,
              GDBusMethodInvocation       *invocation,
              GVariant                    *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_redo_internal (dbus_data);

    ammonite_dbus_file_operations2_complete_redo (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
handle_undo_internal (AmmoniteFileOperationsDBusData *dbus_data)
{
    AmmoniteFileUndoManager *undo_manager = NULL;
    gint *handler_id = g_new0 (int, 1);

    g_application_hold (g_application_get_default ());

    undo_manager = ammonite_file_undo_manager_get ();
    *handler_id = g_signal_connect_swapped (undo_manager, "undo-changed",
                                            G_CALLBACK (undo_redo_on_finished),
                                            handler_id);
    ammonite_file_undo_manager_undo (NULL, dbus_data);
}

static gboolean
handle_undo2 (AmmoniteDBusFileOperations2 *object,
              GDBusMethodInvocation       *invocation,
              GVariant                    *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_undo_internal (dbus_data);

    ammonite_dbus_file_operations2_complete_undo (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
create_folder_on_finished (GFile    *new_file,
                           gboolean  success,
                           gpointer  callback_data)
{
    g_application_release (g_application_get_default ());
}

static void
handle_create_folder_internal (const gchar                    *parent_uri,
                               const gchar                    *new_folder_name,
                               AmmoniteFileOperationsDBusData *dbus_data)
{
    g_application_hold (g_application_get_default ());
    ammonite_file_operations_new_folder (NULL, dbus_data,
                                         parent_uri, new_folder_name,
                                         create_folder_on_finished, NULL);
}

static gboolean
handle_create_folder2 (AmmoniteDBusFileOperations2 *object,
                       GDBusMethodInvocation       *invocation,
                       const gchar                 *parent_uri,
                       const gchar                 *new_folder_name,
                       GVariant                    *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_create_folder_internal (parent_uri, new_folder_name, dbus_data);

    ammonite_dbus_file_operations2_complete_create_folder (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
copy_move_on_finished (GHashTable *debutting_uris,
                       gboolean    success,
                       gpointer    callback_data)
{
    g_application_release (g_application_get_default ());
}

static void
handle_copy_uris_internal (const char                     **sources,
                           const char                      *destination,
                           AmmoniteFileOperationsDBusData  *dbus_data)
{
    GList *source_files = NULL;
    gint idx;

    for (idx = 0; sources[idx] != NULL; idx++)
    {
        source_files = g_list_prepend (source_files, g_strdup (sources[idx]));
    }

    g_application_hold (g_application_get_default ());
    ammonite_file_operations_copy_move (source_files, destination,
                                        GDK_ACTION_COPY, NULL, dbus_data,
                                        copy_move_on_finished, NULL);

    g_list_free_full (source_files, g_free);
}

static gboolean
handle_copy_uris2 (AmmoniteDBusFileOperations2  *object,
                   GDBusMethodInvocation        *invocation,
                   const gchar                 **sources,
                   const gchar                  *destination,
                   GVariant                     *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_copy_uris_internal (sources, destination, dbus_data);

    ammonite_dbus_file_operations2_complete_copy_uris (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
handle_move_uris_internal (const char                     **sources,
                           const char                      *destination,
                           AmmoniteFileOperationsDBusData  *dbus_data)
{
    GList *source_files = NULL;
    gint idx;

    for (idx = 0; sources[idx] != NULL; idx++)
    {
        source_files = g_list_prepend (source_files, g_strdup (sources[idx]));
    }

    g_application_hold (g_application_get_default ());
    ammonite_file_operations_copy_move (source_files, destination,
                                        GDK_ACTION_MOVE, NULL, dbus_data,
                                        copy_move_on_finished, NULL);

    g_list_free_full (source_files, g_free);
}

static gboolean
handle_move_uris2 (AmmoniteDBusFileOperations2  *object,
                   GDBusMethodInvocation        *invocation,
                   const gchar                 **sources,
                   const gchar                  *destination,
                   GVariant                     *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_move_uris_internal (sources, destination, dbus_data);

    ammonite_dbus_file_operations2_complete_copy_uris (object, invocation);
    return TRUE; /* invocation was handled */
}

/* FIXME: Needs a callback for maintaining alive the application */
static void
handle_empty_trash_internal (gboolean                        ask_confirmation,
                             AmmoniteFileOperationsDBusData *dbus_data)
{
    ammonite_file_operations_empty_trash (NULL, ask_confirmation, dbus_data);
}

static gboolean
handle_empty_trash2 (AmmoniteDBusFileOperations2 *object,
                     GDBusMethodInvocation       *invocation,
                     gboolean                     ask_confirmation,
                     GVariant                    *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_empty_trash_internal (ask_confirmation, dbus_data);

    ammonite_dbus_file_operations2_complete_empty_trash (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
trash_on_finished (GHashTable *debutting_uris,
                   gboolean    user_cancel,
                   gpointer    callback_data)
{
    g_application_release (g_application_get_default ());
}

static void
handle_trash_uris_internal (const char                     **uris,
                            AmmoniteFileOperationsDBusData  *dbus_data)
{
    g_autolist (GFile) source_files = NULL;
    gint idx;

    for (idx = 0; uris[idx] != NULL; idx++)
    {
        source_files = g_list_prepend (source_files,
                                       g_file_new_for_uri (uris[idx]));
    }

    g_application_hold (g_application_get_default ());
    ammonite_file_operations_trash_or_delete_async (source_files, NULL,
                                                    dbus_data,
                                                    trash_on_finished, NULL);
}

static gboolean
handle_trash_uris2 (AmmoniteDBusFileOperations2  *object,
                    GDBusMethodInvocation        *invocation,
                    const gchar                 **uris,
                    GVariant                     *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_trash_uris_internal (uris, dbus_data);

    ammonite_dbus_file_operations2_complete_trash_uris (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
delete_on_finished (GHashTable *debutting_uris,
                    gboolean    user_cancel,
                    gpointer    callback_data)
{
    g_application_release (g_application_get_default ());
}

static void
handle_delete_uris_internal (const char                     **uris,
                             AmmoniteFileOperationsDBusData  *dbus_data)
{
    g_autolist (GFile) source_files = NULL;
    gint idx;

    for (idx = 0; uris[idx] != NULL; idx++)
    {
        source_files = g_list_prepend (source_files,
                                       g_file_new_for_uri (uris[idx]));
    }

    g_application_hold (g_application_get_default ());
    ammonite_file_operations_delete_async (source_files, NULL,
                                           dbus_data,
                                           delete_on_finished, NULL);
}

static gboolean
handle_delete_uris2 (AmmoniteDBusFileOperations2  *object,
                     GDBusMethodInvocation        *invocation,
                     const gchar                 **uris,
                     GVariant                     *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_delete_uris_internal (uris, dbus_data);

    ammonite_dbus_file_operations2_complete_delete_uris (object, invocation);
    return TRUE; /* invocation was handled */
}

static void
rename_file_on_finished (GFile    *location,
                         gboolean  success,
                         gpointer  callback_data)
{
    g_application_release (g_application_get_default ());
}

static void
handle_rename_uri_internal (const gchar                    *uri,
                            const gchar                    *new_name,
                            AmmoniteFileOperationsDBusData *dbus_data)
{
    g_autoptr (GFile) location = g_file_new_for_uri (uri);

    g_application_hold (g_application_get_default ());
    ammonite_file_operations_rename (location,
                                     new_name,
                                     NULL,
                                     dbus_data,
                                     rename_file_on_finished,
                                     NULL);
}

static gboolean
handle_rename_uri2 (AmmoniteDBusFileOperations2 *object,
                    GDBusMethodInvocation       *invocation,
                    const gchar                 *uri,
                    const gchar                 *new_name,
                    GVariant                    *platform_data)
{
    g_autoptr (AmmoniteFileOperationsDBusData) dbus_data = NULL;

    dbus_data = ammonite_file_operations_dbus_data_new (platform_data);

    handle_rename_uri_internal (uri, new_name, dbus_data);

    ammonite_dbus_file_operations2_complete_rename_uri (object, invocation);

    return TRUE; /* invocation was handled */
}

static void
undo_manager_changed (AmmoniteDBusManager *self)
{
    AmmoniteFileUndoManagerState undo_state;

    undo_state = ammonite_file_undo_manager_get_state ();
    ammonite_dbus_file_operations2_set_undo_status (self->file_operations2,
                                                    undo_state);
}

static void
ammonite_dbus_manager_init (AmmoniteDBusManager *self)
{
    self->file_operations2 = ammonite_dbus_file_operations2_skeleton_new ();

    g_signal_connect (self->file_operations2,
                      "handle-copy-uris",
                      G_CALLBACK (handle_copy_uris2),
                      self);
    g_signal_connect (self->file_operations2,
                      "handle-move-uris",
                      G_CALLBACK (handle_move_uris2),
                      self);
    g_signal_connect (self->file_operations2,
                      "handle-empty-trash",
                      G_CALLBACK (handle_empty_trash2),
                      self);
    g_signal_connect (self->file_operations2,
                      "handle-trash-uris",
                      G_CALLBACK (handle_trash_uris2),
                      self);
    g_signal_connect (self->file_operations2,
                      "handle-delete-uris",
                      G_CALLBACK (handle_delete_uris2),
                      self);
    g_signal_connect (self->file_operations2,
                      "handle-create-folder",
                      G_CALLBACK (handle_create_folder2),
                      self);
    g_signal_connect (self->file_operations2,
                      "handle-rename-uri",
                      G_CALLBACK (handle_rename_uri2),
                      self);
    g_signal_connect (self->file_operations2,
                      "handle-undo",
                      G_CALLBACK (handle_undo2),
                      self);
    g_signal_connect (self->file_operations2,
                      "handle-redo",
                      G_CALLBACK (handle_redo2),
                      self);
}

static void
ammonite_dbus_manager_class_init (AmmoniteDBusManagerClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose = ammonite_dbus_manager_dispose;
}

AmmoniteDBusManager *
ammonite_dbus_manager_new (void)
{
    return g_object_new (AMMONITE_TYPE_DBUS_MANAGER, NULL);
}

gboolean
ammonite_dbus_manager_register (AmmoniteDBusManager  *self,
                                GDBusConnection      *connection,
                                GError              **error)
{
    gboolean success = TRUE;

    success &= g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->file_operations2),
                                                 connection,
                                                 "/org/ammonite/Ammonite" PROFILE "/FileOperations2",
                                                 error);

    if (success)
    {
        g_signal_connect_object (ammonite_file_undo_manager_get (),
                                 "undo-changed",
                                 G_CALLBACK (undo_manager_changed),
                                 self,
                                 G_CONNECT_SWAPPED);

        undo_manager_changed (self);
    }

    return success;
}

void
ammonite_dbus_manager_unregister (AmmoniteDBusManager *self)
{
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->file_operations2));

    g_signal_handlers_disconnect_by_data (ammonite_file_undo_manager_get (), self);
}
