/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_PROPERTIES_ITEM (ammonite_properties_item_get_type ())

G_DECLARE_FINAL_TYPE (AmmonitePropertiesItem,
                      ammonite_properties_item,
                      AMMONITE, PROPERTIES_ITEM,
                      GObject)

AmmonitePropertiesItem *ammonite_properties_item_new (const char *name,
                                                      const char *value);

const char *ammonite_properties_item_get_name (AmmonitePropertiesItem *self);

const char * ammonite_properties_item_get_value (AmmonitePropertiesItem *self);


G_END_DECLS
