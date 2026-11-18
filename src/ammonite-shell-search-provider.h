/*
 * ammonite-shell-search-provider.h - Implementation of a GNOME Shell search provider
 *
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#pragma once

#define AMMONITE_TYPE_SHELL_SEARCH_PROVIDER ammonite_shell_search_provider_get_type()

G_DECLARE_FINAL_TYPE (AmmoniteShellSearchProvider, ammonite_shell_search_provider, AMMONITE, SHELL_SEARCH_PROVIDER, GObject)

GType ammonite_shell_search_provider_get_type (void);
AmmoniteShellSearchProvider * ammonite_shell_search_provider_new (void);

gboolean ammonite_shell_search_provider_register   (AmmoniteShellSearchProvider *self,
                                                    GDBusConnection             *connection,
                                                    GError                     **error);
void     ammonite_shell_search_provider_unregister (AmmoniteShellSearchProvider *self);
