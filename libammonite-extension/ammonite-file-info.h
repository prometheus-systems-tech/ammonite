/*
 * ammonite-file-info.h - Information about a file
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* AmmoniteFileInfo is an interface to the AmmoniteFile object.  It 
 * provides access to the asynchronous data in the AmmoniteFile.
 * Extensions are passed objects of this type for operations. */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_FILE_INFO (ammonite_file_info_get_type ())

G_DECLARE_INTERFACE (AmmoniteFileInfo, ammonite_file_info, AMMONITE, FILE_INFO, GObject)

struct _AmmoniteFileInfoInterface
{
    GTypeInterface g_iface;

    gboolean          (*is_gone)                   (AmmoniteFileInfo *file_info);

    char             *(*get_name)                  (AmmoniteFileInfo *file_info);
    char             *(*get_uri)                   (AmmoniteFileInfo *file_info);
    char             *(*get_parent_uri)            (AmmoniteFileInfo *file_info);
    char             *(*get_uri_scheme)            (AmmoniteFileInfo *file_info);

    char             *(*get_mime_type)             (AmmoniteFileInfo *file_info);
    gboolean          (*is_mime_type)              (AmmoniteFileInfo *file_info,
                                                    const char       *mime_type);
    gboolean          (*is_directory)              (AmmoniteFileInfo *file_info);

    void              (*add_emblem)                (AmmoniteFileInfo *file_info,
                                                    const char       *emblem_name);
    char             *(*get_string_attribute)      (AmmoniteFileInfo *file_info,
                                                    const char       *attribute_name);
    void              (*add_string_attribute)      (AmmoniteFileInfo *file_info,
                                                    const char       *attribute_name,
                                                    const char       *value);
    void              (*invalidate_extension_info) (AmmoniteFileInfo *file_info);

    char             *(*get_activation_uri)        (AmmoniteFileInfo *file_info);

    GFileType         (*get_file_type)             (AmmoniteFileInfo *file_info);
    GFile            *(*get_location)              (AmmoniteFileInfo *file_info);
    GFile            *(*get_parent_location)       (AmmoniteFileInfo *file_info);
    AmmoniteFileInfo *(*get_parent_info)           (AmmoniteFileInfo *file_info);
    GMount           *(*get_mount)                 (AmmoniteFileInfo *file_info);
    gboolean          (*can_write)                 (AmmoniteFileInfo *file_info);
};

GList            *ammonite_file_info_list_copy            (GList            *files);
void              ammonite_file_info_list_free            (GList            *files);
gboolean          ammonite_file_info_is_gone              (AmmoniteFileInfo *file_info);
GFileType         ammonite_file_info_get_file_type        (AmmoniteFileInfo *file_info);
GFile            *ammonite_file_info_get_location         (AmmoniteFileInfo *file_info);
char             *ammonite_file_info_get_name             (AmmoniteFileInfo *file_info);
char             *ammonite_file_info_get_uri              (AmmoniteFileInfo *file_info);
char             *ammonite_file_info_get_activation_uri   (AmmoniteFileInfo *file_info);
GFile            *ammonite_file_info_get_parent_location  (AmmoniteFileInfo *file_info);
char             *ammonite_file_info_get_parent_uri       (AmmoniteFileInfo *file_info);
GMount           *ammonite_file_info_get_mount            (AmmoniteFileInfo *file_info);
char             *ammonite_file_info_get_uri_scheme       (AmmoniteFileInfo *file_info);
AmmoniteFileInfo *ammonite_file_info_get_parent_info      (AmmoniteFileInfo *file_info);
char *            ammonite_file_info_get_mime_type        (AmmoniteFileInfo *file_info);
gboolean          ammonite_file_info_is_mime_type         (AmmoniteFileInfo *file_info,
                                                           const char       *mime_type);
gboolean          ammonite_file_info_is_directory         (AmmoniteFileInfo *file_info);
gboolean          ammonite_file_info_can_write            (AmmoniteFileInfo *file_info);
void              ammonite_file_info_add_emblem           (AmmoniteFileInfo *file_info,
                                                           const char       *emblem_name);
char             *ammonite_file_info_get_string_attribute (AmmoniteFileInfo *file_info,
                                                           const char       *attribute_name);
void              ammonite_file_info_add_string_attribute (AmmoniteFileInfo *file_info,
                                                           const char       *attribute_name,
                                                           const char       *value);
void              ammonite_file_info_invalidate_extension_info (AmmoniteFileInfo *file_info);
AmmoniteFileInfo *ammonite_file_info_lookup                (GFile *location);
AmmoniteFileInfo *ammonite_file_info_create                (GFile *location);
AmmoniteFileInfo *ammonite_file_info_lookup_for_uri        (const char *uri);
AmmoniteFileInfo *ammonite_file_info_create_for_uri        (const char *uri);
G_END_DECLS
