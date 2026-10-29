/*
 * SPDX-FileCopyrightText: 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_IMAGE (ammonite_image_get_type())

G_DECLARE_FINAL_TYPE (AmmoniteImage, ammonite_image, AMMONITE, IMAGE, GtkWidget)

typedef enum
{
    AMMONITE_IMAGE_STATUS_THUMBNAIL,
    AMMONITE_IMAGE_STATUS_LOADING_ATTRIBUTES,
    AMMONITE_IMAGE_STATUS_LOADING_THUMBNAIL,
    AMMONITE_IMAGE_STATUS_FALLBACK,
} AmmoniteImageStatus;

AmmoniteImage *
ammonite_image_new (void);

GFile *
ammonite_image_get_source                               (AmmoniteImage *self);
void
ammonite_image_set_source                               (AmmoniteImage *image,
                                                         GFile         *source);

int
ammonite_image_get_size                                 (AmmoniteImage *self);
void
ammonite_image_set_size                                 (AmmoniteImage *self,
                                                         gint           size);

GdkPaintable  *
ammonite_image_get_fallback                             (AmmoniteImage *self);
void
ammonite_image_set_fallback                             (AmmoniteImage *self,
                                                         GdkPaintable  *paintable);

AmmoniteImageStatus
ammonite_image_get_status                               (AmmoniteImage *self);

G_END_DECLS

