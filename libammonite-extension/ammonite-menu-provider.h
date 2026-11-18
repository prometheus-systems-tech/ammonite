/*
 * ammonite-menu-provider.h - Interface for Ammonite extensions that provide context menu items.
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

/* This interface is implemented by Ammonite extensions that want to
 * add context menu entries to files.  Extensions are called when
 * Ammonite constructs the context menu for a file.  They are passed a
 * list of AmmoniteFileInfo objects which holds the current selection */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include "ammonite-file-info.h"

G_BEGIN_DECLS

#define AMMONITE_TYPE_MENU_PROVIDER (ammonite_menu_provider_get_type ())

G_DECLARE_INTERFACE (AmmoniteMenuProvider, ammonite_menu_provider,
                     AMMONITE, MENU_PROVIDER,
                     GObject)

struct _AmmoniteMenuProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_file_items)       (AmmoniteMenuProvider *provider,
                                    GList                *files);
    GList *(*get_background_items) (AmmoniteMenuProvider *provider,
                                    AmmoniteFileInfo     *current_folder);
};

GList  *ammonite_menu_provider_get_file_items           (AmmoniteMenuProvider *provider,
                                                         GList                *files);

GList *ammonite_menu_provider_get_background_items      (AmmoniteMenuProvider *provider,
                                                         AmmoniteFileInfo     *current_folder);

void   ammonite_menu_provider_emit_items_updated_signal (AmmoniteMenuProvider *provider);

G_END_DECLS
