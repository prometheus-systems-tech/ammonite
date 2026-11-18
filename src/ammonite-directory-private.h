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

#include "ammonite-directory.h"
#include "ammonite-file.h"

#include <gio/gio.h>
#include <ammonite-extension.h>

typedef struct FileMonitors FileMonitors;
typedef struct DirectoryLoadState DirectoryLoadState;
typedef struct DirectoryCountState DirectoryCountState;
typedef struct DeepCountState DeepCountState;
typedef struct GetInfoState GetInfoState;
typedef struct NewFilesState NewFilesState;
typedef struct ThumbnailInfoState ThumbnailInfoState;
typedef struct ThumbnailBufState ThumbnailBufState;
typedef struct MountState MountState;
typedef struct FilesystemInfoState FilesystemInfoState;

/* Counts number of attribute requests of one or more files. */
typedef gint32 RequestCounter[AMMONITE_ATTRIBUTE_N_TOTAL];

struct AmmoniteDirectoryPrivate
{
	/* The location. */
	GFile *location;

	/* The file objects. */
	AmmoniteFile *as_file;
	GList *file_list;
	GHashTable *file_hash;

	/* Queues of files needing some I/O done. */
	AmmoniteHashQueue *high_priority_queue;
	AmmoniteHashQueue *low_priority_queue;
	AmmoniteHashQueue *extension_queue;

	/* Callbacks are inserted into ready when the callback is triggered and
	 * scheduled to be called at idle. It's still kept in the hash table so we
	 * can kill it when the file goes away before being called. The hash table
	 * uses the file pointer of the ReadyCallback as a key.
	 */
	struct
	{
		GHashTable *unsatisfied;
		GHashTable *ready;
	} call_when_ready_hash;
	RequestCounter call_when_ready_counters;
	GHashTable *monitor_table;
	RequestCounter monitor_counters;
	guint call_ready_idle_id;

	AmmoniteMonitor *monitor;
	gulong 		 mime_db_monitor;

	gboolean in_async_service_loop;
	gboolean state_changed;

	gboolean file_list_monitored;
	gboolean directory_loaded;
	gboolean directory_loaded_sent_notification;
	DirectoryLoadState *directory_load_in_progress;

	GList *pending_file_info; /* list of GnomeVFSFileInfo's that are pending */
	int confirmed_file_count;
        guint dequeue_pending_idle_id;

	GList *new_files_in_progress; /* list of NewFilesState * */

	/* List of GFile's that received CHANGE events while new files were being added in
	 * that same folder. We will process this CHANGE events after new_files_in_progress
	 * list is finished. See Bug 703179 and issue #1576 for a case when this happens.
	 */
	GList *files_changed_while_adding;

	DirectoryCountState *count_in_progress;

	AmmoniteFile *deep_count_file;
	DeepCountState *deep_count_in_progress;

	AmmoniteFile *get_info_file;
	GetInfoState *get_info_in_progress;

	AmmoniteFile *extension_info_file;
	AmmoniteInfoProvider *extension_info_provider;
	AmmoniteOperationHandle *extension_info_in_progress;
	guint extension_info_idle;

	ThumbnailInfoState *thumbnail_info_state;

	ThumbnailBufState *thumbnail_buf_state;

	MountState *mount_state;

	FilesystemInfoState *filesystem_info_state;

	GList *file_operations_in_progress; /* list of FileOperation * */
};

AmmoniteDirectory *ammonite_directory_get_existing                    (GFile                     *location);

/* async. interface */
void               ammonite_directory_async_state_changed             (AmmoniteDirectory         *directory);
void               ammonite_directory_call_when_ready_internal        (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file,
								       AmmoniteAttributes         attributes,
								       AmmoniteDirectoryCallback  directory_callback,
								       AmmoniteFileCallback       file_callback,
								       gpointer                   callback_data);
gboolean           ammonite_directory_check_if_ready_internal         (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file,
								       AmmoniteAttributes         attributes);
