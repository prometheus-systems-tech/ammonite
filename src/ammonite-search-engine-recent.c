/*
 * SPDX-FileCopyrightText: 2018 Canonical Ltd.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */
#define G_LOG_DOMAIN "ammonite-search"

#include <config.h>
#include "ammonite-search-engine-recent.h"

#include "ammonite-query.h"
#include "ammonite-search-hit.h"
#include "ammonite-search-provider.h"
#include "ammonite-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define FILE_ATTRIBS G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
        G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," \
        G_FILE_ATTRIBUTE_ACCESS_CAN_READ "," \
        G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
        G_FILE_ATTRIBUTE_TIME_ACCESS "," \
        G_FILE_ATTRIBUTE_TIME_CREATED

struct _AmmoniteSearchEngineRecent
{
    AmmoniteSearchProvider parent_instance;

    GtkRecentManager *recent_manager;
};

G_DEFINE_FINAL_TYPE (AmmoniteSearchEngineRecent,
                     ammonite_search_engine_recent,
                     AMMONITE_TYPE_SEARCH_PROVIDER)

AmmoniteSearchEngineRecent *
ammonite_search_engine_recent_new (void)
{
    return g_object_new (AMMONITE_TYPE_SEARCH_ENGINE_RECENT, NULL);
}

static void
ammonite_search_engine_recent_finalize (GObject *object)
{
    G_OBJECT_CLASS (ammonite_search_engine_recent_parent_class)->finalize (object);
}

