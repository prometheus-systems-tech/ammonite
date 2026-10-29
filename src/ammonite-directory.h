/*
 * SPDX-FileCopyrightText: 1999, 2000, 2001 Eazel, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#pragma once

#include <gio/gio.h>

#include "ammonite-enums.h"

/* AmmoniteDirectory is a class that manages the model for a directory,
   real or virtual, for Ammonite, mainly the file-manager component. The directory is
   responsible for managing both real data and cached metadata. On top of
   the file system independence provided by gio, the directory
   object also provides:
  
       1) A synchronization framework, which notifies via signals as the
          set of known files changes.
       2) An abstract interface for getting attributes and performing
          operations on files.
*/

#define AMMONITE_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME "ammonite-directory-provider"

#define AMMONITE_TYPE_DIRECTORY ammonite_directory_get_type()
#define AMMONITE_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), AMMONITE_TYPE_DIRECTORY, AmmoniteDirectory))
#define AMMONITE_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), AMMONITE_TYPE_DIRECTORY, AmmoniteDirectoryClass))
#define AMMONITE_IS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AMMONITE_TYPE_DIRECTORY))
#define AMMONITE_IS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), AMMONITE_TYPE_DIRECTORY))
#define AMMONITE_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), AMMONITE_TYPE_DIRECTORY, AmmoniteDirectoryClass))

/* AmmoniteFile is defined both here and in ammonite-file.h. */
#ifndef AMMONITE_FILE_DEFINED
#define AMMONITE_FILE_DEFINED
typedef struct AmmoniteFile AmmoniteFile;
#endif

typedef struct _AmmoniteDirectory        AmmoniteDirectory;
typedef struct  AmmoniteDirectoryPrivate AmmoniteDirectoryPrivate;

struct _AmmoniteDirectory
{
	GObject object;
	AmmoniteDirectoryPrivate *details;
};

typedef void (*AmmoniteDirectoryCallback) (AmmoniteDirectory *directory,
					   GList             *files,
					   gpointer           callback_data);

typedef struct
{
	GObjectClass parent_class;

	/*** Notification signals for clients to connect to. ***/

	/* The files_added signal is emitted as the directory model 
	 * discovers new files.
	 */
	void     (* files_added)         (AmmoniteDirectory          *directory,
					  GList                      *added_files);

	/* The files_changed signal is emitted as changes occur to
	 * existing files that are noticed by the synchronization framework,
	 * including when an old file has been deleted. When an old file
	 * has been deleted, this is the last chance to forget about these
	 * file objects, which are about to be unref'd. Use a call to
	 * ammonite_file_is_gone () to test for this case.
	 */
	void     (* files_changed)       (AmmoniteDirectory         *directory,
					  GList                     *changed_files);

	/* The done_loading signal is emitted when a directory load
	 * request completes. This is needed because, at least in the
	 * case where the directory is empty, the caller will receive
	 * no kind of notification at all when a directory load
	 * initiated by `ammonite_directory_file_monitor_add' completes.
	 */
	void     (* done_loading)        (AmmoniteDirectory         *directory);

	void     (* load_error)          (AmmoniteDirectory         *directory,
					  GError                    *error);

	/*** Virtual functions for subclasses to override. ***/
	gboolean (* contains_file)       (AmmoniteDirectory         *directory,
					  AmmoniteFile              *file);
	void     (* call_when_ready)     (AmmoniteDirectory         *directory,
					  AmmoniteAttributes         attributes,
					  AmmoniteDirectoryCallback  callback,
					  gpointer                   callback_data);
	void     (* cancel_callback)     (AmmoniteDirectory         *directory,
					  AmmoniteDirectoryCallback  callback,
					  gpointer                   callback_data);
	void     (* file_monitor_add)    (AmmoniteDirectory          *directory,
					  gconstpointer              client,
					  gboolean                   monitor_hidden_files,
					  AmmoniteAttributes         monitor_attributes,
					  AmmoniteDirectoryCallback  initial_files_callback,
					  gpointer                   callback_data);
	void     (* file_monitor_remove) (AmmoniteDirectory         *directory,
					  gconstpointer              client);
	void     (* force_reload)        (AmmoniteDirectory         *directory);
	gboolean (* are_all_files_seen)  (AmmoniteDirectory         *directory);
	gboolean (* is_not_empty)        (AmmoniteDirectory         *directory);

	/* get_file_list is a function pointer that subclasses may override to
	 * customize collecting the list of files in a directory.
	 * For example, the AmmoniteDesktopDirectory overrides this so that it can
	 * merge together the list of files in the $HOME/Desktop directory with
	 * the list of standard icons (Home, Trash) on the desktop.
	 */
	GList *	 (* get_file_list)	 (AmmoniteDirectory *directory);

	/* Should return FALSE if the directory is read-only and doesn't
	 * allow setting of metadata.
	 * An example of this is the search directory.
	 */
	gboolean (* is_editable)         (AmmoniteDirectory *directory);

        /* Subclasses can use this to create custom files when asked by the user
         * or the ammonite cache. */
        AmmoniteFile * (* new_as_file) (AmmoniteDirectory *directory);
        /* Subclasses can say if they handle the location provided or should the
         * ammonite file class handle it.
         */
        gboolean       (* handles_location)       (GFile             *location);
} AmmoniteDirectoryClass;

