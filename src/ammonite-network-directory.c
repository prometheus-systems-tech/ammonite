/*
 * SPDX-FileCopyrightText: 2024 António Fernandes <antoniof@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-network-directory.h"

#include <gio/gio.h>

#include "ammonite-directory-private.h"
#include "ammonite-file-private.h"
#include "ammonite-file-utilities.h"
#include "ammonite-internal-place-file.h"
#include "ammonite-recent-servers.h"
#include "ammonite-scheme.h"


struct _AmmoniteNetworkDirectory
{
    AmmoniteDirectory parent_slot;

    AmmoniteDirectory *computer_backend_directory;
    gboolean computer_backend_done_loading;

    AmmoniteDirectory *network_backend_directory;
    gboolean network_backend_done_loading;

    AmmoniteRecentServers *recent_servers;
    GList *recent_server_files;
    gboolean recent_servers_done_loading;

    GList /*<owned NetworkCallback>*/ *callback_list;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (AmmoniteNetworkDirectory, ammonite_network_directory, AMMONITE_TYPE_DIRECTORY,
                               ammonite_ensure_extension_points ();
                               g_io_extension_point_implement (AMMONITE_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                               g_define_type_id,
                                                               AMMONITE_NETWORK_DIRECTORY_PROVIDER_NAME,
                                                               0));

typedef struct
{
    AmmoniteNetworkDirectory *self;

    AmmoniteDirectoryCallback callback;
    gpointer callback_data;

    gboolean computer_backend_ready;
    gboolean network_backend_ready;
} NetworkCallback;

static gboolean
mountable_file_is_remote (AmmoniteFile *file)
{
    g_autoptr (GIcon) icon = ammonite_file_get_gicon (file, AMMONITE_FILE_ICON_FLAGS_NONE);

    /* HACK: It would be nice to have a "mountable::remote" attribute or
     * something like that. Until then, rely on icon name to guess. */
    if (G_IS_THEMED_ICON (icon))
    {
        const gchar * const *names = g_themed_icon_get_names (G_THEMED_ICON (icon));

        for (gsize i = 0; names[i] != NULL; i++)
        {
            if (strstr (names[i], "network") || strstr (names[i], "remote"))
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static GList *
get_remote_mountables (GList *mountable_files)
{
    GList *remotes = NULL;

    for (GList *l = mountable_files; l != NULL; l = l->next)
    {
        AmmoniteFile *file = AMMONITE_FILE (l->data);

        if (mountable_file_is_remote (file))
        {
            remotes = g_list_prepend (remotes, g_object_ref (file));
        }
    }

    return remotes;
}

static gboolean
real_are_all_files_seen (AmmoniteDirectory *directory)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);

    return (ammonite_directory_are_all_files_seen (self->computer_backend_directory) &&
            ammonite_directory_are_all_files_seen (self->network_backend_directory) &&
            self->recent_servers_done_loading);
}

static gboolean
real_contains_file (AmmoniteDirectory *directory,
                    AmmoniteFile      *file)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);

    if (ammonite_file_get_directory (file) == directory)
    {
        /* Recent server files are directly owned by AmmoniteNetworkDirectory. */
        return TRUE;
    }
    if (ammonite_directory_contains_file (self->network_backend_directory, file))
    {
        return TRUE;
    }
    if (ammonite_directory_contains_file (self->computer_backend_directory, file))
    {
        return mountable_file_is_remote (file);
    }

    return FALSE;
}

static void
on_backend_directory_done_loading (AmmoniteDirectory *backend_directory,
                                   gpointer           callback_data)
{
    AmmoniteNetworkDirectory *self = callback_data;

    if (backend_directory == self->computer_backend_directory)
    {
        self->computer_backend_done_loading = TRUE;
    }
    else if (backend_directory == self->network_backend_directory)
    {
        self->network_backend_done_loading = TRUE;
    }
    else
    {
        /* Called from on_recent_servers_loading_changed () */
        g_assert (backend_directory == (AmmoniteDirectory *) self &&
                  self->recent_servers_done_loading);
    }

    if (self->computer_backend_done_loading &&
        self->network_backend_done_loading &&
        self->recent_servers_done_loading)
    {
        ammonite_directory_emit_done_loading (AMMONITE_DIRECTORY (self));
    }
}

static void
real_force_reload (AmmoniteDirectory *directory)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);

    self->computer_backend_done_loading = FALSE;
    ammonite_directory_force_reload (self->computer_backend_directory);

    self->network_backend_done_loading = FALSE;
    ammonite_directory_force_reload (self->network_backend_directory);

    self->recent_servers_done_loading = FALSE;
    ammonite_recent_servers_force_reload (self->recent_servers);
}

