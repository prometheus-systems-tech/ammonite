/*
 * SPDX-FileCopyrightText: 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include "ammonite-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_PROGRESS_INFO_MANAGER ammonite_progress_info_manager_get_type()
G_DECLARE_FINAL_TYPE (AmmoniteProgressInfoManager, ammonite_progress_info_manager, AMMONITE, PROGRESS_INFO_MANAGER, GObject)

AmmoniteProgressInfoManager* ammonite_progress_info_manager_dup_singleton (void);

void ammonite_progress_info_manager_add_new_info (AmmoniteProgressInfoManager *self,
                                                  AmmoniteProgressInfo *info);
GList *ammonite_progress_info_manager_get_all_infos (AmmoniteProgressInfoManager *self);
void ammonite_progress_info_manager_remove_finished_or_cancelled_infos (AmmoniteProgressInfoManager *self);
gboolean ammonite_progress_manager_are_all_infos_finished_or_cancelled (AmmoniteProgressInfoManager *self);

void ammonite_progress_manager_add_viewer (AmmoniteProgressInfoManager *self, GObject *viewer);
void ammonite_progress_manager_remove_viewer (AmmoniteProgressInfoManager *self, GObject *viewer);
gboolean ammonite_progress_manager_has_viewers (AmmoniteProgressInfoManager *self);

G_END_DECLS
