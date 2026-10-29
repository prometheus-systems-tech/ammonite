/*
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#include <config.h>
#include "ammonite-menu.h"

typedef struct
{
    char *name;
    char *label;
    char *tip;
    char *icon;
    AmmoniteMenu *menu;
    gboolean sensitive;
    gboolean priority;
} AmmoniteMenuItemPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AmmoniteMenuItem, ammonite_menu_item, G_TYPE_OBJECT)

enum
{
    ACTIVATE,
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_TIP,
    PROP_ICON,
    PROP_SENSITIVE,
    PROP_PRIORITY,
    PROP_MENU,
    LAST_PROP
};

static guint signals[LAST_SIGNAL];

/**
 * ammonite_menu_item_new:
 * @name: the identifier for the menu item
 * @label: the user-visible label of the menu item
 * @tip: (nullable): this has no effect
 * @icon: (nullable): this has no effect
 *
 * Creates a new menu item that can be added to the toolbar or to a contextual menu.
 *
 * Returns: (transfer full): a new #AmmoniteMenuItem
 */
AmmoniteMenuItem *
ammonite_menu_item_new (const char *name,
                        const char *label,
                        const char *tip,
                        const char *icon)
{
    AmmoniteMenuItem *item;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);

    item = g_object_new (AMMONITE_TYPE_MENU_ITEM,
                         "name", name,
                         "label", label,
                         "tip", tip,
                         "icon", icon,
                         NULL);

    return item;
}

/**
 * ammonite_menu_item_activate:
 * @item: pointer to a #AmmoniteMenuItem
 *
 * Emits #AmmoniteMenuItem::activate.
 */
void
ammonite_menu_item_activate (AmmoniteMenuItem *self)
{
    g_return_if_fail (AMMONITE_IS_MENU_ITEM (self));

    g_signal_emit (self, signals[ACTIVATE], 0);
}

/**
 * ammonite_menu_item_set_submenu:
 * @item: pointer to a #AmmoniteMenuItem
 * @menu: (transfer full): pointer to a #AmmoniteMenu to attach to the button
 *
 * Attaches a menu to the given #AmmoniteMenuItem.
 */
void
ammonite_menu_item_set_submenu (AmmoniteMenuItem *self,
                                AmmoniteMenu     *menu)
{
    g_return_if_fail (AMMONITE_IS_MENU_ITEM (self));

    g_object_set (self, "menu", menu, NULL);
}

