/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include "ammonite-properties-model.h"
#include "ammonite-properties-item.h"

/**
 * AmmonitePropertiesModel:
 *
 * A model to implement custom file Properties.
 *
 * `AmmonitePropertiesModel` is an model that describes a set of file properties.
 * Extensions can provide `AmmonitePropertiesModel` objects by registering a
 * [iface@PropertiesModelProvider] and returning them from
 * [method@PropertiesModelProvider.get_models], which will be called by
 * the main application when creating file properties.
 */

enum
{
    PROP_0,
    PROP_TITLE,
    PROP_MODEL,
    LAST_PROP
};

struct _AmmonitePropertiesModel
{
    GObject parent_instance;

    char *title;
    GListModel *model;
};

G_DEFINE_TYPE (AmmonitePropertiesModel, ammonite_properties_model, G_TYPE_OBJECT)

/**
 * ammonite_properties_model_new:
 * @title: the user-visible name for the set of properties in this model
 * @model: a [iface@Gio.ListModel] containing #AmmonitePropertyItem objects.
 *
 * Create a new `AmmonitePropertiesModel`.
 *
 * Returns: (transfer full): a new #AmmonitePropertiesModel
 */
AmmonitePropertiesModel *
ammonite_properties_model_new (const char *title,
                               GListModel *model)
{
    g_return_val_if_fail (title != NULL, NULL);
    g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);
    g_return_val_if_fail (g_list_model_get_item_type (model) == AMMONITE_TYPE_PROPERTIES_ITEM, NULL);

    return g_object_new (AMMONITE_TYPE_PROPERTIES_MODEL,
                         "title", title,
                         "model", model,
                         NULL);
}

static void
ammonite_properties_model_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
    AmmonitePropertiesModel *self = AMMONITE_PROPERTIES_MODEL (object);

    switch (param_id)
    {
        case PROP_TITLE:
        {
            g_value_set_string (value, self->title);
        }
        break;

        case PROP_MODEL:
        {
            g_value_set_object (value, self->model);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
ammonite_properties_model_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    AmmonitePropertiesModel *self = AMMONITE_PROPERTIES_MODEL (object);

    switch (param_id)
    {
        case PROP_TITLE:
        {
            g_free (self->title);
            self->title = g_value_dup_string (value);
        }
        break;

        case PROP_MODEL:
        {
            g_set_object (&self->model, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
ammonite_properties_model_dispose (GObject *object)
{
    AmmonitePropertiesModel *self = AMMONITE_PROPERTIES_MODEL (object);

    g_clear_object (&self->model);

    G_OBJECT_CLASS (ammonite_properties_model_parent_class)->dispose (object);
}

static void
ammonite_properties_model_finalize (GObject *object)
{
    AmmonitePropertiesModel *self = AMMONITE_PROPERTIES_MODEL (object);

    g_free (self->title);

    G_OBJECT_CLASS (ammonite_properties_model_parent_class)->finalize (object);
}

static void
ammonite_properties_model_init (AmmonitePropertiesModel *self)
{
}

static void
ammonite_properties_model_class_init (AmmonitePropertiesModelClass *class)
{
    GParamSpec *pspec;

    G_OBJECT_CLASS (class)->finalize = ammonite_properties_model_finalize;
    G_OBJECT_CLASS (class)->dispose = ammonite_properties_model_dispose;
    G_OBJECT_CLASS (class)->get_property = ammonite_properties_model_get_property;
    G_OBJECT_CLASS (class)->set_property = ammonite_properties_model_set_property;

    /**
     * AmmonitePropertiesModel:title:
     *
     * The user visible title
     */
    pspec = g_param_spec_string ("title", "", "",
                                 NULL,
                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (G_OBJECT_CLASS (class), PROP_TITLE, pspec);

    /**
     * AmmonitePropertiesModel:model
     *
     * The item model.
     */
    pspec = g_param_spec_object ("model", "", "",
                                 G_TYPE_LIST_MODEL,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (G_OBJECT_CLASS (class), PROP_MODEL, pspec);
}

/**
 * ammonite_properties_model_get_title:
 *
 * Get the user-visible title.
 *
 * Returns: (transfer none): the title of this #AmmonitePropertiesModel
 */
const char *
ammonite_properties_model_get_title (AmmonitePropertiesModel *self)
{
    return self->title;
}

/**
 * ammonite_properties_model_set_title:
 * @title: the new title
 *
 * Set a user-visible name for the set of properties in this model.
 *
 * It should work both as a window title and as a boxed list row.
 * Exactly where it is shown in the UI may vary in the future.
 *
 */
void
ammonite_properties_model_set_title (AmmonitePropertiesModel *self,
                                     const char              *title)
{
    g_object_set (self, "title", title, NULL);
}

/**
 * ammonite_properties_model_get_model:
 *
 * Gets the properties items provided by this model.
 *
 * Returns: (transfer none): a [iface@Gio.ListModel] containing [class@PropertiesItem].
 */
GListModel *
ammonite_properties_model_get_model (AmmonitePropertiesModel *self)
{
    return self->model;
}
