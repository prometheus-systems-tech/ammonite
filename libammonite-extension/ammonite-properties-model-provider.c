/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ammonite-properties-model-provider.h"

/**
 * AmmonitePropertiesModelProvider:
 *
 * Interface to provide additional properties.
 *
 * `AmmonitePropertiesModelProvider` allows extensions to provide additional
 * information for the file properties.
 */

G_DEFINE_INTERFACE (AmmonitePropertiesModelProvider, ammonite_properties_model_provider, G_TYPE_OBJECT)

static void
ammonite_properties_model_provider_default_init (AmmonitePropertiesModelProviderInterface *klass)
{
}

/**
 * ammonite_properties_model_provider_get_models:
 * @files: (element-type AmmoniteFileInfo): a list of files
 *
 * This function is called by the application when it wants properties models
 * from the extension.
 *
 * This function is called in the main thread before the Properties are shown,
 * so it should return quickly. The models can be populated and updated
 * asynchronously.
 *
 * Returns: (nullable) (element-type AmmonitePropertiesModel) (transfer full): a list of models.
 */
GList *
ammonite_properties_model_provider_get_models (AmmonitePropertiesModelProvider *self,
                                               GList                           *files)
{
    AmmonitePropertiesModelProviderInterface *iface;

    g_return_val_if_fail (AMMONITE_IS_PROPERTIES_MODEL_PROVIDER (self), NULL);

    iface = AMMONITE_PROPERTIES_MODEL_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->get_models != NULL, NULL);

    return iface->get_models (self, files);
}
