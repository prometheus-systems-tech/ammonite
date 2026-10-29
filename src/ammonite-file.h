/*
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "ammonite-types.h"

/* AmmoniteFile is an object used to represent a single element of a
 * AmmoniteDirectory. It's lightweight and relies on AmmoniteDirectory
 * to do most of the work.
 */

/* AmmoniteFile is defined both here and in ammonite-directory.h. */
#ifndef AMMONITE_FILE_DEFINED
#define AMMONITE_FILE_DEFINED
typedef struct AmmoniteFile AmmoniteFile;
#endif

#define AMMONITE_TYPE_FILE ammonite_file_get_type()
#define AMMONITE_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), AMMONITE_TYPE_FILE, AmmoniteFile))
#define AMMONITE_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), AMMONITE_TYPE_FILE, AmmoniteFileClass))
#define AMMONITE_IS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AMMONITE_TYPE_FILE))
#define AMMONITE_IS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), AMMONITE_TYPE_FILE))
#define AMMONITE_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), AMMONITE_TYPE_FILE, AmmoniteFileClass))

typedef enum {
	/* These may be set as default-sort-order. When touching this, make sure to
	 * keep the values in sync with the "org.ammonite.ammonite.SortOrder" enum in the
	 * `data/org.ammonite.ammonite.gschema.xml` schemas file.
	 */
	AMMONITE_FILE_SORT_BY_DISPLAY_NAME = 0,
	AMMONITE_FILE_SORT_BY_SIZE = 1,
	AMMONITE_FILE_SORT_BY_TYPE = 2,
	AMMONITE_FILE_SORT_BY_MTIME = 3,
	AMMONITE_FILE_SORT_BY_ATIME = 4,
	AMMONITE_FILE_SORT_BY_BTIME = 5,
	AMMONITE_FILE_SORT_BY_STARRED = 6,

	/* The following are specific to special locations and as such are not to be
	 * included in the "org.ammonite.ammonite.SortOrder" enum.
	 */
	AMMONITE_FILE_SORT_BY_TRASHED_TIME,
	AMMONITE_FILE_SORT_BY_SEARCH_RELEVANCE,
	AMMONITE_FILE_SORT_BY_RECENCY,

	/* Placeholder value not actually used for sorting */
	AMMONITE_FILE_SORT_BY_OTHER
} AmmoniteFileSortType;

typedef enum {
	AMMONITE_REQUEST_NOT_STARTED,
	AMMONITE_REQUEST_IN_PROGRESS,
	AMMONITE_REQUEST_DONE
} AmmoniteRequestStatus;

typedef enum {
	AMMONITE_FILE_ICON_FLAGS_NONE = 0,
	AMMONITE_FILE_ICON_FLAGS_USE_THUMBNAILS = (1<<0),
	/* uses the icon of the mount if present */
	AMMONITE_FILE_ICON_FLAGS_USE_MOUNT_ICON = (1<<1),
} G_GNUC_FLAG_ENUM AmmoniteFileIconFlags;

typedef enum {
	AMMONITE_METADATA_STRING = G_FILE_ATTRIBUTE_TYPE_STRING,
	AMMONITE_METADATA_STRINGV = G_FILE_ATTRIBUTE_TYPE_STRINGV,
} AmmoniteMetadataType;

#define AMMONITE_THUMBNAIL_MINIMUM_ICON_SIZE 32

typedef void (*AmmoniteFileCallback)          (AmmoniteFile  *file,
				               gpointer       callback_data);
typedef gboolean (*AmmoniteFileFilterFunc)    (AmmoniteFile  *file,
                                               gpointer       callback_data);
typedef void (*AmmoniteFileListCallback)      (GList         *file_list,
				               gpointer       callback_data);
typedef void (*AmmoniteFileOperationCallback) (AmmoniteFile  *file,
					       GFile         *result_location,
					       GError        *error,
					       gpointer       callback_data);

typedef void AmmoniteFileListHandle;

/* GObject requirements. */
GType                   ammonite_file_get_type                          (void);

/* Getting at a single file. */
AmmoniteFile *          ammonite_file_get                               (GFile                          *location);
AmmoniteFile *          ammonite_file_get_by_uri                        (const char                     *uri);

