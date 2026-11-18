/*
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "ammonite-file-info.h"

#include "ammonite-extension-private.h"

/**
 * AmmoniteFileInfo:
 *
 * File interface for ammonite extensions.
 *
 * `AmmoniteFileInfo` provides methods to get and modify information
 * about file objects in the file manager.
 */

G_DEFINE_INTERFACE (AmmoniteFileInfo, ammonite_file_info, G_TYPE_OBJECT)

AmmoniteFileInfo * (*ammonite_file_info_getter)(GFile   *location,
                                                gboolean create);

/**
 * ammonite_file_info_list_copy:
 * @files: (element-type AmmoniteFileInfo): the files to copy
 *
 * Deep copy a list of `AmmoniteFileInfo`.
 *
 * Returns: (element-type AmmoniteFileInfo) (transfer full): a copy of @files.
 *  Use [func@FileInfo.list_free] to free the list and unref its contents.
 */
GList *
ammonite_file_info_list_copy (GList *files)
{
    GList *ret;
    GList *l;

    ret = g_list_copy (files);
    for (l = ret; l != NULL; l = l->next)
    {
        g_object_ref (G_OBJECT (l->data));
    }

    return ret;
}

/**
 * ammonite_file_info_list_free:
 * @files: (element-type AmmoniteFileInfo): a list created with [func@FileInfo.list_copy]
 *
 * Deep free a list of `AmmoniteFileInfo`.
 *
 */
void
ammonite_file_info_list_free (GList *files)
{
    GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        g_object_unref (G_OBJECT (l->data));
    }

    g_list_free (files);
}

static void
ammonite_file_info_default_init (AmmoniteFileInfoInterface *klass)
{
}

/**
 * ammonite_file_info_is_gone:
 *
 * Get whether a `AmmoniteFileInfo` is gone.
 *
 * Returns: whether the file is gone.
 */
gboolean
ammonite_file_info_is_gone (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), FALSE);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->is_gone != NULL, FALSE);

    return iface->is_gone (self);
}

/**
 * ammonite_file_info_get_file_type:
 *
 * Get the cached [enum@Gio.FileType].
 *
 * Returns: the file type
 */
GFileType
ammonite_file_info_get_file_type (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), G_FILE_TYPE_UNKNOWN);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_file_type != NULL, G_FILE_TYPE_UNKNOWN);

    return iface->get_file_type (self);
}

/**
 * ammonite_file_info_get_name:
 *
 * Gets the name.
 *
 * Returns: the file name of @file_info
 */
char *
ammonite_file_info_get_name (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_name != NULL, NULL);

    return iface->get_name (self);
}

/**
 * ammonite_file_info_get_location:
 *
 * Get the corresponding [iface@Gio.File]
 *
 * Returns: (transfer full): the corresponding location.
 */
GFile *
ammonite_file_info_get_location (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_location != NULL, NULL);

    return iface->get_location (self);
}

/**
 * ammonite_file_info_get_uri:
 *
 * Gets the URI.
 *
 * Returns: the file URI of @file_info
 */
char *
ammonite_file_info_get_uri (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_uri != NULL, NULL);

    return iface->get_uri (self);
}

/**
 * ammonite_file_info_get_activation_uri:
 *
 * Gets the activation uri.
 *
 * The activation uri may differ from the actual URI if e.g. the file is a .desktop
 * file or a Ammonite XML link file.
 *
 * Returns: the activation URI of @file_info
 */
char *
ammonite_file_info_get_activation_uri (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_activation_uri != NULL, NULL);

    return iface->get_activation_uri (self);
}

/**
 * ammonite_file_info_get_parent_location:
 *
 * Gets the parent location.
 *
 * Returns: (allow-none) (transfer full): a #GFile for the parent location of @file_info,
 *   or %NULL if @file_info has no parent
 */
GFile *
ammonite_file_info_get_parent_location (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_location != NULL, NULL);

    return iface->get_parent_location (self);
}

