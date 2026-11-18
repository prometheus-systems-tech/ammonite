/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */
#define G_LOG_DOMAIN "ammonite-search"

#include <config.h>
#include "ammonite-search-engine-model.h"

#include "ammonite-directory.h"
#include "ammonite-file.h"
#include "ammonite-query.h"
#include "ammonite-search-hit.h"
#include "ammonite-search-provider.h"
#include "ammonite-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

struct _AmmoniteSearchEngineModel
{
    AmmoniteSearchProvider parent_instance;

    AmmoniteDirectory *directory;

    guint finished_id;
};

G_DEFINE_FINAL_TYPE (AmmoniteSearchEngineModel,
                     ammonite_search_engine_model,
                     AMMONITE_TYPE_SEARCH_PROVIDER)

static void
finalize (GObject *object)
{
    AmmoniteSearchEngineModel *model;

    model = AMMONITE_SEARCH_ENGINE_MODEL (object);

    if (model->finished_id != 0)
    {
        g_source_remove (model->finished_id);
        model->finished_id = 0;
    }

    g_clear_object (&model->directory);

    G_OBJECT_CLASS (ammonite_search_engine_model_parent_class)->finalize (object);
}

static gboolean
search_finished (AmmoniteSearchEngineModel *model)
{
    model->finished_id = 0;

    ammonite_search_provider_finished (AMMONITE_SEARCH_PROVIDER (model));

    return G_SOURCE_REMOVE;
}

static void
search_finished_idle (AmmoniteSearchEngineModel *model)
{
    if (model->finished_id != 0)
    {
        return;
    }

    model->finished_id = g_idle_add ((GSourceFunc) search_finished, model);
}

static void
model_directory_ready_cb (AmmoniteDirectory *directory,
                          GList             *list,
                          gpointer           user_data)
{
    AmmoniteSearchEngineModel *model = user_data;
    gchar *uri;
    GList *files, *l;
    AmmoniteFile *file;
    gdouble match;
    gboolean found;
    AmmoniteSearchHit *hit;
    GDateTime *initial_date;
    GDateTime *end_date;
    GPtrArray *date_range;
    AmmoniteQuery *query = ammonite_search_provider_get_query (model);

    files = ammonite_directory_get_file_list (directory);

    for (l = files; l != NULL; l = l->next)
    {
        g_autofree gchar *display_name = NULL;
        g_autoptr (GDateTime) mtime = NULL;
        g_autoptr (GDateTime) atime = NULL;
        g_autoptr (GDateTime) ctime = NULL;

        file = l->data;

        match = ammonite_query_matches_string (query,
                                               ammonite_file_get_display_name (file));
        found = (match > -1);
        if (!found)
        {
            continue;
        }

        const char *mime_type = ammonite_file_get_mime_type (file);

        if (!ammonite_query_matches_mime_type (query, mime_type))
        {
            continue;
        }

        mtime = g_date_time_new_from_unix_local (ammonite_file_get_mtime (file));
        atime = g_date_time_new_from_unix_local (ammonite_file_get_atime (file));
        ctime = g_date_time_new_from_unix_local (ammonite_file_get_btime (file));

        date_range = ammonite_query_get_date_range (query);
        if (found && date_range != NULL)
        {
            AmmoniteSearchTimeType type;
            GDateTime *target_date;

            type = ammonite_query_get_search_type (query);
            initial_date = g_ptr_array_index (date_range, 0);
            end_date = g_ptr_array_index (date_range, 1);

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

            found = ammonite_date_time_is_between_dates (target_date,
                                                         initial_date,
                                                         end_date);
            g_ptr_array_unref (date_range);
        }

        if (found)
        {
            uri = ammonite_file_get_uri (file);
            hit = ammonite_search_hit_new (uri);
            ammonite_search_hit_set_fts_rank (hit, match);
            ammonite_search_hit_set_modification_time (hit, mtime);
            ammonite_search_hit_set_access_time (hit, atime);
            ammonite_search_hit_set_creation_time (hit, ctime);

            ammonite_search_provider_add_hit (model, hit);

            g_free (uri);
        }
    }

    ammonite_file_list_free (files);

    search_finished (model);
}

static const char *
get_name (AmmoniteSearchProvider *provider)
{
    return "model";
}

static gboolean
should_search (AmmoniteSearchProvider *provider,
               AmmoniteQuery          *query)
{
    g_autoptr (GFile) location = ammonite_query_get_location (query);
    g_autoptr (AmmoniteDirectory) directory = ammonite_directory_get (location);

    return directory != NULL;
}

static void
start_search (AmmoniteSearchProvider *provider)
{
    AmmoniteQuery *query = ammonite_search_provider_get_query (provider);
    AmmoniteSearchEngineModel *model;

    model = AMMONITE_SEARCH_ENGINE_MODEL (provider);

    g_autoptr (GFile) query_location = ammonite_query_get_location (query);
    g_autoptr (AmmoniteDirectory) directory = ammonite_directory_get (query_location);
    g_set_object (&model->directory, directory);

    ammonite_directory_call_when_ready (model->directory,
                                        AMMONITE_ATTRIBUTE_INFO | AMMONITE_ATTRIBUTE_FILE_LIST,
                                        model_directory_ready_cb, model);
}

static void
search_engine_model_stop (AmmoniteSearchProvider *provider)
{
    AmmoniteSearchEngineModel *self = AMMONITE_SEARCH_ENGINE_MODEL (provider);

    ammonite_directory_cancel_callback (self->directory,
                                        model_directory_ready_cb, self);
    search_finished_idle (self);

    g_clear_object (&self->directory);
}

static void
ammonite_search_engine_model_class_init (AmmoniteSearchEngineModelClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    AmmoniteSearchProviderClass *search_provider_class = AMMONITE_SEARCH_PROVIDER_CLASS (class);

    gobject_class->finalize = finalize;
    search_provider_class->get_name = get_name;
    search_provider_class->should_search = should_search;
    search_provider_class->start_search = start_search;
    search_provider_class->stop_search = search_engine_model_stop;
}

static void
ammonite_search_engine_model_init (AmmoniteSearchEngineModel *engine)
{
}

AmmoniteSearchEngineModel *
ammonite_search_engine_model_new (void)
{
    AmmoniteSearchEngineModel *engine;

    engine = g_object_new (AMMONITE_TYPE_SEARCH_ENGINE_MODEL, NULL);

    return engine;
}
