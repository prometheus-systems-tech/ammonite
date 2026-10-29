/*
 * ammonite-menu.h - Menus exported by AmmoniteMenuProvider objects.
 *
 * SPDX-FileCopyrightText: 2005 Raffaele Sandrini
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 *         Raffaele Sandrini <rasa@gmx.ch>
 */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_MENU (ammonite_menu_get_type ())
#define AMMONITE_TYPE_MENU_ITEM (ammonite_menu_item_get_type ())

G_DECLARE_FINAL_TYPE     (AmmoniteMenu, ammonite_menu,
                          AMMONITE, MENU,
                          GObject)
G_DECLARE_DERIVABLE_TYPE (AmmoniteMenuItem, ammonite_menu_item,
                          AMMONITE, MENU_ITEM,
                          GObject)

struct _AmmoniteMenuItemClass
{
    GObjectClass parent;

    void (*activate) (AmmoniteMenuItem *item);
};

AmmoniteMenu     *ammonite_menu_new              (void);

void              ammonite_menu_append_item      (AmmoniteMenu     *menu,
                                                  AmmoniteMenuItem *item);

GList            *ammonite_menu_get_items        (AmmoniteMenu     *menu);

void              ammonite_menu_item_list_free   (GList            *item_list);

AmmoniteMenuItem *ammonite_menu_item_new         (const char       *name,
                                                  const char       *label,
                                                  const char       *tip,
                                                  const char       *icon);

void              ammonite_menu_item_activate    (AmmoniteMenuItem *item);

void              ammonite_menu_item_set_submenu (AmmoniteMenuItem *item,
                                                  AmmoniteMenu     *menu);
G_END_DECLS
