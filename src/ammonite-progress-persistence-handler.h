/*
 * ammonite-progress-persistence-handler.h: file operation progress systray icon or notification handler.
 *
 * SPDX-FileCopyrightText: 2007, 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_PROGRESS_PERSISTENCE_HANDLER ammonite_progress_persistence_handler_get_type()
G_DECLARE_FINAL_TYPE (AmmoniteProgressPersistenceHandler, ammonite_progress_persistence_handler, AMMONITE, PROGRESS_PERSISTENCE_HANDLER, GObject)

/* @app is actually a AmmoniteApplication, but we need to avoid circular dependencies */
AmmoniteProgressPersistenceHandler * ammonite_progress_persistence_handler_new (GObject *app);

G_END_DECLS
