/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_PROPERTIES_MODEL (ammonite_properties_model_get_type ())

G_DECLARE_FINAL_TYPE (AmmonitePropertiesModel,
                      ammonite_properties_model,
                      AMMONITE, PROPERTIES_MODEL,
                      GObject)

AmmonitePropertiesModel *ammonite_properties_model_new (const char *title,
                                                        GListModel *model);

const char *ammonite_properties_model_get_title (AmmonitePropertiesModel *self);

void ammonite_properties_model_set_title (AmmonitePropertiesModel *self,
                                          const char              *title);

GListModel * ammonite_properties_model_get_model (AmmonitePropertiesModel *self);


G_END_DECLS