static void
on_backend_directory_ready (AmmoniteDirectory *backend_directory,
                            GList             *unused_parameter,
                            gpointer           callback_data)
{
    NetworkCallback *network_callback = callback_data;
    AmmoniteNetworkDirectory *self = network_callback->self;

    if (backend_directory == self->computer_backend_directory)
    {
        network_callback->computer_backend_ready = TRUE;
    }
    else if (backend_directory == self->network_backend_directory)
    {
        network_callback->network_backend_ready = TRUE;
    }
    else
    {
        g_assert (backend_directory == (AmmoniteDirectory *) self &&
                  network_callback->self->recent_servers_done_loading);
    }

    if (network_callback->computer_backend_ready &&
        network_callback->network_backend_ready &&
        network_callback->self->recent_servers_done_loading)
    {
        g_autolist (AmmoniteFile) files = ammonite_directory_get_file_list (AMMONITE_DIRECTORY (self));

        /* Remove from list before calling to avoid double removal */
        self->callback_list = g_list_remove (self->callback_list, network_callback);

        /* Invoke ready callback */
        (*network_callback->callback)(AMMONITE_DIRECTORY (self), files, network_callback->callback_data);

        g_free (network_callback);
    }
}

static void
on_recent_servers_loading_changed (GObject    *object,
                                   GParamSpec *pspec,
                                   gpointer    user_data)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (user_data);
    AmmoniteRecentServers *recent_servers = AMMONITE_RECENT_SERVERS (object);
    gboolean is_loading = ammonite_recent_servers_get_loading (recent_servers);

    self->recent_servers_done_loading = !is_loading;
    if (self->recent_servers_done_loading)
    {
        AmmoniteDirectory *self_as_directory = AMMONITE_DIRECTORY (self);

        on_backend_directory_done_loading (self_as_directory, self);

        for (GList *l = self->callback_list, *next; l != NULL; l = next)
        {
            next = l->next;
            on_backend_directory_ready (self_as_directory, NULL, l->data);
        }
    }
}

static void
on_computer_backend_directory_files_added (AmmoniteNetworkDirectory *self,
                                           GList                    *added_files)
{
    g_autolist (AmmoniteFile) files = get_remote_mountables (added_files);

    if (files == NULL)
    {
        return;
    }

    ammonite_directory_emit_files_added (AMMONITE_DIRECTORY (self), files);
}

static void
on_computer_backend_directory_files_changed (AmmoniteNetworkDirectory *self,
                                             GList                    *changed_files)
{
    g_autolist (AmmoniteFile) files = get_remote_mountables (changed_files);

    if (files == NULL)
    {
        return;
    }

    ammonite_directory_emit_files_changed (AMMONITE_DIRECTORY (self), files);
}

static AmmoniteFile *
get_recent_server_file (AmmoniteDirectory *directory,
                        GFileInfo         *server_info)
{
    g_autofree char *uri = g_strconcat (SCHEME_NETWORK_VIEW ":///",
                                        g_file_info_get_name (server_info),
                                        NULL);
    g_autoptr (AmmoniteFile) file = ammonite_file_get_by_uri (uri);

    ammonite_file_update_info (file, server_info);

    return g_steal_pointer (&file);
}

static void
on_recent_servers_added (AmmoniteNetworkDirectory *self,
                         GList                    *servers)
{
    AmmoniteDirectory *dir = AMMONITE_DIRECTORY (self);
    g_autolist (AmmoniteFile) added_files = NULL;

    for (GList *l = servers; l != NULL; l = l->next)
    {
        GFileInfo *server_info = l->data;
        g_autoptr (AmmoniteFile) file = get_recent_server_file (dir, server_info);

        self->recent_server_files = g_list_prepend (self->recent_server_files,
                                                    g_object_ref (file));

        added_files = g_list_prepend (added_files, g_steal_pointer (&file));
    }

    ammonite_directory_emit_files_added (dir, added_files);
}

static void
on_recent_servers_changed (AmmoniteNetworkDirectory *self,
                           GList                    *servers)
{
    g_autolist (AmmoniteFile) changed_files = NULL;

    for (GList *l = servers; l != NULL; l = l->next)
    {
        GFileInfo *server_info = l->data;
        AmmoniteFile *file = ammonite_directory_find_file_by_name (AMMONITE_DIRECTORY (self),
                                                                   g_file_info_get_name (server_info));
        if (file == NULL)
        {
            g_critical ("Notified change on recent server whose random GUID name was not known yet");
            continue;
        }

        ammonite_file_update_info (file, server_info);

        changed_files = g_list_prepend (changed_files, g_object_ref (file));
    }

    ammonite_directory_emit_files_changed (AMMONITE_DIRECTORY (self), changed_files);
}

