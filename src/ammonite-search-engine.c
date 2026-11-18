/*
 * SPDX-FileCopyrightText: 2005 Novell, Inc.
 * SPDX-FileCopyrightText: 2025 The Files contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#define G_LOG_DOMAIN "ammonite-search"

#include <config.h>
#include "ammonite-search-engine.h"

#include "ammonite-query.h"
#include "ammonite-search-engine-model.h"
#include "ammonite-search-engine-localsearch.h"
#include "ammonite-search-engine-recent.h"
#include "ammonite-search-engine-simple.h"
#include "ammonite-search-hit.h"
#include "ammonite-search-provider.h"


struct _AmmoniteSearchEngine
{
    GObject parent_instance;

    AmmoniteSearchType search_type;

    AmmoniteSearchProvider *localsearch;
    AmmoniteSearchProvider *model;
    AmmoniteSearchProvider *recent;
    AmmoniteSearchProvider *simple;

    GHashTable *uris;
    guint providers_started;
    guint providers_finished;

    AmmoniteQuery *query;
    gboolean running;
    gboolean starting;
    gboolean restart;
};

enum
{
    PROP_0,
    PROP_RUNNING,
    PROP_SEARCH_TYPE,
    N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES];

enum
{
    HITS_ADDED,
    SEARCH_FINISHED,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (AmmoniteSearchEngine, ammonite_search_engine, G_TYPE_OBJECT)

static void
check_providers_status (AmmoniteSearchEngine *self);

static void
search_engine_start_provider (AmmoniteSearchProvider *provider,
                              AmmoniteSearchEngine   *self)
{
    if (provider == NULL)
    {
        return;
    }
    else if (ammonite_search_provider_start (provider, self->query))
    {
        self->providers_started++;
    }
}

static void
search_engine_start_real (AmmoniteSearchEngine *self)
{
    g_return_if_fail (self->running);

    self->providers_started = 0;
    self->providers_finished = 0;

    self->starting = TRUE;
    search_engine_start_provider (self->localsearch, self);
    search_engine_start_provider (self->model, self);
    search_engine_start_provider (self->recent, self);
    search_engine_start_provider (self->simple, self);
    self->starting = FALSE;

    /* Providers could already be finished */
    check_providers_status (self);
}

void
ammonite_search_engine_start (AmmoniteSearchEngine *self,
                              AmmoniteQuery        *query)
{
    g_return_if_fail (query != NULL);

    g_autoptr (AmmoniteQuery) query_to_copy = g_object_ref (query);

    g_clear_object (&self->query);
    self->query = ammonite_query_copy (query_to_copy);

    if (self->running)
    {
        self->restart = TRUE;
        return;
    }

    /* Keep reference on self while running */
    g_object_ref (self);
    self->running = TRUE;
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RUNNING]);

    g_debug ("Search engine start");
    search_engine_start_real (self);
}

void
ammonite_search_engine_stop (AmmoniteSearchEngine *self)
{
    g_debug ("Search engine stop");

    if (self->localsearch != NULL)
    {
        ammonite_search_provider_stop (self->localsearch);
    }
    if (self->model != NULL)
    {
        ammonite_search_provider_stop (self->model);
    }
    if (self->recent != NULL)
    {
        ammonite_search_provider_stop (self->recent);
    }
    if (self->simple != NULL)
    {
        ammonite_search_provider_stop (self->simple);
    }

    self->restart = FALSE;
}

static void
search_provider_hits_added (AmmoniteSearchProvider *provider,
                            GPtrArray              *transferred_hits,
                            AmmoniteSearchEngine   *self)
{
    g_autoptr (GPtrArray) hits = transferred_hits;

    if (!self->running || self->restart)
    {
        g_debug ("Ignoring hits-added, since engine is %s",
                 !self->running ? "not running" : "waiting to restart");
        return;
    }

    g_autoptr (GPtrArray) added = g_ptr_array_new_with_free_func (g_object_unref);
    for (guint i = 0; i < hits->len; i++)
    {
        AmmoniteSearchHit *hit = hits->pdata[i];
        const char *uri = ammonite_search_hit_get_uri (hit);

        if (!g_hash_table_contains (self->uris, uri))
        {
            g_hash_table_add (self->uris, g_strdup (uri));
            g_ptr_array_add (added, g_object_ref (hit));
        }
    }

    if (added->len > 0)
    {
        g_signal_emit (self, signals[HITS_ADDED], 0, added);
    }
}

static void
check_providers_status (AmmoniteSearchEngine *self)
{
    g_assert (self->running);

    if (self->starting || self->providers_finished < self->providers_started)
    {
        return;
    }

    if (self->restart)
    {
        g_debug ("Search engine finished and restarting");
    }
    else
    {
        g_debug ("Search engine finished");

        g_signal_emit (self, signals[SEARCH_FINISHED], 0);
    }

    g_hash_table_remove_all (self->uris);

    if (self->restart)
    {
        self->restart = FALSE;
        g_debug ("Search engine restarting");
        search_engine_start_real (self);
    }
    else
    {
        self->running = FALSE;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RUNNING]);

        g_object_unref (self);
    }
}

