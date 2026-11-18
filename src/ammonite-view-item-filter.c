/*
 * SPDX-FileCopyrightText: 2024 GNOME Foundation Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "ammonite-view-item-filter.h"

#include "ammonite-file.h"
#include "ammonite-view-item.h"

#include <gio/gio.h>

enum
{
    PROP_0,
    PROP_FILE_FILTER,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

struct _AmmoniteViewItemFilter
{
    GtkFilter parent_instance;

    GtkFilterMatch strictness;

    GtkFileFilter *file_filter;
    GFileInfo *mannequin;
};

G_DEFINE_FINAL_TYPE (AmmoniteViewItemFilter, ammonite_view_item_filter, GTK_TYPE_FILTER)

static GtkFilterMatch
ammonite_view_item_filter_get_strictness (GtkFilter *filter)
{
    AmmoniteViewItemFilter *self = AMMONITE_VIEW_ITEM_FILTER (filter);

    return self->strictness;
}

static gboolean
ammonite_view_item_filter_match (GtkFilter *filter,
                                 gpointer   item)
{
    g_return_val_if_fail (AMMONITE_IS_VIEW_ITEM (item), FALSE);

    AmmoniteViewItemFilter *self = AMMONITE_VIEW_ITEM_FILTER (filter);
    AmmoniteFile *file = ammonite_view_item_get_file (AMMONITE_VIEW_ITEM (item));

    if (ammonite_file_opens_in_view (file))
    {
        /* The current use case (FileChooser) never hides folders. */
        return TRUE;
    }

    GFileInfo *info = self->mannequin;

    g_file_info_set_display_name (info, ammonite_file_get_display_name (file));
    g_file_info_set_content_type (info, ammonite_file_get_mime_type (file));

    return gtk_filter_match (GTK_FILTER (self->file_filter), info);
}

static void
ammonite_view_item_filter_finalize (GObject *object)
{
    AmmoniteViewItemFilter *self = AMMONITE_VIEW_ITEM_FILTER (object);

    g_clear_object (&self->file_filter);
    g_clear_object (&self->mannequin);

    G_OBJECT_CLASS (ammonite_view_item_filter_parent_class)->finalize (object);
}

static void
ammonite_view_item_filter_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    AmmoniteViewItemFilter *self = AMMONITE_VIEW_ITEM_FILTER (object);

    switch (prop_id)
    {
        case PROP_FILE_FILTER:
        {
            ammonite_view_item_filter_set_file_filter (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_view_item_filter_class_init (AmmoniteViewItemFilterClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkFilterClass *filter_class = GTK_FILTER_CLASS (klass);

    object_class->finalize = ammonite_view_item_filter_finalize;
    object_class->set_property = ammonite_view_item_filter_set_property;

    filter_class->get_strictness = ammonite_view_item_filter_get_strictness;
    filter_class->match = ammonite_view_item_filter_match;

    properties[PROP_FILE_FILTER] = g_param_spec_object ("file-filter", NULL, NULL,
                                                        GTK_TYPE_FILE_FILTER,
                                                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ammonite_view_item_filter_init (AmmoniteViewItemFilter *self)
{
    self->strictness = GTK_FILTER_MATCH_ALL;
    self->mannequin = g_file_info_new ();
}

AmmoniteViewItemFilter *
ammonite_view_item_filter_new (void)
{
    return g_object_new (AMMONITE_TYPE_VIEW_ITEM_FILTER, NULL);
}

void
ammonite_view_item_filter_set_file_filter (AmmoniteViewItemFilter *self,
                                           GtkFileFilter          *file_filter)
{
    g_return_if_fail (AMMONITE_IS_VIEW_ITEM_FILTER (self));

    if (!g_set_object (&self->file_filter, file_filter))
    {
        return;
    }

    GtkFilterMatch old_strictness = self->strictness;
    GtkFilterChange change = GTK_FILTER_CHANGE_DIFFERENT;

    self->strictness = (file_filter != NULL ?
                        GTK_FILTER_MATCH_SOME :
                        GTK_FILTER_MATCH_ALL);

    if (self->strictness != old_strictness)
    {
        change = (self->strictness == GTK_FILTER_MATCH_ALL ?
                  GTK_FILTER_CHANGE_LESS_STRICT :
                  GTK_FILTER_CHANGE_MORE_STRICT);
    }

    gtk_filter_changed (GTK_FILTER (self), change);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE_FILTER]);
}