static void
on_recent_servers_removed (AmmoniteNetworkDirectory *self,
                           GList                    *servers)
{
    g_autolist (AmmoniteFile) removed_files = NULL;

    for (GList *l = servers; l != NULL; l = l->next)
    {
        GFileInfo *server_info = l->data;
        AmmoniteFile *file = ammonite_directory_find_file_by_name (AMMONITE_DIRECTORY (self),
                                                                   g_file_info_get_name (server_info));
        if (file == NULL)
        {
            g_critical ("Notified removal of recent server whose random GUID name was not known yet");
            continue;
        }

        ammonite_file_mark_gone (file);

        /* Steal file from self->recent_server_files */
        GList *link = g_list_find (self->recent_server_files, file);
        self->recent_server_files = g_list_remove_link (self->recent_server_files, link);
        removed_files = g_list_concat (link, removed_files);
    }

    ammonite_directory_emit_files_changed (AMMONITE_DIRECTORY (self), removed_files);
}

static NetworkCallback *
network_callback_find (AmmoniteNetworkDirectory  *self,
                       AmmoniteDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    for (GList *l = self->callback_list; l != NULL; l = l->next)
    {
        NetworkCallback *network_callback = l->data;

        if (network_callback->callback == callback &&
            network_callback->callback_data == callback_data)
        {
            return network_callback;
        }
    }
    return NULL;
}

static void
real_call_when_ready (AmmoniteDirectory         *directory,
                      AmmoniteAttributes         attributes,
                      AmmoniteDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);
    NetworkCallback *network_callback;

    network_callback = network_callback_find (self, callback, callback_data);
    if (network_callback != NULL)
    {
        g_warning ("tried to add a new callback while an old one was pending");
        return;
    }

    network_callback = g_new0 (NetworkCallback, 1);
    network_callback->self = self;
    network_callback->callback = callback;
    network_callback->callback_data = callback_data;

    self->callback_list = g_list_prepend (self->callback_list, network_callback);

    ammonite_directory_call_when_ready (self->computer_backend_directory,
                                        attributes,
                                        on_backend_directory_ready, network_callback);
    ammonite_directory_call_when_ready (self->network_backend_directory,
                                        attributes,
                                        on_backend_directory_ready, network_callback);
}

static void
real_cancel_callback (AmmoniteDirectory         *directory,
                      AmmoniteDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);
    NetworkCallback *network_callback;

    network_callback = network_callback_find (self, callback, callback_data);
    if (network_callback == NULL)
    {
        /* No warning needed, because ammonite_directory_cancel_callback() is
         * meant to be used unconditionally as cleanup. */
        return;
    }

    if (!network_callback->computer_backend_ready)
    {
        ammonite_directory_cancel_callback (self->computer_backend_directory, on_backend_directory_ready, network_callback);
    }
    if (!network_callback->network_backend_ready)
    {
        ammonite_directory_cancel_callback (self->network_backend_directory, on_backend_directory_ready, network_callback);
    }

    self->callback_list = g_list_remove (self->callback_list, network_callback);
    g_free (network_callback);
}

static void
real_file_monitor_add (AmmoniteDirectory         *directory,
                       gconstpointer              client,
                       gboolean                   monitor_hidden_files,
                       AmmoniteAttributes         attributes,
                       AmmoniteDirectoryCallback  callback,
                       gpointer                   callback_data)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);

    ammonite_directory_file_monitor_add (self->computer_backend_directory,
                                         client,
                                         monitor_hidden_files,
                                         attributes,
                                         NULL, NULL);
    ammonite_directory_file_monitor_add (self->network_backend_directory,
                                         client,
                                         monitor_hidden_files,
                                         attributes,
                                         NULL, NULL);

    if (callback != NULL)
    {
        g_autolist (AmmoniteFile) files = ammonite_directory_get_file_list (directory);

        (*callback)(directory, files, callback_data);
    }
}

static void
real_file_monitor_remove (AmmoniteDirectory *directory,
                          gconstpointer      client)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);

    ammonite_directory_file_monitor_remove (self->computer_backend_directory, client);
    ammonite_directory_file_monitor_remove (self->network_backend_directory, client);
}

static GList *
real_get_file_list (AmmoniteDirectory *directory)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);
    g_autolist (AmmoniteFile) computer_list = ammonite_directory_get_file_list (self->computer_backend_directory);
    g_autolist (AmmoniteFile) network_list = ammonite_directory_get_file_list (self->network_backend_directory);
    g_autolist (AmmoniteFile) recent_servers = ammonite_file_list_copy (self->recent_server_files);

    return g_list_concat (get_remote_mountables (computer_list),
                          g_list_concat (g_steal_pointer (&recent_servers),
                                         g_steal_pointer (&network_list)));
}