/* Get a file only if the ammonite version already exists */
AmmoniteFile *          ammonite_file_get_existing                      (GFile                          *location);
AmmoniteFile *          ammonite_file_get_existing_by_uri               (const char                     *uri);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
AmmoniteFile *          ammonite_file_ref                               (AmmoniteFile                   *file);
void                    ammonite_file_unref                             (AmmoniteFile                   *file);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AmmoniteFile, ammonite_file_unref)

/* Monitor the file. */
void                    ammonite_file_monitor_add                       (AmmoniteFile                   *file,
									 gconstpointer                   client,
									 AmmoniteAttributes              attributes);
void                    ammonite_file_monitor_remove                    (AmmoniteFile                   *file,
									 gconstpointer                   client);

AmmoniteDirectory *     ammonite_file_get_directory                     (AmmoniteFile                   *file);

/* Waiting for data that's read asynchronously.
 * This interface currently works only for metadata, but could be expanded
 * to other attributes as well.
 */
void                    ammonite_file_call_when_ready                   (AmmoniteFile                   *file,
									 AmmoniteAttributes              attributes,
									 AmmoniteFileCallback            callback,
									 gpointer                        callback_data);
void                    ammonite_file_cancel_call_when_ready            (AmmoniteFile                   *file,
									 AmmoniteFileCallback            callback,
									 gpointer                        callback_data);
gboolean                ammonite_file_check_if_ready                    (AmmoniteFile                   *file,
									 AmmoniteAttributes              attributes);
void                    ammonite_file_invalidate_attributes             (AmmoniteFile                   *file,
									 AmmoniteAttributes              attributes);
void                    ammonite_file_invalidate_all_attributes         (AmmoniteFile                   *file);

/* Basic attributes for file objects. */
gboolean                ammonite_file_contains_text                     (AmmoniteFile                   *file);
const char *            ammonite_file_get_display_name                  (AmmoniteFile                   *file);
const char *            ammonite_file_get_edit_name                     (AmmoniteFile                   *file);
const char *            ammonite_file_get_name                          (AmmoniteFile                   *file);
GFile *                 ammonite_file_get_location                      (AmmoniteFile                   *file);
char *                  ammonite_file_get_uri                           (AmmoniteFile                   *file);
char *                  ammonite_file_get_uri_scheme                    (AmmoniteFile                   *file);
AmmoniteFile *          ammonite_file_get_parent                        (AmmoniteFile                   *file);
GFile *                 ammonite_file_get_parent_location               (AmmoniteFile                   *file);
char *                  ammonite_file_get_parent_uri                    (AmmoniteFile                   *file);
const char *            ammonite_file_get_thumbnail_path                (AmmoniteFile                   *file);
gboolean                ammonite_file_has_thumbnail                     (AmmoniteFile                   *file);
gboolean                ammonite_file_can_get_size                      (AmmoniteFile                   *file);
guint64                 ammonite_file_get_size                          (AmmoniteFile                   *file);
time_t                  ammonite_file_get_mtime                         (AmmoniteFile                   *file);
time_t                  ammonite_file_get_atime                         (AmmoniteFile                   *file);
time_t                  ammonite_file_get_btime                         (AmmoniteFile                   *file);
time_t                  ammonite_file_get_recency                       (AmmoniteFile                   *file);
time_t                  ammonite_file_get_trash_time                    (AmmoniteFile                   *file);
GFileType               ammonite_file_get_file_type                     (AmmoniteFile                   *file);
const char *            ammonite_file_get_mime_type                     (AmmoniteFile                   *file);
gboolean                ammonite_file_is_mime_type                      (AmmoniteFile                   *file,
									 const char                     *mime_type);
