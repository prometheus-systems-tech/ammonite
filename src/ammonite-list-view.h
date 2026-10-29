/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2001, 2002 Anders Carlsson <andersca@gnu.org>
 * SPDX-FileCopyrightText: 2022 GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-list-base.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_LIST_VIEW (ammonite_list_view_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteListView, ammonite_list_view, AMMONITE, LIST_VIEW, AmmoniteListBase)

AmmoniteListView *ammonite_list_view_new (void);

void ammonite_list_view_present_column_editor (AmmoniteListView *self);

G_END_DECLS
