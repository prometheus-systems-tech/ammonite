/*
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include "ammonite-types.h"

#include "ammonite-file.h"

#define AMMONITE_FILE_DEFAULT_ATTRIBUTES				\
	"standard::*,access::*,mountable::*,time::*,unix::*,owner::*,selinux::*,id::filesystem,trash::orig-path,trash::deletion-date,metadata::*,recent::*,preview::icon"

/* These are in the typical sort order. Known things come first, then
 * things where we can't know, finally things where we don't yet know.
 */
typedef enum {
	KNOWN,
	UNKNOWABLE,
	UNKNOWN
} Knowledge;

struct AmmoniteFilePrivate
{
	AmmoniteDirectory *directory;
	
	GRefString *name;

	/* File info: */
	GFileType type;

	GRefString *display_name;
	char *display_name_collation_key;
	char *directory_name_collation_key;
	GRefString *edit_name;

	goffset size; /* -1 is unknown */
	
	int sort_order;
	
	guint32 permissions;
	gboolean has_uid;
	uid_t uid;
	gboolean has_gid;
	gid_t gid;

	GRefString *owner;
	GRefString *owner_real;
	GRefString *group;
	
	time_t atime; /* 0 is unknown */
	time_t mtime; /* 0 is unknown */
	time_t btime; /* 0 is unknown */
	
	char *symlink_name;
	
	GRefString *mime_type;
	
	char *selinux_context;
	
	GError *get_info_error;
	
	guint directory_count;

	guint deep_directory_count;
	guint deep_file_count;
	guint deep_unreadable_count;
	goffset deep_size;

	GIcon *icon;
	
	char *thumbnail_path;
	GdkTexture *thumbnail;
	time_t thumbnail_mtime;

	char *activation_uri;

	/* used during DND, for checking whether source and destination are on
	 * the same file system.
	 */
	GRefString *filesystem_id;

	char *trash_orig_path;

	/* The following is for file operations in progress. Since
	 * there are normally only a few of these, we can move them to
	 * a separate hash table or something if required to keep the
	 * file objects small.
	 */
	GList *operations_in_progress;

	/* AmmoniteInfoProviders that need to be run for this file */
	GList *pending_info_providers;

	/* Emblems provided by extensions */
	GList *extension_emblems;

	/* Attributes provided by extensions */
	GHashTable *extension_attributes;

	GHashTable *metadata;

	/* Mount for mountpoint or the references GMount for a "mountable" */
	GMount *mount;
	
	/* boolean fields: bitfield to save space, since there can be
           many AmmoniteFile objects. */

	guint unconfirmed                   : 1;
	guint has_been_unmounted            : 1;
	guint is_gone                       : 1;
	/* Set when emitting files_added on the directory to make sure we
	   add a file, and only once */
	guint is_added                      : 1;
	/* Set by the AmmoniteDirectory while it's loading the file
	 * list so the file knows not to do redundant I/O.
	 */
	guint loading_directory             : 1;
	guint got_file_info                 : 1;
	guint get_info_failed               : 1;
	guint file_info_is_up_to_date       : 1;
	
	guint got_directory_count           : 1;
	guint directory_count_failed        : 1;
	guint directory_count_is_up_to_date : 1;

	guint deep_counts_status      : 2; /* AmmoniteRequestStatus */
	/* no deep_counts_are_up_to_date field; since we expose
           intermediate values for this attribute, we do actually
           forget it rather than invalidating. */

	guint mount_is_up_to_date           : 1;
	
	guint got_custom_display_name       : 1;

        guint thumbnail_info_is_up_to_date  : 1;
	guint thumbnail_is_up_to_date       : 1;
	guint thumbnailing_failed           : 1;

	guint is_symlink                    : 1;
	guint is_mountpoint                 : 1;
	guint is_hidden                     : 1;

	guint has_permissions               : 1;
	
	guint can_read                      : 1;
	guint can_write                     : 1;
	guint can_execute                   : 1;
	guint can_delete                    : 1;
	guint can_trash                     : 1;
	guint can_rename                    : 1;
	guint can_mount                     : 1;
	guint can_unmount                   : 1;
	guint can_eject                     : 1;
	guint can_start                     : 1;
	guint can_start_degraded            : 1;
	guint can_stop                      : 1;
	guint start_stop_type               : 3; /* GDriveStartStopType */
	guint can_poll_for_media            : 1;
	guint is_media_check_automatic      : 1;
	guint has_preview_icon              : 1;