gboolean                ammonite_file_is_launchable                     (AmmoniteFile                   *file);
gboolean                ammonite_file_is_symbolic_link                  (AmmoniteFile                   *file);
GMount *                ammonite_file_get_mount                         (AmmoniteFile                   *file);
char *                  ammonite_file_get_volume_free_space             (AmmoniteFile                   *file);
char *                  ammonite_file_get_volume_name                   (AmmoniteFile                   *file);
const char *            ammonite_file_get_symbolic_link_target_path     (AmmoniteFile                   *file);
char *                  ammonite_file_get_symbolic_link_target_uri      (AmmoniteFile                   *file);
gboolean                ammonite_file_is_broken_symbolic_link           (AmmoniteFile                   *file);
gboolean                ammonite_file_is_executable                     (AmmoniteFile                   *file);
gboolean                ammonite_file_is_directory                      (AmmoniteFile                   *file);
gboolean                ammonite_file_is_public_share_folder            (AmmoniteFile                   *file);
gboolean                ammonite_file_is_regular_file                   (AmmoniteFile                   *file);
gboolean                ammonite_file_is_user_special_directory         (AmmoniteFile                   *file,
									 GUserDirectory                 special_directory);
gboolean		ammonite_file_is_archive			(AmmoniteFile			*file);
gboolean                ammonite_file_is_in_search                      (AmmoniteFile                   *file);
gboolean                ammonite_file_is_in_trash                       (AmmoniteFile                   *file);
gboolean                ammonite_file_is_in_recent                      (AmmoniteFile                   *file);
gboolean                ammonite_file_is_in_starred                     (AmmoniteFile                   *file);
gboolean                ammonite_file_is_in_admin                       (AmmoniteFile                   *file);
gboolean                ammonite_file_is_remote                         (AmmoniteFile                   *file);
gboolean                ammonite_file_is_starred_location              (AmmoniteFile                   *file);
gboolean                ammonite_file_is_network_view                   (AmmoniteFile                   *file);
gboolean		ammonite_file_is_home				(AmmoniteFile                   *file);
GError *                ammonite_file_get_file_info_error               (AmmoniteFile                   *file);
gboolean                ammonite_file_get_directory_item_count          (AmmoniteFile                   *file,
									 guint                          *count,
									 gboolean                       *count_unreadable);
void                    ammonite_file_recompute_deep_counts             (AmmoniteFile                   *file);
AmmoniteRequestStatus   ammonite_file_get_deep_counts                   (AmmoniteFile                   *file,
									 guint                          *directory_count,
									 guint                          *file_count,
									 guint                          *unreadable_directory_count,
									 goffset               *total_size,
									 gboolean                        force);
gboolean                ammonite_file_should_show_thumbnail             (AmmoniteFile                   *file);
gboolean                ammonite_file_should_show_directory_item_count  (AmmoniteFile                   *file);

void                    ammonite_file_set_search_relevance              (AmmoniteFile                   *file,
									 gdouble                         relevance);
void                    ammonite_file_set_search_fts_snippet            (AmmoniteFile                   *file,
                                                                         const gchar                    *fts_snippet);
const gchar*            ammonite_file_get_search_fts_snippet            (AmmoniteFile                   *file);

void                    ammonite_file_set_attributes                    (AmmoniteFile                   *file,
									 GFileInfo                      *attributes,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);

const char *            ammonite_file_get_filesystem_id                 (AmmoniteFile                   *file);

AmmoniteFile *          ammonite_file_get_trash_original_file           (AmmoniteFile                   *file);

/* Permissions. */
gboolean                ammonite_file_can_get_permissions               (AmmoniteFile                   *file);
gboolean                ammonite_file_can_set_permissions               (AmmoniteFile                   *file);
guint                   ammonite_file_get_permissions                   (AmmoniteFile                   *file);
gboolean                ammonite_file_can_get_owner                     (AmmoniteFile                   *file);
gboolean                ammonite_file_can_set_owner                     (AmmoniteFile                   *file);
gboolean                ammonite_file_can_get_group                     (AmmoniteFile                   *file);
gboolean                ammonite_file_can_set_group                     (AmmoniteFile                   *file);
uid_t                   ammonite_file_get_uid                           (AmmoniteFile                   *file);
char *                  ammonite_file_get_owner_name                    (AmmoniteFile                   *file);
gid_t                   ammonite_file_get_gid                           (AmmoniteFile                   *file);
const char *            ammonite_file_get_group_name                    (AmmoniteFile                   *file);
GList *                 ammonite_get_user_names                         (void);
GList *                 ammonite_get_all_group_names                    (void);
GList *                 ammonite_file_get_settable_group_names          (AmmoniteFile                   *file);
gboolean                ammonite_file_can_get_selinux_context           (AmmoniteFile                   *file);
char *                  ammonite_file_get_selinux_context               (AmmoniteFile                   *file);