void               ammonite_directory_cancel_callback_internal        (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file,
								       AmmoniteDirectoryCallback  directory_callback,
								       AmmoniteFileCallback       file_callback,
								       gpointer                   callback_data);
void               ammonite_directory_monitor_add_internal            (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file,
								       gconstpointer              client,
								       gboolean                   monitor_hidden_files,
								       AmmoniteAttributes         attributes,
								       AmmoniteDirectoryCallback  callback,
								       gpointer                   callback_data);
void               ammonite_directory_monitor_remove_internal         (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file,
								       gconstpointer              client);
void               ammonite_directory_get_info_for_new_files          (AmmoniteDirectory         *directory,
								       GList                     *vfs_uris);
AmmoniteFile *     ammonite_directory_get_existing_corresponding_file (AmmoniteDirectory         *directory);
void               ammonite_directory_invalidate_count                (AmmoniteDirectory         *directory);
gboolean           ammonite_directory_is_file_list_monitored          (AmmoniteDirectory         *directory);
gboolean           ammonite_directory_is_anyone_monitoring_file_list  (AmmoniteDirectory         *directory);
gboolean           ammonite_directory_has_request_for_file            (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file);
void               ammonite_directory_schedule_dequeue_pending        (AmmoniteDirectory         *directory);
void               ammonite_directory_stop_monitoring_file_list       (AmmoniteDirectory         *directory);
void               ammonite_directory_cancel                          (AmmoniteDirectory         *directory);
void               ammonite_async_destroying_file                     (AmmoniteFile              *file);
void               ammonite_directory_force_reload_internal           (AmmoniteDirectory         *directory,
								       AmmoniteAttributes         attributes);
void               ammonite_directory_cancel_loading_attributes       (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file,
								       AmmoniteAttributes         attributes);

/* Calls shared between directory, file, and async. code. */
void               ammonite_directory_emit_files_added                (AmmoniteDirectory         *directory,
								       GList                     *added_files);
void               ammonite_directory_emit_files_changed              (AmmoniteDirectory         *directory,
								       GList                     *changed_files);
void               ammonite_directory_emit_change_signals             (AmmoniteDirectory         *directory,
								       AmmoniteFileList          *changed_files);
void               emit_change_signals_for_all_files		      (AmmoniteDirectory	 *directory);
void               emit_change_signals_for_all_files_in_all_directories (void);
void               ammonite_directory_emit_done_loading               (AmmoniteDirectory         *directory);
void               ammonite_directory_emit_load_error                 (AmmoniteDirectory         *directory,
								       GError                    *error);
char *             ammonite_directory_get_name_for_self_as_new_file   (AmmoniteDirectory         *directory);

/* Interface to the file list. */
AmmoniteFile *     ammonite_directory_find_file_by_name               (AmmoniteDirectory         *directory,
								       const char                *filename);
AmmoniteFile *     ammonite_directory_new_as_vfs_file                 (AmmoniteDirectory         *directory);

void               ammonite_directory_add_file                        (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file);
void               ammonite_directory_remove_file                     (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file);
FileMonitors *     ammonite_directory_remove_file_monitors            (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file);
void               ammonite_directory_add_file_monitors               (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file,
								       FileMonitors              *monitors);
GList *            ammonite_directory_begin_file_name_change          (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file);
void               ammonite_directory_end_file_name_change            (AmmoniteDirectory         *directory,
								       AmmoniteFile              *file,
								       GList                     *node);
void               ammonite_directory_moved                           (const char                *from_uri,
								       const char                *to_uri);
/* Interface to the work queue. */

void               ammonite_directory_add_file_to_work_queue          (AmmoniteDirectory *directory,
								       AmmoniteFile *file);
void               ammonite_directory_remove_file_from_work_queue     (AmmoniteDirectory *directory,
								       AmmoniteFile *file);

/* debugging functions */
int                ammonite_directory_number_outstanding              (void);
