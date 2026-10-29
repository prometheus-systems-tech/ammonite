/*
 * SPDX-FileCopyrightText: 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* This is the little brother of ammonite-types.h and only contains enumerations.
 *
 * Now that you’ve familiarized yourself with it, the reason for its existence
 * is similar, and the split is purely for convenience reasons. Include this
 * when you only need a certain enumeration and not the whole header that might
 * have had it originally. Otherwise, include both!
 */

#pragma once

#include <glib.h>

/* Keep sorted alphabetically. */

typedef enum
{
    AMMONITE_GRID_ICON_SIZE_SMALL       = 48,
    AMMONITE_GRID_ICON_SIZE_SMALL_PLUS  = 64,
    AMMONITE_GRID_ICON_SIZE_MEDIUM      = 96,
    AMMONITE_GRID_ICON_SIZE_LARGE       = 168,
    AMMONITE_GRID_ICON_SIZE_EXTRA_LARGE = 256,
} AmmoniteGridIconSize;

typedef enum
{
    AMMONITE_GRID_ZOOM_LEVEL_SMALL,
    AMMONITE_GRID_ZOOM_LEVEL_SMALL_PLUS,
    AMMONITE_GRID_ZOOM_LEVEL_MEDIUM,
    AMMONITE_GRID_ZOOM_LEVEL_LARGE,
    AMMONITE_GRID_ZOOM_LEVEL_EXTRA_LARGE,
} AmmoniteGridZoomLevel;

typedef enum
{
    AMMONITE_LIST_ICON_SIZE_SMALL  = 16,
    AMMONITE_LIST_ICON_SIZE_MEDIUM = 32,
    AMMONITE_LIST_ICON_SIZE_LARGE  = 64,
} AmmoniteListIconSize;

typedef enum
{
    AMMONITE_LIST_ZOOM_LEVEL_SMALL,
    AMMONITE_LIST_ZOOM_LEVEL_MEDIUM,
    AMMONITE_LIST_ZOOM_LEVEL_LARGE,
} AmmoniteListZoomLevel;

#define AMMONITE_ATTRIBUTE_N_TOTAL 9
typedef enum
{
    /* Some attributes require others, which is done by ORing them */

    AMMONITE_ATTRIBUTE_INFO                 = 1 << 0, /* All standard info */
    AMMONITE_ATTRIBUTE_DEEP_COUNT           = 1 << 1,
    AMMONITE_ATTRIBUTE_DIRECTORY_ITEM_COUNT = 1 << 2,
    AMMONITE_ATTRIBUTE_THUMBNAIL_INFO       = 1 << 3 | AMMONITE_ATTRIBUTE_INFO,
    AMMONITE_ATTRIBUTE_EXTENSION_INFO       = 1 << 4,
    AMMONITE_ATTRIBUTE_THUMBNAIL_BUFFER     = 1 << 5 | AMMONITE_ATTRIBUTE_THUMBNAIL_INFO,
    AMMONITE_ATTRIBUTE_MOUNT                = 1 << 6 | AMMONITE_ATTRIBUTE_INFO,
    AMMONITE_ATTRIBUTE_FILESYSTEM_INFO      = 1 << 7,
    AMMONITE_ATTRIBUTE_FILE_LIST            = 1 << 8,
    /* Adjust AMMONITE_ATTRIBUTE_N_TOTAL when expanding */
} AmmoniteAttributes;

typedef enum
{
    AMMONITE_MODE_BROWSE = 0,
    AMMONITE_MODE_OPEN_FILE,
    AMMONITE_MODE_OPEN_FILES,
    AMMONITE_MODE_OPEN_FOLDER,
    AMMONITE_MODE_OPEN_FOLDERS,
    AMMONITE_MODE_SAVE_FILE,
    AMMONITE_MODE_SAVE_FILES,
} AmmoniteMode;

typedef enum
{
    AMMONITE_OPEN_FLAG_NORMAL           = 1 << 0,
    AMMONITE_OPEN_FLAG_NEW_WINDOW       = 1 << 1,
    AMMONITE_OPEN_FLAG_NEW_TAB          = 1 << 2,
    AMMONITE_OPEN_FLAG_DONT_MAKE_ACTIVE = 1 << 3,
    AMMONITE_OPEN_FLAG_REUSE_EXISTING   = 1 << 4,
} G_GNUC_FLAG_ENUM AmmoniteOpenFlags;

/* See org.ammonite.ammonite.SearchFilterTimeType schema */
typedef enum {
    AMMONITE_SEARCH_TIME_TYPE_LAST_MODIFIED = 0,
    AMMONITE_SEARCH_TIME_TYPE_LAST_ACCESS,
    AMMONITE_SEARCH_TIME_TYPE_CREATED,
} AmmoniteSearchTimeType;

typedef enum {
  AMMONITE_SIDEBAR_ROW_INVALID = 0,
  AMMONITE_SIDEBAR_ROW_BUILT_IN,
  AMMONITE_SIDEBAR_ROW_EXTERNAL_MOUNT,
  AMMONITE_SIDEBAR_ROW_INTERNAL_MOUNT,
  AMMONITE_SIDEBAR_ROW_NEW_BOOKMARK,
  AMMONITE_SIDEBAR_ROW_BOOKMARK,
  AMMONITE_SIDEBAR_ROW_BOOKMARK_PLACEHOLDER,
  AMMONITE_SIDEBAR_N_ROW_TYPES
} AmmoniteSidebarRowType;

/* Keep order, since it's used for the sort functions */
typedef enum {
  AMMONITE_SIDEBAR_SECTION_INVALID = 0,
  AMMONITE_SIDEBAR_SECTION_DEFAULT_LOCATIONS,
  AMMONITE_SIDEBAR_SECTION_BOOKMARKS,
  AMMONITE_SIDEBAR_SECTION_CLOUD,
  AMMONITE_SIDEBAR_SECTION_MOUNTS,
  AMMONITE_SIDEBAR_N_SECTION_TYPES
} AmmoniteSidebarSectionType;

/**
 * AmmoniteSelectionSource:
 * @AMMONITE_SELECTION_SOURCE_NONE: Fallback for undefined source.
 * @AMMONITE_SELECTION_SOURCE_MANUAL: Selection was manual by the user through
 * manual clicking, rubberbanding, pattern selection, or selection shortcuts.
 * @AMMONITE_SELECTION_SOURCE_AUTO: Selection was programmatically by a source
 * other than what is defined below.
 * @AMMONITE_SELECTION_SOURCE_IN_SEARCH: Automatic selection of the first item
 * during search
 * @AMMONITE_SELECTION_SOURCE_AFTER_SEARCH: Same as IN_SEARCH, but transferred
 * after exiting search without change.
 * @AMMONITE_SELECTION_SOURCE_OP_START: The selection that existed at the time
 * of an operation start.
 * @AMMONITE_SELECTION_SOURCE_OP_DONE: Selection that appears as a result of an
 * operation after it has finished.
 *
 **/
typedef enum
{
    AMMONITE_SELECTION_SOURCE_NONE,
    AMMONITE_SELECTION_SOURCE_MANUAL,
    AMMONITE_SELECTION_SOURCE_AUTO,
    AMMONITE_SELECTION_SOURCE_IN_SEARCH,
    AMMONITE_SELECTION_SOURCE_AFTER_SEARCH,
    AMMONITE_SELECTION_SOURCE_OP_START,
    AMMONITE_SELECTION_SOURCE_OP_DONE,
} AmmoniteSelectionSource;