/* "Capabilities". */
gboolean                ammonite_file_can_read                          (AmmoniteFile                   *file);
gboolean                ammonite_file_can_write                         (AmmoniteFile                   *file);
gboolean                ammonite_file_can_execute                       (AmmoniteFile                   *file);
gboolean                ammonite_file_can_rename                        (AmmoniteFile                   *file);
gboolean                ammonite_file_can_delete                        (AmmoniteFile                   *file);
gboolean                ammonite_file_can_trash                         (AmmoniteFile                   *file);

gboolean                ammonite_file_can_mount                         (AmmoniteFile                   *file);
gboolean                ammonite_file_can_unmount                       (AmmoniteFile                   *file);
gboolean                ammonite_file_can_eject                         (AmmoniteFile                   *file);
gboolean                ammonite_file_can_start                         (AmmoniteFile                   *file);
gboolean                ammonite_file_can_start_degraded                (AmmoniteFile                   *file);
gboolean                ammonite_file_can_stop                          (AmmoniteFile                   *file);
GDriveStartStopType     ammonite_file_get_start_stop_type               (AmmoniteFile                   *file);
gboolean                ammonite_file_can_poll_for_media                (AmmoniteFile                   *file);
gboolean                ammonite_file_is_media_check_automatic          (AmmoniteFile                   *file);

