/*
 * SPDX-FileCopyrightText: 2005 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */
#define G_LOG_DOMAIN "ammonite-search"

#include <config.h>
#include "ammonite-search-engine-simple.h"

#include "ammonite-file-utilities.h"
#include "ammonite-query.h"
#include "ammonite-search-hit.h"
#include "ammonite-search-provider.h"
#include "ammonite-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define FLUSH_TIME_SPAN (250 * G_TIME_SPAN_MILLISECOND)

struct _AmmoniteSearchEngineSimple
{
    AmmoniteSearchProvider parent_instance;

    gint64 last_saved_time;
};

G_DEFINE_FINAL_TYPE (AmmoniteSearchEngineSimple,
                     ammonite_search_engine_simple,
                     AMMONITE_TYPE_SEARCH_PROVIDER)

#define STD_ATTRIBUTES \
        G_FILE_ATTRIBUTE_STANDARD_NAME "," \
        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
        G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," \
        G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
        G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
        G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
        G_FILE_ATTRIBUTE_TIME_ACCESS "," \
        G_FILE_ATTRIBUTE_TIME_CREATED "," \
        G_FILE_ATTRIBUTE_ID_FILE

#define STD_ATTRIBUTES_WITH_CONTENT_TYPE \
        STD_ATTRIBUTES "," \
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
        G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE

static void
iterate_file (GFileInfo                  *info,
              GFile                      *location,
              AmmoniteSearchEngineSimple *self)
{
    AmmoniteQuery *query = ammonite_search_provider_get_query (self);

    AmmoniteSearchTimeType type = ammonite_query_get_search_type (query);
    g_autoptr (GPtrArray) date_range = ammonite_query_get_date_range (query);
    gboolean show_hidden = ammonite_query_get_show_hidden_files (query);
    const char *display_name = g_file_info_get_display_name (info);

    if (display_name == NULL)
    {
        return;
    }

    if (!show_hidden &&
        (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
         g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP)))
    {
        return;
    }

    gdouble match = ammonite_query_matches_string (query, display_name);
    gboolean found = (match > -1);
    gint64 current_time;

    if (found && ammonite_query_has_mime_types (query))
    {
        const char *mime_type = g_file_info_get_attribute_string (
            info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
        if (mime_type == NULL)
        {
            mime_type = g_file_info_get_attribute_string (info,
                                                          G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
        }

        found = ammonite_query_matches_mime_type (query, mime_type);
    }

    g_autoptr (GDateTime) mtime = g_file_info_get_modification_date_time (info);
    g_autoptr (GDateTime) atime = g_file_info_get_access_date_time (info);
    g_autoptr (GDateTime) ctime = g_file_info_get_creation_date_time (info);

    if (found && date_range != NULL)
    {
        GDateTime *target_date;
        GDateTime *initial_date = g_ptr_array_index (date_range, 0);
        GDateTime *end_date = g_ptr_array_index (date_range, 1);

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
    }

    if (found)
    {
        g_autofree gchar *uri = g_file_get_uri (location);
        AmmoniteSearchHit *hit = ammonite_search_hit_new (uri);

        ammonite_search_hit_set_fts_rank (hit, match);
        ammonite_search_hit_set_modification_time (hit, mtime);
        ammonite_search_hit_set_access_time (hit, atime);
        ammonite_search_hit_set_creation_time (hit, ctime);

        ammonite_search_provider_add_hit (self, hit);
    }

    current_time = g_get_monotonic_time ();
    if (current_time - self->last_saved_time >= FLUSH_TIME_SPAN)
    {
        self->last_saved_time = current_time;
        ammonite_search_provider_flush_hits (self);
    }
}

static void
iterate_finish (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
    AmmoniteSearchEngineSimple *self = user_data;

    g_idle_add_once ((GSourceOnceFunc) ammonite_search_provider_finished, self);
}

static const char *
get_name (AmmoniteSearchProvider *provider)
{
    return "simple";
}

static gboolean
run_in_thread (AmmoniteSearchProvider *provider)
{
    return TRUE;
}

static guint
search_delay (AmmoniteSearchProvider *provider)
{
    return 500;
}

static gboolean
should_search (AmmoniteSearchProvider *provider,
               AmmoniteQuery          *query)
{
    g_autoptr (GFile) location = ammonite_query_get_location (query);

    return location != NULL;
}

static void
start_search (AmmoniteSearchProvider *provider)
{
    AmmoniteSearchEngineSimple *self = AMMONITE_SEARCH_ENGINE_SIMPLE (provider);

    GCancellable *cancellable = ammonite_search_provider_get_cancellable (self);
    AmmoniteQuery *query = ammonite_search_provider_get_query (self);
    g_autoptr (GFile) toplevel = ammonite_query_get_location (query);
    gboolean local_only = ammonite_query_recursive_local_only (query);
    const char *attributes = ammonite_query_has_mime_types (query)
                             ? STD_ATTRIBUTES_WITH_CONTENT_TYPE
                             : STD_ATTRIBUTES;
    gboolean recursion_enabled = ammonite_query_recursive (query);

    ammonite_iterate_directory_recursive (toplevel,
                                          recursion_enabled ? G_MAXUINT : 0,
                                          attributes,
                                          local_only,
                                          cancellable,
                                          (IterationFileCallback) iterate_file,
                                          iterate_finish,
                                          self);
}

static void
ammonite_search_engine_simple_class_init (AmmoniteSearchEngineSimpleClass *class)
{
    AmmoniteSearchProviderClass *search_provider_class = AMMONITE_SEARCH_PROVIDER_CLASS (class);

    search_provider_class->get_name = get_name;
    search_provider_class->run_in_thread = run_in_thread;
    search_provider_class->search_delay = search_delay;
    search_provider_class->should_search = should_search;
    search_provider_class->start_search = start_search;
}

static void
ammonite_search_engine_simple_init (AmmoniteSearchEngineSimple *self)
{
    self->last_saved_time = g_get_monotonic_time ();
}

AmmoniteSearchEngineSimple *
ammonite_search_engine_simple_new (void)
{
    AmmoniteSearchEngineSimple *engine;

    engine = g_object_new (AMMONITE_TYPE_SEARCH_ENGINE_SIMPLE, NULL);

    return engine;
}
