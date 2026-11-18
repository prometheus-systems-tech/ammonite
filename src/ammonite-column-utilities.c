/*
 * SPDX-FileCopyrightText: 2004 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Dave Camp <dave@ximian.com>
 */

#include <config.h>
#include "ammonite-column-utilities.h"

#include <string.h>
#include <glib/gi18n.h>
#include <ammonite-extension.h>
#include "ammonite-file.h"
#include "ammonite-global-preferences.h"
#include "ammonite-metadata.h"
#include "ammonite-module.h"

static const char *default_column_order[] =
{
    "name",
    "size",
    "type",
    "owner",
    "group",
    "permissions",
    "detailed_type",
    "date_modified",
    "date_accessed",
    "date_created",
    "recency",
    "starred",
    NULL
};

static const char *default_columns_for_recent[] =
{
    "name", "size", "recency", NULL
};

static const char *default_columns_for_trash[] =
{
    "name", "size", "trashed_on", NULL
};

static GList *
get_builtin_columns (void)
{
    GList *columns;

    columns = g_list_append (NULL,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "name",
                                           "attribute", "name",
                                           "label", _("Name"),
                                           "description", _("The name and icon of the file."),
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "size",
                                           "attribute", "size",
                                           "label", _("Size"),
                                           "description", _("The size of the file."),
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "type",
                                           "attribute", "type",
                                           "label", _("Type"),
                                           "description", _("The type of the file."),
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "date_modified",
                                           "attribute", "date_modified",
                                           "label", _("Modified"),
                                           "description", _("The date the file was modified."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 1.0,
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "detailed_type",
                                           "attribute", "detailed_type",
                                           "label", _("Detailed Type"),
                                           "description", _("The detailed type of the file."),
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "date_accessed",
                                           "attribute", "date_accessed",
                                           "label", _("Accessed"),
                                           "description", _("The date the file was accessed."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 1.0,
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "date_created",
                                           "attribute", "date_created",
                                           "label", _("Created"),
                                           "description", _("The date the file was created."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 1.0,
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "owner",
                                           "attribute", "owner",
                                           "label", _("Owner"),
                                           "description", _("The owner of the file."),
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "group",
                                           "attribute", "group",
                                           "label", _("Group"),
                                           "description", _("The group of the file."),
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "permissions",
                                           "attribute", "permissions",
                                           "label", _("Permissions"),
                                           "description", _("The permissions of the file."),
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "recency",
                                           "attribute", "recency",
                                           "label", _("Recency"),
                                           "description", _("The date the file was accessed by the user."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 1.0,
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (AMMONITE_TYPE_COLUMN,
                                           "name", "starred",
                                           "attribute", "starred",
                                           "label", "",
                                           "description", _("Shows if file is starred."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 0.5,
                                           NULL));

    return columns;
}

static GList *
get_extension_columns (void)
{
    GList *columns;
    GList *providers;
    GList *l;

    providers = ammonite_module_get_extensions_for_type (AMMONITE_TYPE_COLUMN_PROVIDER);

    columns = NULL;

    for (l = providers; l != NULL; l = l->next)
    {
        AmmoniteColumnProvider *provider;
        GList *provider_columns;

        provider = AMMONITE_COLUMN_PROVIDER (l->data);
        provider_columns = ammonite_column_provider_get_columns (provider);
        columns = g_list_concat (columns, provider_columns);
    }

    ammonite_module_extension_list_free (providers);

    return columns;
}

static GList *
get_trash_columns (void)
{
    static GList *columns = NULL;

    if (columns == NULL)
    {
        columns = g_list_append (columns,
                                 g_object_new (AMMONITE_TYPE_COLUMN,
                                               "name", "trashed_on",
                                               "attribute", "trashed_on",
                                               "label", _("Trashed On"),
                                               "description", _("Date when file was moved to the Trash"),
                                               "xalign", 1.0,
                                               NULL));
        columns = g_list_append (columns,
                                 g_object_new (AMMONITE_TYPE_COLUMN,
                                               "name", "trash_orig_path",
                                               "attribute", "trash_orig_path",
                                               "label", _("Original Location"),
                                               "description", _("Original location of file before moved to the Trash"),
                                               NULL));
    }

    return ammonite_column_list_copy (columns);
}

static GList *
get_search_columns (void)
{
    static GList *columns = NULL;

    if (columns == NULL)
    {
        columns = g_list_append (columns,
                                 g_object_new (AMMONITE_TYPE_COLUMN,
                                               "name", "search_relevance",
                                               "attribute", "search_relevance",
                                               "label", _("Relevance"),
                                               "description", _("Relevance rank for search"),
                                               NULL));
    }

    return ammonite_column_list_copy (columns);
}

GList *
ammonite_get_common_columns (void)
{
    static GList *columns = NULL;

    if (!columns)
    {
        columns = g_list_concat (get_builtin_columns (),
                                 get_extension_columns ());
    }

    return ammonite_column_list_copy (columns);
}

GList *
ammonite_get_all_columns (void)
{
    GList *columns = NULL;

    columns = g_list_concat (ammonite_get_common_columns (),
                             get_trash_columns ());
    columns = g_list_concat (columns,
                             get_search_columns ());

    return columns;
}

GList *
ammonite_get_columns_for_file (AmmoniteFile *file)
{
    GList *columns;

    columns = ammonite_get_common_columns ();

    if (file != NULL && ammonite_file_is_in_trash (file))
    {
        columns = g_list_concat (columns,
                                 get_trash_columns ());
    }

    return columns;
}

GList *
ammonite_column_list_copy (GList *columns)
{
    GList *ret;
    GList *l;

    ret = g_list_copy (columns);

    for (l = ret; l != NULL; l = l->next)
    {
        g_object_ref (l->data);
    }

    return ret;
}

void
ammonite_column_list_free (GList *columns)
{
    GList *l;

    for (l = columns; l != NULL; l = l->next)
    {
        g_object_unref (l->data);
    }

    g_list_free (columns);
}

static int
strv_index (char       **strv,
            const char  *str)
{
    int i;

    for (i = 0; strv[i] != NULL; ++i)
    {
        if (strcmp (strv[i], str) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int
column_compare (AmmoniteColumn  *a,
                AmmoniteColumn  *b,
                char           **column_order)
{
    int index_a;
    int index_b;
    char *name_a;
    char *name_b;
    int ret;

    g_object_get (G_OBJECT (a), "name", &name_a, NULL);
    index_a = strv_index (column_order, name_a);

    g_object_get (G_OBJECT (b), "name", &name_b, NULL);
    index_b = strv_index (column_order, name_b);

    if (index_a == index_b)
    {
        int pos_a;
        int pos_b;

        pos_a = strv_index ((char **) default_column_order, name_a);
        pos_b = strv_index ((char **) default_column_order, name_b);

        if (pos_a == pos_b)
        {
            char *label_a;
            char *label_b;

            g_object_get (G_OBJECT (a), "label", &label_a, NULL);
            g_object_get (G_OBJECT (b), "label", &label_b, NULL);
            ret = strcmp (label_a, label_b);
            g_free (label_a);
            g_free (label_b);
        }
        else if (pos_a == -1)
        {
            ret = 1;
        }
        else if (pos_b == -1)
        {
            ret = -1;
        }
        else
        {
            ret = index_a - index_b;
        }
    }
    else if (index_a == -1)
    {
        ret = 1;
    }
    else if (index_b == -1)
    {
        ret = -1;
    }
    else
    {
        ret = index_a - index_b;
    }

    g_free (name_a);
    g_free (name_b);

    return ret;
}

GList *
ammonite_sort_columns (GList  *columns,
                       char  **column_order)
{
    if (column_order == NULL)
    {
        return columns;
    }

    return g_list_sort_with_data (columns,
                                  (GCompareDataFunc) column_compare,
                                  column_order);
}

void
ammonite_column_save_metadata (AmmoniteFile *file,
                               GStrv         column_order,
                               GStrv         visible_columns)
{
    ammonite_file_set_metadata (file,
                                AMMONITE_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
                                AMMONITE_METADATA_STRINGV, visible_columns);
    ammonite_file_set_metadata (file,
                                AMMONITE_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
                                AMMONITE_METADATA_STRINGV, column_order);
}

GStrv
ammonite_column_get_default_visible_columns (AmmoniteFile *file)
{
    if (ammonite_file_is_in_trash (file))
    {
        return g_strdupv ((gchar **) default_columns_for_trash);
    }

    if (ammonite_file_is_in_recent (file))
    {
        return g_strdupv ((gchar **) default_columns_for_recent);
    }

    return g_settings_get_strv (ammonite_list_view_preferences,
                                AMMONITE_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
}

GStrv
ammonite_column_get_visible_columns (AmmoniteFile *file)
{
    const GStrv visible_columns =
        ammonite_file_get_metadata_list (file, AMMONITE_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS);

    if (visible_columns == NULL || visible_columns[0] == NULL)
    {
        return ammonite_column_get_default_visible_columns (file);
    }

    return g_strdupv (visible_columns);
}

GStrv
ammonite_column_get_default_column_order (AmmoniteFile *file)
{
    if (ammonite_file_is_in_trash (file))
    {
        return g_strdupv ((gchar **) default_columns_for_trash);
    }

    if (ammonite_file_is_in_recent (file))
    {
        return g_strdupv ((gchar **) default_columns_for_recent);
    }

    return g_settings_get_strv (ammonite_list_view_preferences,
                                AMMONITE_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
}

GStrv
ammonite_column_get_column_order (AmmoniteFile *file)
{
    const GStrv column_order =
        ammonite_file_get_metadata_list (file, AMMONITE_METADATA_KEY_LIST_VIEW_COLUMN_ORDER);

    if (column_order != NULL && column_order[0] != NULL)
    {
        return g_strdupv (column_order);
    }

    return ammonite_column_get_default_column_order (file);
}
