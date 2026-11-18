/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !defined (AMMONITE_EXTENSION_H) && !defined (AMMONITE_COMPILATION)
#warning "Only <ammonite-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_PROPERTIES_MODEL_PROVIDER (ammonite_properties_model_provider_get_type ())

G_DECLARE_INTERFACE (AmmonitePropertiesModelProvider,
                     ammonite_properties_model_provider,
                     AMMONITE, PROPERTIES_MODEL_PROVIDER,
                     GObject)

struct _AmmonitePropertiesModelProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_models) (AmmonitePropertiesModelProvider *provider,
                          GList                           *files);
};

GList *ammonite_properties_model_provider_get_models (AmmonitePropertiesModelProvider *provider,
                                                      GList                           *files);

G_END_DECLS