void                    ammonite_file_mount                             (AmmoniteFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    ammonite_file_unmount                           (AmmoniteFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    ammonite_file_eject                             (AmmoniteFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);

void                    ammonite_file_start                             (AmmoniteFile                   *file,
									 GMountOperation                *start_op,
									 GCancellable                   *cancellable,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    ammonite_file_stop                              (AmmoniteFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    ammonite_file_poll_for_media                    (AmmoniteFile                   *file);

/* Basic operations for file objects. */
void                    ammonite_file_set_owner                         (AmmoniteFile                   *file,
									 const char                     *user_name_or_id,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    ammonite_file_set_group                         (AmmoniteFile                   *file,
									 const char                     *group_name_or_id,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    ammonite_file_set_permissions                   (AmmoniteFile                   *file,
									 guint32                         permissions,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    ammonite_file_rename                            (AmmoniteFile                   *file,
									 const char                     *new_name,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    ammonite_file_batch_rename                      (GList                          *files,
                                                                         GList                          *new_names,
                                                                         AmmoniteFileOperationCallback   callback,
                                                                         gpointer                        callback_data);
void                    ammonite_file_cancel                            (AmmoniteFile                   *file,
									 AmmoniteFileOperationCallback   callback,
									 gpointer                        callback_data);

/* Return true if this file has already been deleted.
 * This object will be unref'd after sending the files_removed signal,
 * but it could hang around longer if someone ref'd it.
 */
gboolean                ammonite_file_is_gone                           (AmmoniteFile                   *file);

gboolean                ammonite_file_has_been_unmounted                (AmmoniteFile                   *file);

/* Used in subclasses that handles the rename of a file. This handles the case
 * when the file is gone. If this returns TRUE, simply do nothing
 */
gboolean                ammonite_file_rename_handle_file_gone           (AmmoniteFile                   *file,
                                                                         AmmoniteFileOperationCallback   callback,
                                                                         gpointer                        callback_data);

/* Simple getting and setting top-level metadata. */
const char *            ammonite_file_get_metadata                      (AmmoniteFile                   *file,
									 const char                     *key);
GStrv		            ammonite_file_get_metadata_list                 (AmmoniteFile                   *file,
									 const char                     *key);
void                    ammonite_file_set_metadata                      (AmmoniteFile                   *file,
                            const char         *key,
									 AmmoniteMetadataType type,
                            		 gpointer             value);

/* Covers for common data types. */
gboolean                ammonite_file_get_boolean_metadata              (AmmoniteFile                   *file,
									 const char                     *key,
									 gboolean                        default_metadata);
void                    ammonite_file_set_boolean_metadata              (AmmoniteFile                   *file,
									 const char                     *key,
									 gboolean                        metadata);


/* Attributes for file objects as user-displayable strings. */
char *                  ammonite_file_get_string_attribute              (AmmoniteFile                   *file,
									 const char                     *attribute_name);
char *                  ammonite_file_get_string_attribute_q            (AmmoniteFile                   *file,
									 GQuark                          attribute_q);
char *                  ammonite_file_get_string_attribute_with_default (AmmoniteFile                   *file,
									 const char                     *attribute_name);
char *                  ammonite_file_get_string_attribute_with_default_q (AmmoniteFile                  *file,
									 GQuark                          attribute_q);

/* Matching with another URI. */
gboolean                ammonite_file_matches_uri                       (AmmoniteFile                   *file,
									 const char                     *uri);

gboolean                ammonite_file_has_local_path                    (AmmoniteFile                   *file);

/* Comparing two file objects for sorting */
const char *            ammonite_file_get_default_sort_order            (AmmoniteFile                   *file,
									 gboolean                       *reversed,
									 gboolean                       *is_forced);

int                     ammonite_file_compare_for_sort                  (AmmoniteFile                   *file_1,
									 AmmoniteFile                   *file_2,
									 AmmoniteFileSortType            sort_type,
									 gboolean			 directories_first,
									 gboolean		  	 reversed);
int                     ammonite_file_compare_for_sort_by_attribute     (AmmoniteFile                   *file_1,
									 AmmoniteFile                   *file_2,
									 const char                     *attribute,
									 gboolean                        directories_first,
									 gboolean                        reversed);
int                     ammonite_file_compare_for_sort_by_attribute_q   (AmmoniteFile                   *file_1,
									 AmmoniteFile                   *file_2,
									 GQuark                          attribute,
									 gboolean                        directories_first,
									 gboolean                        reversed);
gboolean                ammonite_file_is_date_sort_attribute_q          (GQuark                          attribute);
gboolean                ammonite_file_attribute_slow_sort               (const gchar                    *sort_attribute);
int                     ammonite_file_compare_location                  (AmmoniteFile                    *file_1,
                                                                         AmmoniteFile                    *file_2);

/* Compare display name of file with string for equality */
int                     ammonite_file_compare_display_name              (AmmoniteFile                   *file,
									 const char                     *string);

/* filtering functions for use by various directory views */
gboolean                ammonite_file_is_hidden_file                    (AmmoniteFile                   *file);
gboolean                ammonite_file_should_show                       (AmmoniteFile                   *file,
									 gboolean                        show_hidden);


/* Get the URI that's used when activating the file.
 * Getting this can require reading the contents of the file.
 */
gboolean                ammonite_file_has_activation_uri                (AmmoniteFile                   *file);
char *                  ammonite_file_get_activation_uri                (AmmoniteFile                   *file);
GFile *                 ammonite_file_get_activation_location           (AmmoniteFile                   *file);
GIcon *                 ammonite_file_get_gicon                         (AmmoniteFile                   *file,
									 AmmoniteFileIconFlags           flags);
GdkPaintable *          ammonite_file_get_icon_paintable                (AmmoniteFile                   *file,
									 int                             size,
									 int                             scale,
									 AmmoniteFileIconFlags           flags);

GList *                 ammonite_file_get_emblem_icons                  (AmmoniteFile                   *file);

/* Whether the file should open inside a view */
gboolean                ammonite_file_opens_in_view                     (AmmoniteFile                   *file);

/* Convenience functions for dealing with a list of AmmoniteFile objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
void                    ammonite_file_list_free                         (GList                          *file_list);
GList *                 ammonite_file_list_copy                         (GList                          *file_list);
GList *			ammonite_file_list_sort_by_display_name		(GList				*file_list);
void                    ammonite_file_list_call_when_ready              (GList                          *file_list,
									 AmmoniteAttributes              attributes,
									 AmmoniteFileListHandle        **handle,
									 AmmoniteFileListCallback        callback,
									 gpointer                        callback_data);
void                    ammonite_file_list_cancel_call_when_ready       (AmmoniteFileListHandle         *handle);

AmmoniteFileList *      ammonite_file_list_filter                       (AmmoniteFileList               *file_list,
                                                                         AmmoniteFileFilterFunc          filter_function,
                                                                         gpointer                        user_data);

gboolean                ammonite_file_list_are_all_folders              (const GList                    *files);

AmmoniteFileList *
ammonite_file_list_from_uris (const GStrv uris);

/* Debugging */

void                    ammonite_file_dump                              (AmmoniteFile                   *file);
void                    ammonite_file_list_debug                        (GList                          *files);

typedef struct AmmoniteFilePrivate AmmoniteFilePrivate;

struct AmmoniteFile {
	GObject parent_slot;
	AmmoniteFilePrivate *details;
};

/* This is actually a "protected" type, but it must be here so we can
 * compile the get_date function pointer declaration below.
 */
typedef enum {
	AMMONITE_DATE_TYPE_MODIFIED,
	AMMONITE_DATE_TYPE_ACCESSED,
	AMMONITE_DATE_TYPE_CREATED,
	AMMONITE_DATE_TYPE_TRASHED,
	AMMONITE_DATE_TYPE_RECENCY
} AmmoniteDateType;

typedef struct {
	GObjectClass parent_slot;

	/* Subclasses can set this to something other than G_FILE_TYPE_UNKNOWN and
	   it will be used as the default file type. This is useful when creating
	   a "virtual" AmmoniteFile subclass that you can't actually get real
	   information about. For exaple AmmoniteDesktopDirectoryFile. */
	GFileType default_file_type;

	/* Called when the file notices any change. */
	void                  (* changed)                (AmmoniteFile *file);

	/* Called periodically while directory deep count is being computed. */
	void                  (* updated_deep_count_in_progress) (AmmoniteFile *file);

	/* Virtual functions which MUST be implemented by subclasses */
	void                  (* monitor_add)            (AmmoniteFile           *file,
							  gconstpointer           client,
							  AmmoniteAttributes      attributes);
	void                  (* monitor_remove)         (AmmoniteFile           *file,
							  gconstpointer           client);
	void                  (* call_when_ready)        (AmmoniteFile           *file,
							  AmmoniteAttributes      attributes,
							  AmmoniteFileCallback    callback,
							  gpointer                callback_data);
	void                  (* cancel_call_when_ready) (AmmoniteFile           *file,
							  AmmoniteFileCallback    callback,
							  gpointer                callback_data);
	gboolean              (* check_if_ready)         (AmmoniteFile           *file,
							  AmmoniteAttributes      attributes);

        /* Virtual functions which MAY be overridden by subclasses */
	gboolean              (* get_item_count)         (AmmoniteFile           *file,
							  guint                  *count,
							  gboolean               *count_unreadable);
	AmmoniteRequestStatus (* get_deep_counts)        (AmmoniteFile           *file,
							  guint                  *directory_count,
							  guint                  *file_count,
							  guint                  *unreadable_directory_count,
							  goffset       *total_size);

        /* Virtual functions which MAY be implemented by subclasses (default implementation does nothing) */
	void                  (* set_metadata)           (AmmoniteFile           *file,
							  const char             *key,
							  AmmoniteMetadataType    type,
							  gpointer             	  value);

	void                  (* mount)                  (AmmoniteFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  AmmoniteFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* unmount)                 (AmmoniteFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  AmmoniteFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* eject)                   (AmmoniteFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  AmmoniteFileOperationCallback   callback,
							  gpointer                        callback_data);

	void                  (* start)                  (AmmoniteFile                   *file,
							  GMountOperation                *start_op,
							  GCancellable                   *cancellable,
							  AmmoniteFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* stop)                    (AmmoniteFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  AmmoniteFileOperationCallback   callback,
							  gpointer                        callback_data);

	void                 (* poll_for_media)          (AmmoniteFile                   *file);
} AmmoniteFileClass;
