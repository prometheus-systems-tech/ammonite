/*
 * SPDX-FileCopyrightText: 2018 Ernestas Kulik <ernestask@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* This here header contains Ammonite type definitions.
 *
 * It is advisable to include this in headers or when you
 * only need the name of a type (i.e. not calling any functions from the
 * associated header). Doing so will help avoid circular inclusions and
 * pointless rebuilds should the header ever change.
 */

#pragma once

#include "ammonite-enums.h"

#include <glib.h>

/* Keep sorted alphabetically. */

typedef struct _AmmoniteBatchRenameDialog   AmmoniteBatchRenameDialog;
typedef struct _AmmoniteBookmark            AmmoniteBookmark;
typedef struct _AmmoniteBookmarkList        AmmoniteBookmarkList;
typedef struct _AmmoniteClipboard           AmmoniteClipboard;
typedef struct _AmmoniteColumn              AmmoniteColumn;
typedef struct _AmmoniteDirectory           AmmoniteDirectory;
typedef struct  AmmoniteFile                AmmoniteFile;
typedef struct _AmmoniteFileOperationsDBusData AmmoniteFileOperationsDBusData;
typedef struct _AmmoniteFileUndoInfo        AmmoniteFileUndoInfo;
typedef struct _AmmoniteFilesView           AmmoniteFilesView;
typedef struct _AmmoniteFileUndoManager     AmmoniteFileUndoManager;
typedef struct  AmmoniteHashQueue           AmmoniteHashQueue;
typedef struct _AmmoniteIconInfo            AmmoniteIconInfo;
typedef struct _AmmoniteListBase            AmmoniteListBase;
typedef struct  AmmoniteMonitor             AmmoniteMonitor;
typedef struct _AmmoniteProgressInfo        AmmoniteProgressInfo;
typedef struct _AmmoniteQuery               AmmoniteQuery;
typedef struct _AmmoniteQueryEditor         AmmoniteQueryEditor;
typedef struct _AmmoniteSearchHit           AmmoniteSearchHit;
typedef struct _AmmoniteSearchProvider      AmmoniteSearchProvider;
typedef struct _AmmoniteViewCell            AmmoniteViewCell;
typedef struct _AmmoniteViewItem            AmmoniteViewItem;
typedef struct _AmmoniteViewModel           AmmoniteViewModel;
typedef struct _AmmoniteWindow              AmmoniteWindow;
typedef struct _AmmoniteWindowSlot          AmmoniteWindowSlot;

/* List aliases used for implicit type documentation */
#define LIST_ALIAS(ALIAS) \
    typedef GList ALIAS; \
    G_DEFINE_AUTOPTR_CLEANUP_FUNC(ALIAS, g_list_free)

LIST_ALIAS(GFileList)
LIST_ALIAS(AmmoniteDirectoryList)
LIST_ALIAS(AmmoniteFileList)
