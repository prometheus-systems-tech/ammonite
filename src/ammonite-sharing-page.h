/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_SHARING_PAGE (ammonite_sharing_page_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteSharingPage, ammonite_sharing_page, AMMONITE, SHARING_PAGE, AdwNavigationPage)

AmmoniteSharingPage *ammonite_sharing_page_new (const char *path);

G_END_DECLS
