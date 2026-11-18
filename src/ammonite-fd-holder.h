/*
 * SPDX-FileCopyrightText: 2023 António Fernandes <antoniof@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_FD_HOLDER (ammonite_fd_holder_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteFdHolder, ammonite_fd_holder, AMMONITE, FD_HOLDER, GObject)

AmmoniteFdHolder *ammonite_fd_holder_new                (void);
void              ammonite_fd_holder_set_location       (AmmoniteFdHolder *self,
                                                         GFile            *location);
void              ammonite_fd_holders_release_for_mount (GMount           *mount);

G_END_DECLS
