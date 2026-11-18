/*
 * SPDX-FileCopyrightText: 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

#pragma once

/* Keys for getting/setting Ammonite metadata. All metadata used in Ammonite
 * should define its key here, so we can keep track of the whole set easily.
 * Any updates here needs to be added in ammonite-metadata.c too.
 */

#include <glib.h>

/* Per-file */

#define AMMONITE_METADATA_KEY_ICON_VIEW_SORT_BY          	"ammonite-icon-view-sort-by"
#define AMMONITE_METADATA_KEY_ICON_VIEW_SORT_REVERSED    	"ammonite-icon-view-sort-reversed"

#define AMMONITE_METADATA_KEY_LIST_VIEW_SORT_COLUMN      	"ammonite-list-view-sort-column"
#define AMMONITE_METADATA_KEY_LIST_VIEW_SORT_REVERSED    	"ammonite-list-view-sort-reversed"
#define AMMONITE_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS    	"ammonite-list-view-visible-columns"
#define AMMONITE_METADATA_KEY_LIST_VIEW_COLUMN_ORDER    	"ammonite-list-view-column-order"

#define AMMONITE_METADATA_KEY_CUSTOM_ICON                	"custom-icon"
#define AMMONITE_METADATA_KEY_CUSTOM_ICON_NAME                	"custom-icon-name"
#define AMMONITE_METADATA_KEY_EMBLEMS				"emblems"

guint ammonite_metadata_get_id (const char *metadata);