/**
 * ammonite_file_info_get_parent_uri:
 *
 * Get the parent `AmmoniteFileInfo` uri.
 *
 * Returns: the URI for the parent location of @file_info, or the empty string
 *   if it has none
 */
char *
ammonite_file_info_get_parent_uri (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_uri != NULL, NULL);

    return iface->get_parent_uri (self);
}

/**
 * ammonite_file_info_get_parent_info:
 *
 * Get the parent `AmmoniteFileInfo`.
 *
 * It's not safe to call this recursively multiple times, as it works
 * only for files already cached by Ammonite.
 *
 * Returns: (nullable) (transfer full): a #AmmoniteFileInfo for the parent of @file_info,
 *                                      or %NULL if @file_info has no parent.
 */
AmmoniteFileInfo *
ammonite_file_info_get_parent_info (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_parent_info != NULL, NULL);

    return iface->get_parent_info (self);
}

/**
 * ammonite_file_info_get_mount:
 *
 * Gets the cached mount.
 *
 * This only returns the [iface@Gio.Mount] if Ammonite has already cached it.
 * The return value may be `NULL` even if the `AmmoniteFileInfo` has a corresponding
 * mount in which case you can call [method@Gio.File.find_enclosing_mount_async].
 *
 * Returns: (nullable) (transfer full): the mount of @file_info,
 *                                      or %NULL if @file_info has no mount
 */
GMount *
ammonite_file_info_get_mount (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_mount != NULL, NULL);

    return iface->get_mount (self);
}

/**
 * ammonite_file_info_get_uri_scheme:
 *
 * Get the uri scheme.
 *
 * Returns: the URI scheme of @file_info
 */
char *
ammonite_file_info_get_uri_scheme (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_uri_scheme != NULL, NULL);

    return iface->get_uri_scheme (self);
}

/**
 * ammonite_file_info_get_mime_type:
 *
 * Get the cached mime_type.
 *
 * Returns: (transfer full): the MIME type of @file_info
 */
char *
ammonite_file_info_get_mime_type (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_mime_type != NULL, NULL);

    return iface->get_mime_type (self);
}

/**
 * ammonite_file_info_is_mime_type:
 *
 * Gets whether the mime_type of the `AmmoniteFileInfo` matches the given type.
 *
 * Returns: %TRUE when the MIME type of @file_info matches @mime_type, and
 *   %FALSE otherwise
 */
gboolean
ammonite_file_info_is_mime_type (AmmoniteFileInfo *self,
                                 const char       *mime_type)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), FALSE);
    g_return_val_if_fail (mime_type != NULL, FALSE);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->is_mime_type != NULL, FALSE);

    return iface->is_mime_type (self, mime_type);
}

/**
 * ammonite_file_info_is_directory:
 *
 * Gets whether the `AmmoniteFileInfo` is a directory.
 *
 * Uses the cached [enum@Gio.FileType] matches `G_FILE_TYPE_DIRECTORY` without
 * doing any blocking i/o.
 *
 * Returns: %TRUE when @file_info is a directory, and %FALSE otherwise
 */
gboolean
ammonite_file_info_is_directory (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), FALSE);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->is_directory != NULL, FALSE);

    return iface->is_directory (self);
}

/**
 * ammonite_file_info_can_write:
 *
 * Gets whether the `AmmoniteFileInfo` is writeable.
 *
 * Returns: %TRUE when @file_info is writeable, and %FALSE otherwise
 */
gboolean
ammonite_file_info_can_write (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), FALSE);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->can_write != NULL, FALSE);

    return iface->can_write (self);
}

/**
 * ammonite_file_info_add_emblem:
 * @emblem_name: the name of an emblem
 *
 * Add an emblem.
 */
void
ammonite_file_info_add_emblem (AmmoniteFileInfo *self,
                               const char       *emblem_name)
{
    AmmoniteFileInfoInterface *iface;

    g_return_if_fail (AMMONITE_IS_FILE_INFO (self));
    g_return_if_fail (emblem_name != NULL && emblem_name[0] != '\0');

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_if_fail (iface->add_emblem != NULL);

    iface->add_emblem (self, emblem_name);
}