	guint filesystem_readonly           : 1;
	guint filesystem_use_preview        : 2; /* GFilesystemPreviewType */
	guint filesystem_info_is_up_to_date : 1;
	guint filesystem_remote             : 1;

	time_t trash_time; /* 0 is unknown */
	time_t recency; /* 0 is unknown */

	gdouble search_relevance;
	gchar *fts_snippet;

	guint64 free_space; /* (guint)-1 for unknown */
	time_t free_space_read; /* The time free_space was updated, or 0 for never */
};

typedef struct {
	AmmoniteFile *file;
	GList *files;
	guint renamed_files;
	guint skipped_files;
	GCancellable *cancellable;
	AmmoniteFileOperationCallback callback;
	gpointer callback_data;
	gboolean is_rename;
	
	gpointer data;
	GDestroyNotify free_data;
	AmmoniteFileUndoInfo *undo_info;
} AmmoniteFileOperation;

AmmoniteFile *ammonite_file_new_from_info                  (AmmoniteDirectory      *directory,
							    GFileInfo              *info);
void          ammonite_file_emit_changed                   (AmmoniteFile           *file);
void          ammonite_file_mark_unmounted                 (AmmoniteFile           *file);
void          ammonite_file_mark_gone                      (AmmoniteFile           *file);

GDateTime *   ammonite_file_get_date                       (AmmoniteFile           *file,
							    AmmoniteDateType        date_type);
void          ammonite_file_updated_deep_count_in_progress (AmmoniteFile           *file);


void          ammonite_file_clear_info                     (AmmoniteFile           *file);
/* Compare file's state with a fresh file info struct, return FALSE if
 * no change, update file and return TRUE if the file info contains
 * new state.  */
gboolean      ammonite_file_update_info                    (AmmoniteFile           *file,
							    GFileInfo              *info);
gboolean      ammonite_file_update_thumbnail_info          (AmmoniteFile           *file,
                                                            GFileInfo              *info);
gboolean      ammonite_file_update_metadata_from_info      (AmmoniteFile           *file,
							    GFileInfo              *info);

gboolean      ammonite_file_update_name_and_directory      (AmmoniteFile           *file,
							    const char             *name,
							    AmmoniteDirectory      *directory);

gboolean      ammonite_file_set_display_name               (AmmoniteFile           *file,
							    const char             *display_name,
							    const char             *edit_name,
							    gboolean                custom);
void          ammonite_file_set_directory                  (AmmoniteFile           *file,
							    AmmoniteDirectory      *directory);
void          ammonite_file_set_mount                      (AmmoniteFile           *file,
							    GMount                 *mount);

/* Mark specified attributes for this file out of date without canceling current
 * I/O or kicking off new I/O.
 */
void                   ammonite_file_invalidate_attributes_internal     (AmmoniteFile           *file,
									 AmmoniteAttributes      attributes);
AmmoniteAttributes     ammonite_file_get_all_attributes                 (void);
gboolean               ammonite_file_is_self_owned                      (AmmoniteFile           *file);
void                   ammonite_file_invalidate_count                   (AmmoniteFile           *file);
gboolean               ammonite_file_rename_in_progress                 (AmmoniteFile           *file);
void                   ammonite_file_invalidate_extension_info_internal (AmmoniteFile           *file);

/* Thumbnailing: */
gboolean          ammonite_file_set_thumbnail              (AmmoniteFile           *file,
                                                            GdkTexture             *texture,
                                                            time_t                  mtime);

AmmoniteFileOperation *ammonite_file_operation_new      (AmmoniteFile                  *file,
							 AmmoniteFileOperationCallback  callback,
							 gpointer                       callback_data);
void                   ammonite_file_operation_free     (AmmoniteFileOperation         *op);
void                   ammonite_file_operation_complete (AmmoniteFileOperation         *op,
							 GFile                         *result_location,
							 GError                        *error);
void                   ammonite_file_operation_cancel   (AmmoniteFileOperation         *op);
