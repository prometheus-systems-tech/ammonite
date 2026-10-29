/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include "ammonite-properties-item.h"

/**
 * AmmonitePropertiesItem:
 *
 * A single element in a file's properties.
 *
 * A file's properties will consist of one or more `AmmonitePropertiesItem`.
 * Each item is a name/value pair.  Items are added to their corresponding
 * [property@AmmonitePropertiesModel:model].
 */

enum
{
    PROP_0,
    PROP_NAME,
    PROP_VALUE,
    LAST_PROP
};

struct _AmmonitePropertiesItem
{
    GObject parent_instance;

    char *name;
    char *value;
};

G_DEFINE_TYPE (AmmonitePropertiesItem, ammonite_properties_item, G_TYPE_OBJECT)

/**
 * ammonite_properties_item_new:
 * @name: the user-visible name for the properties item.
 * @value: the user-visible value for the properties item.
 *
 * Create a new `AmmonitePropertiesItem`
 *
 * returns: (transfer full): a new `AmmonitePropertiesItem`
 */
AmmonitePropertiesItem *
ammonite_properties_item_new (const char *name,
                              const char *value)
{
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_object_new (AMMONITE_TYPE_PROPERTIES_ITEM,
                         "name", name,
                         "value", value,
                         NULL);
}

static void
ammonite_properties_item_get_property (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    AmmonitePropertiesItem *self = AMMONITE_PROPERTIES_ITEM (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, self->name);
        }
        break;

        case PROP_VALUE:
        {
            g_value_set_string (value, self->value);
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
ammonite_properties_item_set_property (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    AmmonitePropertiesItem *self = AMMONITE_PROPERTIES_ITEM (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_free (self->name);
            self->name = g_value_dup_string (value);
        }
        break;

        case PROP_VALUE:
        {
            g_free (self->value);
            self->value = g_value_dup_string (value);
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
ammonite_properties_item_finalize (GObject *object)
{
    AmmonitePropertiesItem *self = AMMONITE_PROPERTIES_ITEM (object);

    g_free (self->name);
    g_free (self->value);

    G_OBJECT_CLASS (ammonite_properties_item_parent_class)->finalize (object);
}

static void
ammonite_properties_item_init (AmmonitePropertiesItem *self)
{
}

static void
ammonite_properties_item_class_init (AmmonitePropertiesItemClass *class)
{
    GParamSpec *pspec;

    G_OBJECT_CLASS (class)->finalize = ammonite_properties_item_finalize;
    G_OBJECT_CLASS (class)->get_property = ammonite_properties_item_get_property;
    G_OBJECT_CLASS (class)->set_property = ammonite_properties_item_set_property;

    /**
     * AmmonitePropertiesItem:name:
     *
     * The user-visible name.
     */
    pspec = g_param_spec_string ("name", "", "",
                                 NULL,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (G_OBJECT_CLASS (class), PROP_NAME, pspec);

    /**
     * AmmonitePropertiesItem:value:
     *
     * The user-visible value.
     */
    pspec = g_param_spec_string ("value", "", "",
                                 NULL,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (G_OBJECT_CLASS (class), PROP_VALUE, pspec);
}

/**
 * ammonite_properties_item_get_name:
 *
 * Get the name.
 *
 * Returns: (transfer none): the name of this `AmmonitePropertiesItem`
 */
const char *
ammonite_properties_item_get_name (AmmonitePropertiesItem *self)
{
    return self->name;
}

/**
 * ammonite_properties_item_get_value:
 *
 * Get the value.
 *
 * Returns: (transfer none): the value of this `AmmonitePropertiesItem`
 */
const char *
ammonite_properties_item_get_value (AmmonitePropertiesItem *self)
{
    return self->value;
}
