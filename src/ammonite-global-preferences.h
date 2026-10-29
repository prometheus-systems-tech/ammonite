
/*
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

/* Display  */
#define AMMONITE_PREFERENCES_SHOW_HIDDEN_FILES			"show-hidden"

/* Mouse */
#define AMMONITE_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS		"mouse-use-extra-buttons"
#define AMMONITE_PREFERENCES_MOUSE_FORWARD_BUTTON		"mouse-forward-button"
#define AMMONITE_PREFERENCES_MOUSE_BACK_BUTTON			"mouse-back-button"

/* Single/Double click preference  */
#define AMMONITE_PREFERENCES_CLICK_POLICY			"click-policy"

/* Drag and drop preferences */
#define AMMONITE_PREFERENCES_OPEN_FOLDER_ON_DND_HOVER   	"open-folder-on-dnd-hover"

/* Spatial or browser mode */
#define AMMONITE_PREFERENCES_NEW_TAB_POSITION			"tabs-open-position"

#define AMMONITE_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY		"always-use-location-entry"

/* Which views should be displayed for new windows */
#define AMMONITE_WINDOW_STATE_INITIAL_SIZE			"initial-size"
#define AMMONITE_WINDOW_STATE_MAXIMIZED				"maximized"

#define AMMONITE_WINDOW_STATE_INITIAL_SIZE_FILE_CHOOSER         "initial-size-file-chooser"

/* Sorting order */
#define AMMONITE_PREFERENCES_SORT_DIRECTORIES_FIRST		"sort-directories-first"
#define AMMONITE_PREFERENCES_DEFAULT_SORT_ORDER			"default-sort-order"
#define AMMONITE_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER	"default-sort-in-reverse-order"

/* The default folder viewer - one of the two enums below */
#define AMMONITE_PREFERENCES_DEFAULT_FOLDER_VIEWER		"default-folder-viewer"

/* Compression */
#define AMMONITE_PREFERENCES_DEFAULT_COMPRESSION_FORMAT         "default-compression-format"

typedef enum
{
        AMMONITE_COMPRESSION_ZIP = 0,
        AMMONITE_COMPRESSION_TAR_XZ,
        AMMONITE_COMPRESSION_7ZIP,
        AMMONITE_COMPRESSION_ENCRYPTED_ZIP
} AmmoniteCompressionFormat;

/* Icon View */
#define AMMONITE_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"

/* Which text attributes appear beneath icon names */
#define AMMONITE_PREFERENCES_ICON_VIEW_CAPTIONS				"captions"

/* List View */
#define AMMONITE_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"
#define AMMONITE_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS		"default-visible-columns"
#define AMMONITE_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER		"default-column-order"
#define AMMONITE_PREFERENCES_LIST_VIEW_USE_TREE                         "use-tree-view"

enum
{
	AMMONITE_CLICK_POLICY_SINGLE,
	AMMONITE_CLICK_POLICY_DOUBLE
};

typedef enum
{
	AMMONITE_SPEED_TRADEOFF_ALWAYS,
	AMMONITE_SPEED_TRADEOFF_LOCAL_ONLY,
	AMMONITE_SPEED_TRADEOFF_NEVER
} AmmoniteSpeedTradeoffValue;

#define AMMONITE_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS "show-directory-item-counts"
#define AMMONITE_PREFERENCES_SHOW_FILE_THUMBNAILS	"show-image-thumbnails"
#define AMMONITE_PREFERENCES_FILE_THUMBNAIL_LIMIT	"thumbnail-limit"

typedef enum
{
	AMMONITE_COMPLEX_SEARCH_BAR,
	AMMONITE_SIMPLE_SEARCH_BAR
} AmmoniteSearchBarMode;

/* Lockdown */
#define AMMONITE_PREFERENCES_LOCKDOWN_COMMAND_LINE         "disable-command-line"

/* Recent files */
#define AMMONITE_PREFERENCES_RECENT_FILES_ENABLED          "remember-recent-files"

/* Search behaviour */
#define AMMONITE_PREFERENCES_RECURSIVE_SEARCH "recursive-search"

/* Context menu options */
#define AMMONITE_PREFERENCES_SHOW_DELETE_PERMANENTLY "show-delete-permanently"
#define AMMONITE_PREFERENCES_SHOW_CREATE_LINK "show-create-link"
#define AMMONITE_PREFERENCES_SHOW_FOLDER_TREE "show-folder-tree"

/* Full Text Search enabled */
#define AMMONITE_PREFERENCES_FTS_ENABLED "fts-enabled"

/* Date and time format in the view */
#define AMMONITE_PREFERENCES_DATE_TIME_FORMAT "date-time-format"

typedef enum
{
        AMMONITE_DATE_TIME_FORMAT_SIMPLE = 0,
        AMMONITE_DATE_TIME_FORMAT_DETAILED,
} AmmoniteDateTimeFormat;

void ammonite_global_preferences_init                      (void);
guint ammonite_global_preferences_get_back_button (void);
guint ammonite_global_preferences_get_forward_button (void);
gboolean ammonite_global_preferences_get_use_extra_buttons (void);

extern GSettings *ammonite_preferences;
extern GSettings *ammonite_compression_preferences;
extern GSettings *ammonite_icon_view_preferences;
extern GSettings *ammonite_list_view_preferences;
extern GSettings *ammonite_window_state;
extern GSettings *gtk_filechooser_preferences;
extern GSettings *gnome_lockdown_preferences;
extern GSettings *gnome_interface_preferences;
extern GSettings *gnome_privacy_preferences;
extern GSettings *gnome_user_share_preferences;
extern GSettings *localsearch_preferences;

G_END_DECLS