static void
search_provider_finished (AmmoniteSearchEngine *self)
{
    self->providers_finished++;

    check_providers_status (self);
}

typedef AmmoniteSearchProvider *(* CreateFunc) (void);

static void
setup_provider (AmmoniteSearchEngine    *self,
                AmmoniteSearchProvider **provider_pointer,
                AmmoniteSearchType       provider_flag,
                CreateFunc               create_func)
{
    if (self->search_type & provider_flag)
    {
        if (*provider_pointer == NULL)
        {
            *provider_pointer = create_func ();

            g_signal_connect (*provider_pointer, "hits-added",
                              G_CALLBACK (search_provider_hits_added),
                              self);
            g_signal_connect_swapped (*provider_pointer, "provider-finished",
                                      G_CALLBACK (search_provider_finished),
                                      self);
        }
    }
    else
    {
        g_clear_object (provider_pointer);
    }
}

void
ammonite_search_engine_set_search_type (AmmoniteSearchEngine *self,
                                        AmmoniteSearchType    search_type)
{
    if (self->search_type == search_type)
    {
        return;
    }

    self->search_type = search_type;

    setup_provider (self, &self->localsearch, AMMONITE_SEARCH_TYPE_LOCALSEARCH,
                    (CreateFunc) ammonite_search_engine_localsearch_new);
    setup_provider (self, &self->model, AMMONITE_SEARCH_TYPE_MODEL,
                    (CreateFunc) ammonite_search_engine_model_new);
    setup_provider (self, &self->recent, AMMONITE_SEARCH_TYPE_RECENT,
                    (CreateFunc) ammonite_search_engine_recent_new);
    setup_provider (self, &self->simple, AMMONITE_SEARCH_TYPE_SIMPLE,
                    (CreateFunc) ammonite_search_engine_simple_new);
}

static void
ammonite_search_engine_finalize (GObject *object)
{
    AmmoniteSearchEngine *self = AMMONITE_SEARCH_ENGINE (object);

    g_hash_table_destroy (self->uris);

    g_clear_object (&self->localsearch);
    g_clear_object (&self->recent);
    g_clear_object (&self->model);
    g_clear_object (&self->simple);
    g_clear_object (&self->query);

    G_OBJECT_CLASS (ammonite_search_engine_parent_class)->finalize (object);
}

static void
ammonite_search_engine_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    AmmoniteSearchEngine *self = AMMONITE_SEARCH_ENGINE (object);

    switch (prop_id)
    {
        case PROP_RUNNING:
        {
            g_value_set_boolean (value, self->running);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
search_engine_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    AmmoniteSearchEngine *self = AMMONITE_SEARCH_ENGINE (object);

    switch (prop_id)
    {
        case PROP_SEARCH_TYPE:
        {
            AmmoniteSearchType search_type = g_value_get_int (value);
            ammonite_search_engine_set_search_type (self, search_type);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_search_engine_class_init (AmmoniteSearchEngineClass *class)
{
    GObjectClass *object_class;

    object_class = (GObjectClass *) class;

    object_class->finalize = ammonite_search_engine_finalize;
    object_class->get_property = ammonite_search_engine_get_property;
    object_class->set_property = search_engine_set_property;

    /**
     * AmmoniteSearchEngine::running:
     *
     * Whether the search engine is running a search.
     */
    properties[PROP_RUNNING] =
        g_param_spec_boolean ("running",
                              "search running",
                              "Whether the engine is running a search",
                              FALSE,
                              G_PARAM_READABLE);
    properties[PROP_SEARCH_TYPE] =
        g_param_spec_int ("search-type",
                          "search type",
                          "a #AmmoniteSearchType",
                          0, G_MAXINT, 0,
                          G_PARAM_WRITABLE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    /**
     * AmmoniteSearchEngine::hits-added:
     *
     * @engine: The engine which emitted the signal.
     * @hits: (transfer none): #GPtrArray of #AmmoniteSearchHit
     *
     * Emitted when search hits are found.
     */
    signals[HITS_ADDED] = g_signal_new (
        "hits-added", G_TYPE_FROM_CLASS (object_class),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1, G_TYPE_POINTER);

    /**
     * AmmoniteSearchEngine::search-finished:
     *
     * @engine: The engine which emitted the signal.
     *
     * Emitted when the search finishes. Not emitted in-between restarts.
     */
    signals[SEARCH_FINISHED] = g_signal_new (
        "search-finished", G_TYPE_FROM_CLASS (object_class),
        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static void
ammonite_search_engine_init (AmmoniteSearchEngine *self)
{
    self->uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

AmmoniteSearchEngine *
ammonite_search_engine_new (AmmoniteSearchType search_type)
{
    return g_object_new (AMMONITE_TYPE_SEARCH_ENGINE,
                         "search-type", search_type,
                         NULL);
}
