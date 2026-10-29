/*
 * SPDX-FileCopyrightText: 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib-object.h>

#define AMMONITE_TYPE_IMAGE_PROPERTIES_MODEL_PROVIDER (ammonite_image_properties_model_provider_get_type ())

G_DECLARE_FINAL_TYPE (AmmoniteImagesPropertiesModelProvider,
                      ammonite_image_properties_model_provider,
                      AMMONITE, IMAGE_PROPERTIES_MODEL_PROVIDER,
                      GObject)

void ammonite_image_properties_model_provider_load (GTypeModule *module);
