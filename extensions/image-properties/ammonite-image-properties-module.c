/*
 * SPDX-FileCopyrightText: 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include "ammonite-image-properties-model-provider.h"

#include <glib/gi18n-lib.h>

#include <ammonite-extension.h>

void
ammonite_module_initialize (GTypeModule *module)
{
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    ammonite_image_properties_model_provider_load (module);
}

void
ammonite_module_shutdown (void)
{
}

void
ammonite_module_list_types (const GType **types,
                            int          *num_types)
{
    static GType type_list[1] = { 0 };

    g_assert (types != NULL);
    g_assert (num_types != NULL);

    type_list[0] = AMMONITE_TYPE_IMAGE_PROPERTIES_MODEL_PROVIDER;

    *types = type_list;
    *num_types = G_N_ELEMENTS (type_list);
}