static gboolean
is_file_valid_recursive (AmmoniteSearchEngineRecent  *self,
                         GFile                       *file,
                         gboolean                     show_hidden,
                         GDateTime                  **mtime,
                         GDateTime                  **atime,
                         GDateTime                  **ctime,
                         GError                     **error)
{
    g_autoptr (GFileInfo) file_info = NULL;

    file_info = g_file_query_info (file, FILE_ATTRIBS,
                                   G_FILE_QUERY_INFO_NONE,
                                   ammonite_search_provider_get_cancellable (self), error);
    if (*error != NULL)
    {
        return FALSE;
    }

    if (!g_file_info_get_attribute_boolean (file_info,
                                            G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
        return FALSE;
    }

    if (mtime && atime && ctime)
    {
        *mtime = g_file_info_get_modification_date_time (file_info);
        *atime = g_file_info_get_access_date_time (file_info);
        *ctime = g_file_info_get_creation_date_time (file_info);
    }

    if (!show_hidden)
    {
        gboolean is_hidden;

        is_hidden = g_file_info_get_attribute_boolean (file_info,
                                                       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
                    g_file_info_get_attribute_boolean (file_info,
                                                       G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP);
        if (!is_hidden)
        {
            g_autoptr (GFile) parent = g_file_get_parent (file);

            if (parent)
            {
                return is_file_valid_recursive (self, parent, show_hidden,
                                                NULL, NULL, NULL,
                                                error);
            }
        }
        else
        {
            return FALSE;
        }
    }

    return TRUE;
}

static gpointer
recent_thread_func (AmmoniteSearchEngineRecent *self)
{
    g_autoptr (GPtrArray) date_range = NULL;
    g_autoptr (GFile) query_location = NULL;
    GList *recent_items;
    AmmoniteQuery *query = ammonite_search_provider_get_query (self);
    gboolean show_hidden = ammonite_query_get_show_hidden_files (query);
    GList *l;

    recent_items = gtk_recent_manager_get_items (self->recent_manager);
    date_range = ammonite_query_get_date_range (query);
    query_location = ammonite_query_get_location (query);

    for (l = recent_items; l != NULL; l = l->next)
    {
        GtkRecentInfo *info = l->data;
        g_autoptr (GFile) file = NULL;
        const gchar *uri;
        const gchar *name;
        gdouble rank;

        uri = gtk_recent_info_get_uri (info);
        file = g_file_new_for_uri (uri);

        if (query_location != NULL && !g_file_has_prefix (file, query_location))
        {
            continue;
        }

        if (ammonite_search_provider_should_stop (self))
        {
            break;
        }

        name = gtk_recent_info_get_display_name (info);
        rank = ammonite_query_matches_string (query, name);

        if (rank <= 0)
        {
            g_autofree char *short_name = gtk_recent_info_get_short_name (info);
            rank = ammonite_query_matches_string (query, short_name);
        }

        if (rank > 0)
        {
            AmmoniteSearchHit *hit;
            g_autoptr (GDateTime) mtime = NULL;
            g_autoptr (GDateTime) atime = NULL;
            g_autoptr (GDateTime) ctime = NULL;
            g_autoptr (GError) error = NULL;

            if (!gtk_recent_info_is_local (info))
            {
                continue;
            }

            if (!is_file_valid_recursive (self, file, show_hidden, &mtime, &atime, &ctime, &error))
            {
                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                {
                    break;
                }

                if (error != NULL &&
                    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                {
                    g_debug ("Impossible to read recent file info: %s",
                             error->message);
                }

                continue;
            }

            const char *mime_type = gtk_recent_info_get_mime_type (info);

            if (!ammonite_query_matches_mime_type (query, mime_type))
            {
                continue;
            }

            if (date_range != NULL)
            {
                AmmoniteSearchTimeType type;
                GDateTime *target_date;
                GDateTime *initial_date;
                GDateTime *end_date;

                initial_date = g_ptr_array_index (date_range, 0);
                end_date = g_ptr_array_index (date_range, 1);
                type = ammonite_query_get_search_type (query);

                switch (type)
                {
                    case AMMONITE_SEARCH_TIME_TYPE_LAST_ACCESS:
                    {
                        target_date = atime;
                    }
                    break;

                    case AMMONITE_SEARCH_TIME_TYPE_LAST_MODIFIED:
                    {
                        target_date = mtime;
                    }
                    break;

                    case AMMONITE_SEARCH_TIME_TYPE_CREATED:
                    {
                        target_date = ctime;
                    }
                    break;

                    default:
                    {
                        target_date = NULL;
                    }
                }

                if (!ammonite_date_time_is_between_dates (target_date,
                                                          initial_date,
                                                          end_date))
                {
                    continue;
                }
            }

            hit = ammonite_search_hit_new (uri);
            ammonite_search_hit_set_fts_rank (hit, rank);
            ammonite_search_hit_set_modification_time (hit, mtime);
            ammonite_search_hit_set_access_time (hit, atime);
            ammonite_search_hit_set_creation_time (hit, ctime);

            ammonite_search_provider_add_hit (self, hit);
        }
    }

    g_idle_add_once ((GSourceOnceFunc) ammonite_search_provider_finished, self);

    g_list_free_full (recent_items, (GDestroyNotify) gtk_recent_info_unref);

    return NULL;
}

static const char *
get_name (AmmoniteSearchProvider *provider)
{
    return "recent";
}

static gboolean
run_in_thread (AmmoniteSearchProvider *provider)
{
    return TRUE;
}

static void
start_search (AmmoniteSearchProvider *provider)
{
    AmmoniteSearchEngineRecent *self = AMMONITE_SEARCH_ENGINE_RECENT (provider);
    g_autoptr (GThread) thread = NULL;

    thread = g_thread_new ("ammonite-search-recent", (GThreadFunc) recent_thread_func, self);
}
static void
ammonite_search_engine_recent_class_init (AmmoniteSearchEngineRecentClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    AmmoniteSearchProviderClass *search_provider_class = AMMONITE_SEARCH_PROVIDER_CLASS (klass);

    object_class->finalize = ammonite_search_engine_recent_finalize;
    search_provider_class->get_name = get_name;
    search_provider_class->run_in_thread = run_in_thread;
    search_provider_class->start_search = start_search;
}

static void
ammonite_search_engine_recent_init (AmmoniteSearchEngineRecent *self)
{
    self->recent_manager = gtk_recent_manager_get_default ();
}