/**
 * ammonite_file_info_get_string_attribute:
 * @attribute_name: the name of an attribute
 *
 * Get the attribute's value.
 *
 * Returns: (nullable): the value for the given @attribute_name, or %NULL if
 *   there is none
 */
char *
ammonite_file_info_get_string_attribute (AmmoniteFileInfo *self,
                                         const char       *attribute_name)
{
    AmmoniteFileInfoInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_FILE_INFO (self), NULL);
    g_return_val_if_fail (attribute_name != NULL, NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_val_if_fail (iface->get_string_attribute != NULL, NULL);

    return iface->get_string_attribute (self, attribute_name);
}

/**
 * ammonite_file_info_add_string_attribute:
 * @attribute_name: the name of an attribute
 * @value: the value of an attribute
 *
 * Set's the attributes value or replacing the existing value (if one exists).
 *
 * This function is necessary to set the value of the `AmmoniteFileInfo`'s
 * correspond attribute for a [property@Column:attribute].
 */
void
ammonite_file_info_add_string_attribute (AmmoniteFileInfo *self,
                                         const char       *attribute_name,
                                         const char       *value)
{
    AmmoniteFileInfoInterface *iface;

    g_return_if_fail (AMMONITE_IS_FILE_INFO (self));
    g_return_if_fail (attribute_name != NULL);
    g_return_if_fail (value != NULL);

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_if_fail (iface->add_string_attribute != NULL);

    iface->add_string_attribute (self, attribute_name, value);
}

/**
 * ammonite_file_info_invalidate_extension_info:
 *
 * Invalidate the current extension information.
 *
 * This removes any information, such as emblems or or string attributes, that
 * were added to the `AmmoniteFileInfo` from any extension.
 *
 */
void
ammonite_file_info_invalidate_extension_info (AmmoniteFileInfo *self)
{
    AmmoniteFileInfoInterface *iface;

    g_return_if_fail (AMMONITE_IS_FILE_INFO (self));

    iface = AMMONITE_FILE_INFO_GET_IFACE (self);

    g_return_if_fail (iface->invalidate_extension_info != NULL);

    iface->invalidate_extension_info (self);
}

/**
 * ammonite_file_info_lookup:
 * @location: the location for which to look up a corresponding #AmmoniteFileInfo object
 *
 * Get an existing `AmmoniteFileInfo` or `NULL` if it does not exist in the
 * application cache.
 *
 * Returns: (nullable) (transfer full):
 */
AmmoniteFileInfo *
ammonite_file_info_lookup (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return ammonite_file_info_getter (location, FALSE);
}

/**
 * ammonite_file_info_create:
 * @location: the location to create the file info for
 *
 * Get an existing `AmmoniteFileInfo` (if it exists) or create a new one is it
 * does not exist.
 *
 * Returns: (transfer full):
 */
AmmoniteFileInfo *
ammonite_file_info_create (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return ammonite_file_info_getter (location, TRUE);
}

/**
 * ammonite_file_info_lookup_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Get an existing `AmmoniteFileInfo` or `NULL` if it does not exist in the
 * application cache.
 *
 * Returns: (nullable) (transfer full):
 */
AmmoniteFileInfo *
ammonite_file_info_lookup_for_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (uri != NULL, NULL);

    location = g_file_new_for_uri (uri);

    return ammonite_file_info_lookup (location);
}

/**
 * ammonite_file_info_create_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Get an existing `AmmoniteFileInfo` (if it exists) or create a new one is it
 * does not exist.
 *
 * Returns: (transfer full):
 */
AmmoniteFileInfo *
ammonite_file_info_create_for_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (uri != NULL, NULL);

    location = g_file_new_for_uri (uri);

    return ammonite_file_info_create (location);
}
