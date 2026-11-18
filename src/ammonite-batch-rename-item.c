/*
 * SPDX-FileCopyrightText: 2024–2025 Markus Göllnitz <camelcasenick@bewares.it>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ammonite-batch-rename-item.h"

#include <config.h>

#include "ammonite-batch-rename-dialog.h"

#include <glib.h>

struct _AmmoniteBatchRenameItem
{
    GObject parent_instance;

    gchar *name_before;
    gchar *name_after;
    gboolean has_conflict;

    AmmoniteBatchRenameDialog *dialog;
};

enum
{
    PROP_0,

    PROP_NAME_BEFORE,
    PROP_NAME_AFTER,
    PROP_HAS_CONFLICT,
    PROP_DIALOG,

    NUM_PROPERTIES
};

G_DEFINE_FINAL_TYPE (AmmoniteBatchRenameItem, ammonite_batch_rename_item, G_TYPE_OBJECT)

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

static void
ammonite_batch_rename_item_finalize (GObject *object)
{
    AmmoniteBatchRenameItem *item = AMMONITE_BATCH_RENAME_ITEM (object);

    g_free (item->name_before);
    g_free (item->name_after);
    g_clear_weak_pointer (&item->dialog);

    G_OBJECT_CLASS (ammonite_batch_rename_item_parent_class)->finalize (object);
}

static void
ammonite_batch_rename_item_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
    AmmoniteBatchRenameItem *item = AMMONITE_BATCH_RENAME_ITEM (object);

    switch (prop_id)
    {
        case PROP_NAME_BEFORE:
        {
            g_value_set_string (value, item->name_before);
            break;
        }

        case PROP_NAME_AFTER:
        {
            g_value_set_string (value, item->name_after);
            break;
        }

        case PROP_HAS_CONFLICT:
        {
            g_value_set_boolean (value, item->has_conflict);
            break;
        }

        case PROP_DIALOG:
        {
            g_value_set_object (value, item->dialog);
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_batch_rename_item_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
    AmmoniteBatchRenameItem *item = AMMONITE_BATCH_RENAME_ITEM (object);

    switch (prop_id)
    {
        case PROP_NAME_BEFORE:
        {
            ammonite_batch_rename_item_set_name_before (item, g_value_get_string (value));
            break;
        }

        case PROP_NAME_AFTER:
        {
            ammonite_batch_rename_item_set_name_after (item, g_value_get_string (value));
            break;
        }

        case PROP_HAS_CONFLICT:
        {
            ammonite_batch_rename_item_set_has_conflict (item, g_value_get_boolean (value));
            break;
        }

        case PROP_DIALOG:
        {
            if (g_set_weak_pointer (&item->dialog, AMMONITE_BATCH_RENAME_DIALOG (g_value_get_object (value))))
            {
                g_object_notify_by_pspec (G_OBJECT (item), props[PROP_DIALOG]);
            }
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_batch_rename_item_class_init (AmmoniteBatchRenameItemClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ammonite_batch_rename_item_finalize;

    oclass->get_property = ammonite_batch_rename_item_get_property;
    oclass->set_property = ammonite_batch_rename_item_set_property;

    props[PROP_NAME_BEFORE] =
        g_param_spec_string ("name-before",
                             NULL,
                             "",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    props[PROP_NAME_AFTER] =
        g_param_spec_string ("name-after",
                             NULL,
                             "",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    props[PROP_HAS_CONFLICT] =
        g_param_spec_boolean ("has-conflict",
                              NULL,
                              "",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    props[PROP_DIALOG] =
        g_param_spec_object ("dialog",
                             NULL,
                             "",
                             AMMONITE_TYPE_BATCH_RENAME_DIALOG,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, props);
}

static void
ammonite_batch_rename_item_init (AmmoniteBatchRenameItem *item)
{
}

AmmoniteBatchRenameItem *
ammonite_batch_rename_item_new (const gchar               *name_before,
                                const gchar               *name_after,
                                AmmoniteBatchRenameDialog *dialog)
{
    return g_object_new (AMMONITE_TYPE_BATCH_RENAME_ITEM,
                         "name-before", name_before,
                         "name-after", name_after,
                         "dialog", dialog,
                         NULL);
}

void
ammonite_batch_rename_item_set_name_before (AmmoniteBatchRenameItem *item,
                                            const gchar             *name_before)
{
    if (g_set_str (&item->name_before, name_before))
    {
        g_object_notify_by_pspec (G_OBJECT (item), props[PROP_NAME_BEFORE]);
    }
}

void
ammonite_batch_rename_item_set_name_after (AmmoniteBatchRenameItem *item,
                                           const gchar             *name_after)
{
    if (g_set_str (&item->name_after, name_after))
    {
        g_object_notify_by_pspec (G_OBJECT (item), props[PROP_NAME_AFTER]);
    }
}

void
ammonite_batch_rename_item_set_has_conflict (AmmoniteBatchRenameItem *item,
                                             gboolean                 has_conflict)
{
    if (item->has_conflict != has_conflict)
    {
        item->has_conflict = has_conflict;

        g_object_notify_by_pspec (G_OBJECT (item), props[PROP_HAS_CONFLICT]);
    }
}
