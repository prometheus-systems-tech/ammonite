/*
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#include "ammonite-column.h"

/**
 * AmmoniteColumn:
 *
 * List view column descriptor object.
 *
 * `AmmoniteColumn` is an object that describes a column in the file manager
 * list view. Extensions can provide `AmmoniteColumn` by registering a
 * [iface@ColumnProvider] and returning them from
 * [method@ColumnProvider.get_columns], which will be called by the main
 * application when creating a view.
 */

enum
{
    PROP_0,
    PROP_NAME,
    PROP_ATTRIBUTE,
    PROP_ATTRIBUTE_Q,
    PROP_LABEL,
    PROP_DESCRIPTION,
    PROP_XALIGN,
    PROP_DEFAULT_SORT_ORDER,
    PROP_VISIBLE,
    LAST_PROP
};

struct _AmmoniteColumn
{
    GObject parent_instance;

    char *name;
    GQuark attribute;
    char *label;
    char *description;
    float xalign;
    int default_sort_order; /* Actually, meant to store GtkSortType */
    gboolean visible;
};

G_DEFINE_TYPE (AmmoniteColumn, ammonite_column, G_TYPE_OBJECT);

/**
 * ammonite_column_new:
 * @name: (not nullable): identifier of the column
 * @attribute: (not nullable): the file attribute to be displayed in the column
 * @label: (not nullable): the user-visible label for the column
 * @description: (not nullable): a user-visible description of the column
 *
 * Creates a new [class@Column] object.
 *
 * Returns: (transfer full): a new column
 */
AmmoniteColumn *
ammonite_column_new (const char *name,
                     const char *attribute,
                     const char *label,
                     const char *description)
{
    AmmoniteColumn *column;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (attribute != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (description != NULL, NULL);

    column = g_object_new (AMMONITE_TYPE_COLUMN,
                           "name", name,
                           "attribute", attribute,
                           "label", label,
                           "description", description,
                           NULL);

    return column;
}

static void
ammonite_column_get_property (GObject    *object,
                              guint       param_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    AmmoniteColumn *column;

    column = AMMONITE_COLUMN (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, column->name);
        }
        break;

        case PROP_ATTRIBUTE:
        {
            g_value_set_string (value, g_quark_to_string (column->attribute));
        }
        break;

        case PROP_ATTRIBUTE_Q:
        {
            g_value_set_uint (value, column->attribute);
        }
        break;

        case PROP_LABEL:
        {
            g_value_set_string (value, column->label);
        }
        break;

        case PROP_DESCRIPTION:
        {
            g_value_set_string (value, column->description);
        }
        break;

        case PROP_XALIGN:
        {
            g_value_set_float (value, column->xalign);
        }
        break;

        case PROP_DEFAULT_SORT_ORDER:
        {
            g_value_set_int (value, column->default_sort_order);
        }
        break;

        case PROP_VISIBLE:
        {
            g_value_set_boolean (value, column->visible);
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
ammonite_column_set_property (GObject      *object,
                              guint         param_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    AmmoniteColumn *column;

    column = AMMONITE_COLUMN (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_set_str (&column->name, g_value_get_string (value));
            g_object_notify (object, "name");
        }
        break;

        case PROP_ATTRIBUTE:
        {
            column->attribute = g_quark_from_string (g_value_get_string (value));
            g_object_notify (object, "attribute");
            g_object_notify (object, "attribute_q");
        }
        break;

        case PROP_LABEL:
        {
            g_set_str (&column->label, g_value_get_string (value));
            g_object_notify (object, "label");
        }
        break;

        case PROP_DESCRIPTION:
        {
            g_set_str (&column->description, g_value_get_string (value));
            g_object_notify (object, "description");
        }
        break;

        case PROP_XALIGN:
        {
            column->xalign = g_value_get_float (value);
            g_object_notify (object, "xalign");
        }
        break;

        case PROP_DEFAULT_SORT_ORDER:
        {
            column->default_sort_order = g_value_get_int (value);
            g_object_notify (object, "default-sort-order");
        }
        break;

        case PROP_VISIBLE:
        {
            column->visible = g_value_get_boolean (value);
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
ammonite_column_finalize (GObject *object)
{
    AmmoniteColumn *column;

    column = AMMONITE_COLUMN (object);

    g_free (column->name);
    g_free (column->label);
    g_free (column->description);

    G_OBJECT_CLASS (ammonite_column_parent_class)->finalize (object);
}

static void
ammonite_column_init (AmmoniteColumn *column)
{
    column->xalign = 0.0;
}

static void
ammonite_column_class_init (AmmoniteColumnClass *class)
{
    G_OBJECT_CLASS (class)->finalize = ammonite_column_finalize;
    G_OBJECT_CLASS (class)->get_property = ammonite_column_get_property;
    G_OBJECT_CLASS (class)->set_property = ammonite_column_set_property;

    /**
     * AmmoniteColumn:name:
     *
     * The identifier for the column.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          "Name",
                                                          "Name of the column",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));

    /**
     * AmmoniteColumn:attribute:
     *
     * The file attribute to be displayed in the column.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_ATTRIBUTE,
                                     g_param_spec_string ("attribute",
                                                          "Attribute",
                                                          "The attribute name to display",
                                                          NULL,
                                                          G_PARAM_READWRITE));

    /**
     * AmmoniteColumn:attribute_q:
     *
     * The name of the attribute to display, in quark form.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_ATTRIBUTE_Q,
                                     g_param_spec_uint ("attribute_q",
                                                        "Attribute quark",
                                                        "The attribute name to display, in quark form",
                                                        0, G_MAXUINT, 0,
                                                        G_PARAM_READABLE));

    /**
     * AmmoniteColumn:label:
     *
     * The label to display in the column.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_LABEL,
                                     g_param_spec_string ("label",
                                                          "Label",
                                                          "Label to display in the column",
                                                          NULL,
                                                          G_PARAM_READWRITE));

    /**
     * AmmoniteColumn:description:
     *
     * The user-visible description of the column.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_DESCRIPTION,
                                     g_param_spec_string ("description",
                                                          "Description",
                                                          "A user-visible description of the column",
                                                          NULL,
                                                          G_PARAM_READWRITE));

    /**
     * AmmoniteColumn:xalign:
     *
     * The x-alignment of the column.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_XALIGN,
                                     g_param_spec_float ("xalign",
                                                         "xalign",
                                                         "The x-alignment of the column",
                                                         0.0,
                                                         1.0,
                                                         0.0,
                                                         G_PARAM_READWRITE));
    /**
     * AmmoniteColumn:default-sort-order: (type gboolean)
     *
     * The enum values of GtkSortType
     *
     * Uses enum because we don't want extensions to depend on Gtk. This property
     * is not meant to be used by extensions.
     *
     * Stability: Private: Internal to the application.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_DEFAULT_SORT_ORDER,
                                     g_param_spec_int ("default-sort-order",
                                                       "Default sort order",
                                                       "Default sort order",
                                                       G_MININT, G_MAXINT, 0,
                                                       G_PARAM_READWRITE));

    /**
     * AmmoniteColumn:visible: (type gboolean)
     *
     * Whether to show the AmmoniteColumn in a ColumnChooser.
     *
     * This is not meant to be used by extensions. The value may be changed
     * over the life of the AmmoniteColumn.
     *
     * Stability: Private: Internal to the application.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_VISIBLE,
                                     g_param_spec_boolean ("visible",
                                                           NULL, NULL,
                                                           FALSE,
                                                           G_PARAM_READWRITE));
}