static gboolean
real_is_not_empty (AmmoniteDirectory *directory)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (directory);

    if (self->recent_server_files != NULL ||
        ammonite_directory_is_not_empty (self->network_backend_directory))
    {
        return TRUE;
    }
    if (ammonite_directory_is_not_empty (self->computer_backend_directory))
    {
        g_autolist (AmmoniteFile) computer_list = ammonite_directory_get_file_list (self->computer_backend_directory);
        g_autolist (AmmoniteFile) remote_mountables = get_remote_mountables (computer_list);

        if (remote_mountables != NULL)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
real_is_editable (AmmoniteDirectory *directory)
{
    return FALSE;
}

static gboolean
real_handles_location (GFile *location)
{
    return g_file_has_uri_scheme (location, SCHEME_NETWORK_VIEW);
}

static AmmoniteFile *
real_new_as_file (AmmoniteDirectory *directory)
{
    return g_object_new (AMMONITE_TYPE_INTERNAL_PLACE_FILE, "directory", directory, NULL);
}

static void
ammonite_network_directory_dispose (GObject *object)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (object);

    g_clear_list (&self->recent_server_files, g_object_unref);
    g_clear_list (&self->callback_list, g_free);

    G_OBJECT_CLASS (ammonite_network_directory_parent_class)->dispose (object);
}

static void
ammonite_network_directory_finalize (GObject *object)
{
    AmmoniteNetworkDirectory *self = AMMONITE_NETWORK_DIRECTORY (object);

    g_clear_object (&self->computer_backend_directory);
    g_clear_object (&self->network_backend_directory);
    g_clear_object (&self->recent_servers);

    G_OBJECT_CLASS (ammonite_network_directory_parent_class)->finalize (object);
}

static void
ammonite_network_directory_init (AmmoniteNetworkDirectory *self)
{
    self->computer_backend_directory = ammonite_directory_get_by_uri (SCHEME_COMPUTER ":///");
    g_signal_connect_object (self->computer_backend_directory, "files-added",
                             G_CALLBACK (on_computer_backend_directory_files_added), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->computer_backend_directory, "files-changed",
                             G_CALLBACK (on_computer_backend_directory_files_changed), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->computer_backend_directory, "done-loading",
                             G_CALLBACK (on_backend_directory_done_loading), self, G_CONNECT_DEFAULT);
    g_signal_connect_object (self->computer_backend_directory, "load-error",
                             G_CALLBACK (ammonite_directory_emit_load_error), self, G_CONNECT_SWAPPED);

    self->network_backend_directory = ammonite_directory_get_by_uri (SCHEME_NETWORK ":///");
    g_signal_connect_object (self->network_backend_directory, "files-added",
                             G_CALLBACK (ammonite_directory_emit_files_added), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->network_backend_directory, "files-changed",
                             G_CALLBACK (ammonite_directory_emit_files_changed), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->network_backend_directory, "done-loading",
                             G_CALLBACK (on_backend_directory_done_loading), self, G_CONNECT_DEFAULT);
    g_signal_connect_object (self->network_backend_directory, "load-error",
                             G_CALLBACK (ammonite_directory_emit_load_error), self, G_CONNECT_SWAPPED);

    self->recent_servers = ammonite_recent_servers_new ();
    g_signal_connect_object (self->recent_servers, "notify::loading",
                             G_CALLBACK (on_recent_servers_loading_changed), self, G_CONNECT_DEFAULT);
    g_signal_connect_object (self->recent_servers, "added",
                             G_CALLBACK (on_recent_servers_added), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->recent_servers, "changed",
                             G_CALLBACK (on_recent_servers_changed), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (self->recent_servers, "removed",
                             G_CALLBACK (on_recent_servers_removed), self, G_CONNECT_SWAPPED);
}

static void
ammonite_network_directory_class_init (AmmoniteNetworkDirectoryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    AmmoniteDirectoryClass *directory_class = AMMONITE_DIRECTORY_CLASS (klass);

    object_class->finalize = ammonite_network_directory_finalize;
    object_class->dispose = ammonite_network_directory_dispose;

    directory_class->are_all_files_seen = real_are_all_files_seen;
    directory_class->contains_file = real_contains_file;
    directory_class->force_reload = real_force_reload;
    directory_class->call_when_ready = real_call_when_ready;
    directory_class->cancel_callback = real_cancel_callback;
    directory_class->file_monitor_add = real_file_monitor_add;
    directory_class->file_monitor_remove = real_file_monitor_remove;
    directory_class->get_file_list = real_get_file_list;
    directory_class->is_not_empty = real_is_not_empty;
    directory_class->is_editable = real_is_editable;
    directory_class->handles_location = real_handles_location;
    directory_class->new_as_file = real_new_as_file;
}

AmmoniteNetworkDirectory *
ammonite_network_directory_new (void)
{
    g_autoptr (GFile) location = g_file_new_for_uri (SCHEME_NETWORK_VIEW ":///");

    return g_object_new (AMMONITE_TYPE_NETWORK_DIRECTORY,
                         "location", location,
                         NULL);
}
