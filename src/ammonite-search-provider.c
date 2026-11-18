/*
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2025 The Files contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#define G_LOG_DOMAIN "ammonite-search"

#include <config.h>
#include "ammonite-search-provider.h"

#include "ammonite-query.h"

#include <gio/gio.h>
#include <glib-object.h>

typedef struct
{
    const char *name;
    guint delayed_timeout_id;

    GPtrArray *hits;

    GMutex idle_submit_mutex;
    guint submit_on_idle_id;
    GPtrArray *hits_to_submit;

    /* Thread-safe variables */
    GCancellable *cancellable;
    AmmoniteQuery *query;
} AmmoniteSearchProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AmmoniteSearchProvider, ammonite_search_provider, G_TYPE_OBJECT)

enum
{
    HITS_ADDED,
    FINISHED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
setup_signals (void)
{
    /**
     * AmmoniteSearchProvider::hits-added:
     * @provider: the provider that found search hits
     * @hits: (transfer full): #GPtrArray of #AmmoniteSearchHit
     *
     * This signal is emitted when the provider has search hits.
     */
    signals[HITS_ADDED] = g_signal_new ("hits-added",
                                        AMMONITE_TYPE_SEARCH_PROVIDER,
                                        G_SIGNAL_RUN_LAST, 0,
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__POINTER,
                                        G_TYPE_NONE, 1,
                                        G_TYPE_POINTER);

    /**
     * AmmoniteSearchProvider::provider-finished:
     * @provider: the provider that finished searching
     *
     * This signal is emitted when the provider finishes searching.
     */
    signals[FINISHED] = g_signal_new ("provider-finished", AMMONITE_TYPE_SEARCH_PROVIDER,
                                      G_SIGNAL_RUN_LAST, 0,
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE, 0);
}

static const char *
search_provider_name (AmmoniteSearchProvider *self)
{
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    if (G_UNLIKELY (priv->name == NULL))
    {
        AmmoniteSearchProviderClass *klass = AMMONITE_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));
        priv->name = klass->get_name (self);
    }

    return priv->name;
}

static void
actual_start (AmmoniteSearchProvider *self)
{
    AmmoniteSearchProviderClass *klass = AMMONITE_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    priv->delayed_timeout_id = 0;
    priv->cancellable = g_cancellable_new ();
    priv->hits = g_ptr_array_new_with_free_func (g_object_unref);
    /* Keep reference on self while running */
    g_object_ref (self);

    g_debug ("Search provider '%s' starting", search_provider_name (self));
    klass->start_search (self);
}

gboolean
ammonite_search_provider_start (AmmoniteSearchProvider *self,
                                AmmoniteQuery          *query)
{
    g_return_val_if_fail (AMMONITE_IS_SEARCH_PROVIDER (self), FALSE);
    g_return_val_if_fail (AMMONITE_IS_QUERY (query), FALSE);

    AmmoniteSearchProviderClass *klass = AMMONITE_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    /* Can't start provider again before it finished */
    g_return_val_if_fail (priv->cancellable == NULL, FALSE);

    if (!klass->should_search (self, query))
    {
        return FALSE;
    }

    g_set_object (&priv->query, query);

    guint delay_ms = klass->search_delay (self);
    if (delay_ms > 0)
    {
        g_debug ("Search provider '%s' delayed", search_provider_name (self));
        priv->delayed_timeout_id = g_timeout_add_once (delay_ms,
                                                       (GSourceOnceFunc) actual_start,
                                                       self);
    }
    else
    {
        actual_start (self);
    }

    return TRUE;
}

void
ammonite_search_provider_stop (AmmoniteSearchProvider *self)
{
    g_return_if_fail (AMMONITE_IS_SEARCH_PROVIDER (self));

    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    if (priv->delayed_timeout_id != 0)
    {
        g_debug ("Search provider '%s' cancelled before starting", search_provider_name (self));
        g_clear_handle_id (&priv->delayed_timeout_id, g_source_remove);
        g_clear_object (&priv->query);
        g_signal_emit (self, signals[FINISHED], 0);
    }
    else if (ammonite_search_provider_should_stop (self))
    {
        return;
    }
    else
    {
        AmmoniteSearchProviderClass *klass = AMMONITE_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));

        g_debug ("Search provider '%s' stopping", search_provider_name (self));
        g_cancellable_cancel (priv->cancellable);

        klass->stop_search (self);
    }
}

static void
search_provider_submit_hits (AmmoniteSearchProvider *self,
                             GPtrArray              *hits)
{
    if (ammonite_search_provider_should_stop (self) ||
        hits == NULL ||
        hits->len == 0)
    {
        g_clear_pointer (&hits, g_ptr_array_unref);
        return;
    }

    g_debug ("Search provider '%s' found %d hits", search_provider_name (self), hits->len);
    g_signal_emit (self, signals[HITS_ADDED], 0, g_steal_pointer (&hits));
}

