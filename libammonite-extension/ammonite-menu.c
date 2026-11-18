/*
 * SPDX-FileCopyrightText: 2005 Raffaele Sandrini
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Raffaele Sandrini <rasa@gmx.ch>
 */

#include <config.h>
#include "ammonite-menu.h"

#include <glib.h>

/**
 * AmmoniteMenu:
 *
 * A submenu linked to a [class@AmmoniteMenuItem].
 *
 * `AmmoniteMenu` is an object that describes a submenu in a file manager
 * menu. Extensions can provide `AmmoniteMenu` objects by attaching them to
 * `AmmoniteMenuItem` objects, using [method@AmmoniteMenuItem.set_submenu].
 */

/**
 * AmmoniteMenuItem:
 *
 * An individual item with a Ammonite context menu.
 *
 * `AmmoniteMenuItem` is an object that describes an item in a file manager
 * menu. Extensions can provide `AmmoniteMenuItem` objects by registering a
 * [iface@AmmoniteMenuProvider] and returning them from
 * [method@AmmoniteMenuProvider.get_file_items], or
 * [method@AmmoniteMenuProvider.get_background_items], which will be called by the
 * main application when creating menus.
 */

struct _AmmoniteMenu
{
    GObject parent_instance;

    GList *item_list;
};

G_DEFINE_TYPE (AmmoniteMenu, ammonite_menu, G_TYPE_OBJECT);

/**
 * ammonite_menu_append_item:
 *
 * Append a [class@AmmoniteMenuItem] to the current `AmmoniteMenu`.
 *
 */
void
ammonite_menu_append_item (AmmoniteMenu     *self,
                           AmmoniteMenuItem *menu_item)
{
    g_return_if_fail (AMMONITE_IS_MENU (self));
    g_return_if_fail (AMMONITE_IS_MENU_ITEM (menu_item));

    self->item_list = g_list_append (self->item_list, g_object_ref (menu_item));
}

/**
 * ammonite_menu_get_items:
 *
 * Get a list of [class@AmmoniteMenuItem] for the current `AmmoniteMenu`.
 *
 * Returns: (nullable) (element-type AmmoniteMenuItem) (transfer full): the provided #AmmoniteMenuItem list
 */
GList *
ammonite_menu_get_items (AmmoniteMenu *self)
{
    GList *item_list;

    g_return_val_if_fail (AMMONITE_IS_MENU (self), NULL);

    item_list = g_list_copy (self->item_list);
    g_list_foreach (item_list, (GFunc) g_object_ref, NULL);

    return item_list;
}

/**
 * ammonite_menu_item_list_free:
 * @item_list: (element-type AmmoniteMenuItem): a list of #AmmoniteMenuItem
 *
 * Deep frees a list of `AmmoniteMenuItem`.
 *
 */
void
ammonite_menu_item_list_free (GList *item_list)
{
    g_return_if_fail (item_list != NULL);

    g_list_foreach (item_list, (GFunc) g_object_unref, NULL);
    g_list_free (item_list);
}

static void
ammonite_menu_finalize (GObject *object)
{
    AmmoniteMenu *menu = AMMONITE_MENU (object);

    g_clear_pointer (&menu->item_list, g_list_free);

    G_OBJECT_CLASS (ammonite_menu_parent_class)->finalize (object);
}

static void
ammonite_menu_init (AmmoniteMenu *self)
{
    self->item_list = NULL;
}

static void
ammonite_menu_class_init (AmmoniteMenuClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = ammonite_menu_finalize;
}

/**
 * ammonite_menu_new:
 *
 * Creates a new `AmmoniteMenu`
 *
 * Returns: a new `AmmoniteMenu`.
 */
AmmoniteMenu *
ammonite_menu_new (void)
{
    return g_object_new (AMMONITE_TYPE_MENU, NULL);
}