static void
ammonite_menu_item_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    AmmoniteMenuItem *item;
    AmmoniteMenuItemPrivate *priv;

    item = AMMONITE_MENU_ITEM (object);
    priv = ammonite_menu_item_get_instance_private (item);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, priv->name);
        }
        break;

        case PROP_LABEL:
        {
            g_value_set_string (value, priv->label);
        }
        break;

        case PROP_TIP:
        {
            g_value_set_string (value, priv->tip);
        }
        break;

        case PROP_ICON:
        {
            g_value_set_string (value, priv->icon);
        }
        break;

        case PROP_SENSITIVE:
        {
            g_value_set_boolean (value, priv->sensitive);
        }
        break;

        case PROP_PRIORITY:
        {
            g_value_set_boolean (value, priv->priority);
        }
        break;

        case PROP_MENU:
        {
            g_value_set_object (value, priv->menu);
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
ammonite_menu_item_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    AmmoniteMenuItem *item;
    AmmoniteMenuItemPrivate *priv;

    item = AMMONITE_MENU_ITEM (object);
    priv = ammonite_menu_item_get_instance_private (item);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_free (priv->name);
            priv->name = g_strdup (g_value_get_string (value));
            g_object_notify (object, "name");
        }
        break;

        case PROP_LABEL:
        {
            g_free (priv->label);
            priv->label = g_strdup (g_value_get_string (value));
            g_object_notify (object, "label");
        }
        break;

        case PROP_TIP:
        {
            g_free (priv->tip);
            priv->tip = g_strdup (g_value_get_string (value));
            g_object_notify (object, "tip");
        }
        break;

        case PROP_ICON:
        {
            g_free (priv->icon);
            priv->icon = g_strdup (g_value_get_string (value));
            g_object_notify (object, "icon");
        }
        break;

        case PROP_SENSITIVE:
        {
            priv->sensitive = g_value_get_boolean (value);
            g_object_notify (object, "sensitive");
        }
        break;

        case PROP_PRIORITY:
        {
            priv->priority = g_value_get_boolean (value);
            g_object_notify (object, "priority");
        }
        break;

        case PROP_MENU:
        {
            if (priv->menu)
            {
                g_object_unref (priv->menu);
            }
            priv->menu = g_object_ref (g_value_get_object (value));
            g_object_notify (object, "menu");
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
ammonite_menu_item_finalize (GObject *object)
{
    AmmoniteMenuItem *item;
    AmmoniteMenuItemPrivate *priv;

    item = AMMONITE_MENU_ITEM (object);
    priv = ammonite_menu_item_get_instance_private (item);

    g_free (priv->name);
    g_free (priv->label);
    g_free (priv->tip);
    g_free (priv->icon);
    if (priv->menu)
    {
        g_object_unref (priv->menu);
    }

    G_OBJECT_CLASS (ammonite_menu_item_parent_class)->finalize (object);
}

static void
ammonite_menu_item_init (AmmoniteMenuItem *self)
{
    AmmoniteMenuItemPrivate *priv;

    priv = ammonite_menu_item_get_instance_private (self);

    priv->sensitive = TRUE;
    priv->menu = NULL;
}

static void
ammonite_menu_item_class_init (AmmoniteMenuItemClass *class)
{
    G_OBJECT_CLASS (class)->finalize = ammonite_menu_item_finalize;
    G_OBJECT_CLASS (class)->get_property = ammonite_menu_item_get_property;
    G_OBJECT_CLASS (class)->set_property = ammonite_menu_item_set_property;

    /**
     * AmmoniteMenuItem::activate:
     * @item: the object which received the signal
     *
     * Signals that the user has activated this menu item.
     */
    signals[ACTIVATE] =
        g_signal_new ("activate",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AmmoniteMenuItemClass,
                                       activate),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    /**
     * AmmoniteMenuItem:name:
     *
     * A unique identifier for the `AmmoniteMenuItem`.  This is not user visible.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          "Name",
                                                          "Name of the item",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
    /**
     * AmmoniteMenuItem:label:
     *
     * A user visible string describing the `AmmoniteMenuItem`.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_LABEL,
                                     g_param_spec_string ("label",
                                                          "Label",
                                                          "Label to display to the user",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    /**
     * AmmoniteMenuItem:tip:
     *
     * This property has no effect.
     *
     * Deprecated: 4.1
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_TIP,
                                     g_param_spec_string ("tip",
                                                          "Tip",
                                                          "Tooltip for the menu item",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    /**
     * AmmoniteMenuItem:icon:
     *
     * This property has no effect.
     *
     * Deprecated: 4.1
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_ICON,
                                     g_param_spec_string ("icon",
                                                          "Icon",
                                                          "Name of the icon to display in the menu item",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    /**
     * AmmoniteMenuItem:sensitive:
     *
     * Whether the `AmmoniteMenuItem` should be sensitive (i.e. clickable).
     *
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_SENSITIVE,
                                     g_param_spec_boolean ("sensitive",
                                                           "Sensitive",
                                                           "Whether the menu item is sensitive",
                                                           TRUE,
                                                           G_PARAM_READWRITE));
    /**
     * AmmoniteMenuItem:priority:
     *
     * This property has no effect.
     *
     * Deprecated: 4.1
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_PRIORITY,
                                     g_param_spec_boolean ("priority",
                                                           "Priority",
                                                           "Show priority text in toolbars",
                                                           TRUE,
                                                           G_PARAM_READWRITE));
    /**
     * AmmoniteMenuItem:menu:
     *
     * A submenu for the current `AmmoniteMenuItem`.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_MENU,
                                     g_param_spec_object ("menu",
                                                          "Menu",
                                                          "The menu belonging to this item. May be null.",
                                                          AMMONITE_TYPE_MENU,
                                                          G_PARAM_READWRITE));
}
