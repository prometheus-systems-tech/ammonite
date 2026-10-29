/*
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */

#pragma once

#include "ammonite-types.h"

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_SEARCH_PROVIDER (ammonite_search_provider_get_type ())
G_DECLARE_DERIVABLE_TYPE (AmmoniteSearchProvider, ammonite_search_provider,
                          AMMONITE, SEARCH_PROVIDER, GObject)

struct _AmmoniteSearchProviderClass
{
        GObjectClass parent_class;

        /** Name for provider implementation. Evaluated once. */
        const char * (*get_name) (AmmoniteSearchProvider *provider);
        /** Whether search happens in a dedicated thread. Optional for main thread. */
        gboolean (*run_in_thread) (AmmoniteSearchProvider *provider);
        /** Delay duration in ms. Optional. */
        guint (*search_delay) (AmmoniteSearchProvider *provider);

        /** Whether a search for given @query should run. Optional. */
        gboolean (*should_search) (AmmoniteSearchProvider *provider,
                                   AmmoniteQuery          *query);
        /** Starts a search. Only called when should_search() returned TRUE. */
        void (*start_search) (AmmoniteSearchProvider *provider);
        /** Stop an ongoing search. Optional. */
        void (*stop_search) (AmmoniteSearchProvider *provider);
};

/* Interface Functions */
gboolean       ammonite_search_provider_start           (AmmoniteSearchProvider *provider,
                                                         AmmoniteQuery *query);
void           ammonite_search_provider_stop            (AmmoniteSearchProvider *provider);

/*
 * Protected methods, generic type for convenience.
 * Can be called from main context or a single search thread.
 */
gboolean
ammonite_search_provider_should_stop (gpointer self);
GCancellable *
ammonite_search_provider_get_cancellable (gpointer self);
AmmoniteQuery *
ammonite_search_provider_get_query (gpointer self);
void
ammonite_search_provider_add_hit (gpointer           self,
                                  AmmoniteSearchHit *hit);
void
ammonite_search_provider_flush_hits (gpointer self);

void           ammonite_search_provider_finished        (AmmoniteSearchProvider *provider);

G_END_DECLS