static void
search_provider_submit_on_idle (AmmoniteSearchProvider *self)
{
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);
    GPtrArray *hits;

    g_mutex_lock (&priv->idle_submit_mutex);
    priv->submit_on_idle_id = 0;
    hits = g_steal_pointer (&priv->hits_to_submit);
    g_mutex_unlock (&priv->idle_submit_mutex);

    search_provider_submit_hits (self, hits);
}

void
ammonite_search_provider_finished (AmmoniteSearchProvider *self)
{
    g_return_if_fail (AMMONITE_IS_SEARCH_PROVIDER (self));

    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);
    GPtrArray *ready_hits;

    g_mutex_lock (&priv->idle_submit_mutex);
    g_clear_handle_id (&priv->submit_on_idle_id, g_source_remove);
    ready_hits = g_steal_pointer (&priv->hits_to_submit);
    g_mutex_unlock (&priv->idle_submit_mutex);

    search_provider_submit_hits (self, ready_hits);
    search_provider_submit_hits (self, g_steal_pointer (&priv->hits));

    g_clear_object (&priv->cancellable);
    g_clear_object (&priv->query);

    g_debug ("Search provider '%s' finished", search_provider_name (self));

    /* Drop self-reference, counterpart to start() */
    g_object_unref (self);

    g_signal_emit (self, signals[FINISHED], 0);
}

/**
 * Protected methods, generic type for convenience.
 * These functions may be called outside the main context.
 */

gboolean
ammonite_search_provider_should_stop (gpointer self)
{
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    return priv->cancellable == NULL || g_cancellable_is_cancelled (priv->cancellable);
}

GCancellable *
ammonite_search_provider_get_cancellable (gpointer self)
{
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    return priv->cancellable;
}

AmmoniteQuery *
ammonite_search_provider_get_query (gpointer self)
{
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    return priv->query;
}

void
ammonite_search_provider_add_hit (gpointer           self,
                                  AmmoniteSearchHit *hit)
{
    const guint BATCH_LIMIT = 100;

    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    g_ptr_array_add (priv->hits, hit);

    if (priv->hits->len >= BATCH_LIMIT)
    {
        ammonite_search_provider_flush_hits (self);
    }
}

void
ammonite_search_provider_flush_hits (gpointer self)
{
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    if (ammonite_search_provider_should_stop (self) ||
        priv->hits->len == 0)
    {
        /* Nothing to do */
        return;
    }

    AmmoniteSearchProviderClass *klass = (AmmoniteSearchProviderClass *) G_OBJECT_GET_CLASS (self);

    if (klass->run_in_thread (self))
    {
        G_MUTEX_AUTO_LOCK (&priv->idle_submit_mutex, locker);

        if (priv->submit_on_idle_id != 0)
        {
            /* Last batch is still pending, don't schedule another */
            return;
        }
        /* Schedule submit from main context */
        priv->hits_to_submit = g_steal_pointer (&priv->hits);
        priv->submit_on_idle_id = g_idle_add_once ((GSourceOnceFunc) search_provider_submit_on_idle,
                                                   self);
    }
    else
    {
        search_provider_submit_hits (self, g_steal_pointer (&priv->hits));
    }

    priv->hits = g_ptr_array_new_with_free_func (g_object_unref);
}

/* End protected methods */

static void
ammonite_search_provider_init (AmmoniteSearchProvider *self)
{
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    g_mutex_init (&priv->idle_submit_mutex);
}

static void
search_provider_dispose (GObject *object)
{
    AmmoniteSearchProvider *self = AMMONITE_SEARCH_PROVIDER (object);
    AmmoniteSearchProviderPrivate *priv = ammonite_search_provider_get_instance_private (self);

    g_warn_if_fail (priv->submit_on_idle_id == 0);

    g_mutex_clear (&priv->idle_submit_mutex);
    g_clear_object (&priv->cancellable);
    g_clear_object (&priv->query);
    g_clear_pointer (&priv->hits, g_ptr_array_unref);
    g_clear_handle_id (&priv->delayed_timeout_id, g_source_remove);

    G_OBJECT_CLASS (ammonite_search_provider_parent_class)->dispose (object);
}

static gboolean
default_should_search (AmmoniteSearchProvider *provider,
                       AmmoniteQuery          *query)
{
    return TRUE;
}

static gboolean
default_run_in_thread (AmmoniteSearchProvider *provider)
{
    return FALSE;
}

static guint
default_search_delay (AmmoniteSearchProvider *provider)
{
    return 0;
}

static void
default_stop_search (AmmoniteSearchProvider *provider)
{
}

static void
ammonite_search_provider_class_init (AmmoniteSearchProviderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    AmmoniteSearchProviderClass *search_provider_class = AMMONITE_SEARCH_PROVIDER_CLASS (klass);

    object_class->dispose = search_provider_dispose;
    search_provider_class->run_in_thread = default_run_in_thread;
    search_provider_class->should_search = default_should_search;
    search_provider_class->search_delay = default_search_delay;
    search_provider_class->stop_search = default_stop_search;

    setup_signals ();
}
