/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-view-item.h"

#include "ammonite-file.h"

struct _AmmoniteViewItem
{
    GObject parent_instance;
    gboolean is_cut;
    gboolean drag_accept;
    gboolean loading;
    AmmoniteFile *file;
    GtkWidget *item_ui;
};

G_DEFINE_FINAL_TYPE (AmmoniteViewItem, ammonite_view_item, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_FILE,
    PROP_IS_CUT,
    PROP_DRAG_ACCEPT,
    PROP_LOADING,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

enum
{
    FILE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
ammonite_view_item_dispose (GObject *object)
{
    AmmoniteViewItem *self = AMMONITE_VIEW_ITEM (object);

    g_clear_weak_pointer (&self->item_ui);

    G_OBJECT_CLASS (ammonite_view_item_parent_class)->dispose (object);
}

static void
ammonite_view_item_finalize (GObject *object)
{
    AmmoniteViewItem *self = AMMONITE_VIEW_ITEM (object);

    g_clear_object (&self->file);

    G_OBJECT_CLASS (ammonite_view_item_parent_class)->finalize (object);
}

static void
ammonite_view_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    AmmoniteViewItem *self = AMMONITE_VIEW_ITEM (object);

    switch (prop_id)
    {
        case PROP_FILE:
        {
            g_value_set_object (value, self->file);
        }
        break;

        case PROP_IS_CUT:
        {
            g_value_set_boolean (value, self->is_cut);
        }
        break;

        case PROP_DRAG_ACCEPT:
        {
            g_value_set_boolean (value, self->drag_accept);
        }
        break;

        case PROP_LOADING:
        {
            g_value_set_boolean (value, self->loading);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_view_item_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    AmmoniteViewItem *self = AMMONITE_VIEW_ITEM (object);

    switch (prop_id)
    {
        case PROP_FILE:
        {
            self->file = g_value_dup_object (value);
        }
        break;

        case PROP_IS_CUT:
        {
            ammonite_view_item_set_cut (self, g_value_get_boolean (value));
        }
        break;

        case PROP_DRAG_ACCEPT:
        {
            ammonite_view_item_set_drag_accept (self, g_value_get_boolean (value));
        }
        break;

        case PROP_LOADING:
        {
            ammonite_view_item_set_loading (self, g_value_get_boolean (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_view_item_init (AmmoniteViewItem *self)
{
}

static void
ammonite_view_item_class_init (AmmoniteViewItemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = ammonite_view_item_dispose;
    object_class->finalize = ammonite_view_item_finalize;
    object_class->get_property = ammonite_view_item_get_property;
    object_class->set_property = ammonite_view_item_set_property;

    properties[PROP_IS_CUT] = g_param_spec_boolean ("is-cut",
                                                    "", "",
                                                    FALSE,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_DRAG_ACCEPT] = g_param_spec_boolean ("drag-accept",
                                                         "", "",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_FILE] = g_param_spec_object ("file",
                                                 "", "",
                                                 AMMONITE_TYPE_FILE,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_LOADING] = g_param_spec_boolean ("loading", NULL, NULL,
                                                     FALSE,
                                                     G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);

    signals[FILE_CHANGED] = g_signal_new ("file-changed",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE, 0);
}

AmmoniteViewItem *
ammonite_view_item_new (AmmoniteFile *file)
{
    return g_object_new (AMMONITE_TYPE_VIEW_ITEM,
                         "file", file,
                         NULL);
}

void
ammonite_view_item_set_cut (AmmoniteViewItem *self,
                            gboolean          is_cut)
{
    g_return_if_fail (AMMONITE_IS_VIEW_ITEM (self));

    if (self->is_cut != is_cut)
    {
        self->is_cut = is_cut;

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IS_CUT]);
    }
}

void
ammonite_view_item_set_drag_accept (AmmoniteViewItem *self,
                                    gboolean          drag_accept)
{
    g_return_if_fail (AMMONITE_IS_VIEW_ITEM (self));

    if (self->drag_accept != drag_accept)
    {
        self->drag_accept = drag_accept;

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DRAG_ACCEPT]);
    }
}

gboolean
ammonite_view_item_get_loading (AmmoniteViewItem *self)
{
    g_return_val_if_fail (AMMONITE_IS_VIEW_ITEM (self), FALSE);

    return self->loading;
}

void
ammonite_view_item_set_loading (AmmoniteViewItem *self,
                                gboolean          loading)
{
    g_return_if_fail (AMMONITE_IS_VIEW_ITEM (self));

    if (self->loading != loading)
    {
        self->loading = loading;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
    }
}

AmmoniteFile *
ammonite_view_item_get_file (AmmoniteViewItem *self)
{
    g_return_val_if_fail (AMMONITE_IS_VIEW_ITEM (self), NULL);

    return self->file;
}

GtkWidget *
ammonite_view_item_get_item_ui (AmmoniteViewItem *self)
{
    g_return_val_if_fail (AMMONITE_IS_VIEW_ITEM (self), NULL);

    return self->item_ui;
}

void
ammonite_view_item_set_item_ui (AmmoniteViewItem *self,
                                GtkWidget        *item_ui)
{
    g_return_if_fail (AMMONITE_IS_VIEW_ITEM (self));

    g_set_weak_pointer (&self->item_ui, item_ui);
}

void
ammonite_view_item_file_changed (AmmoniteViewItem *self)
{
    g_return_if_fail (AMMONITE_IS_VIEW_ITEM (self));

    g_signal_emit (self, signals[FILE_CHANGED], 0);
}