/* Basic GObject requirements. */
GType              ammonite_directory_get_type                 (void);

/* Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
AmmoniteDirectory *ammonite_directory_get                      (GFile                     *location);
AmmoniteDirectory *ammonite_directory_get_by_uri               (const char                *uri);
AmmoniteDirectory *ammonite_directory_get_for_file             (AmmoniteFile              *file);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
AmmoniteDirectory *ammonite_directory_ref                      (AmmoniteDirectory         *directory);
void               ammonite_directory_unref                    (AmmoniteDirectory         *directory);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AmmoniteDirectory, ammonite_directory_unref)

/* Access to a URI. */
char *             ammonite_directory_get_uri                  (AmmoniteDirectory         *directory);
GFile *            ammonite_directory_get_location             (AmmoniteDirectory         *directory);

/* Is this file still alive and in this directory? */
gboolean           ammonite_directory_contains_file            (AmmoniteDirectory         *directory,
								AmmoniteFile              *file);

AmmoniteFile*           ammonite_directory_get_file_by_name            (AmmoniteDirectory *directory,
                                                                        const gchar       *name);
/* Get (and ref) a AmmoniteFile object for this directory. */
AmmoniteFile *     ammonite_directory_get_corresponding_file   (AmmoniteDirectory         *directory);

/* Waiting for data that's read asynchronously.
 * The file attribute and metadata keys are for files in the directory.
 */
void               ammonite_directory_call_when_ready          (AmmoniteDirectory         *directory,
								AmmoniteAttributes         attributes,
								AmmoniteDirectoryCallback  callback,
								gpointer                   callback_data);
void               ammonite_directory_cancel_callback          (AmmoniteDirectory         *directory,
								AmmoniteDirectoryCallback  callback,
								gpointer                   callback_data);


/* Monitor the files in a directory. */
void               ammonite_directory_file_monitor_add         (AmmoniteDirectory         *directory,
								gconstpointer              client,
								gboolean                   monitor_hidden_files,
								AmmoniteAttributes         attributes,
								AmmoniteDirectoryCallback  initial_files_callback,
								gpointer                   callback_data);
void               ammonite_directory_file_monitor_remove      (AmmoniteDirectory         *directory,
								gconstpointer              client);
void               ammonite_directory_force_reload             (AmmoniteDirectory         *directory);

/* Get a list of all files currently known in the directory. */
GList *            ammonite_directory_get_file_list            (AmmoniteDirectory         *directory);

GList *            ammonite_directory_match_pattern            (AmmoniteDirectory         *directory,
							        const char *glob);


/* Return true if the directory has information about all the files.
 * This will be false until the directory has been read at least once.
 */
gboolean           ammonite_directory_are_all_files_seen       (AmmoniteDirectory         *directory);

gboolean           ammonite_directory_is_local_or_fuse         (AmmoniteDirectory         *directory);

gboolean           ammonite_directory_is_in_trash              (AmmoniteDirectory         *directory);
gboolean           ammonite_directory_is_in_recent             (AmmoniteDirectory         *directory);
gboolean           ammonite_directory_is_in_starred            (AmmoniteDirectory         *directory);
gboolean           ammonite_directory_is_in_admin              (AmmoniteDirectory         *directory);

/* Return false if directory contains anything besides a Ammonite metafile.
 * Only valid if directory is monitored. Used by the Trash monitor.
 */
gboolean           ammonite_directory_is_not_empty             (AmmoniteDirectory         *directory);

/* Convenience functions for dealing with a list of AmmoniteDirectory objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
GList *            ammonite_directory_list_ref                 (GList                     *directory_list);
void               ammonite_directory_list_unref               (GList                     *directory_list);
void               ammonite_directory_list_free                (GList                     *directory_list);
GList *            ammonite_directory_list_copy                (GList                     *directory_list);
GList *            ammonite_directory_list_sort_by_uri         (GList                     *directory_list);

gboolean           ammonite_directory_is_editable              (AmmoniteDirectory         *directory);

void               ammonite_directory_dump                     (AmmoniteDirectory         *directory);

AmmoniteFile *     ammonite_directory_new_as_file              (AmmoniteDirectory *directory);
