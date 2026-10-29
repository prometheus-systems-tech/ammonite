
/*
 * SPDX-FileCopyrightText: 2004 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>

#include "ammonite-enums.h"


/**
 * ammonite_capitalize_str:
 * @string: input string
 *
 * Returns: a newly allocated copy of @string, with the first letter capitalized.
 * If @string is %NULL, returns %NULL.
 */
char      * ammonite_capitalize_str                 (const char    *string);

void        ammonite_gmenu_set_from_model           (GMenu             *target_menu,
                                                     GMenuModel        *source_model);

void        ammonite_g_menu_model_set_for_mode      (GMenuModel        *model,
                                                     AmmoniteMode       mode);

void        ammonite_g_menu_model_set_for_view      (GMenuModel        *model,
                                                     const char        *view_name);

void        ammonite_menu_item_change_attribute     (GMenuModel        *menu_model,
                                                     const gchar       *item_name,
                                                     const gchar       *attribute,
                                                     const gchar       *new_value);

void        ammonite_ui_frame_video                 (GtkSnapshot       *snapshot,
                                                     gdouble            width,
                                                     gdouble            height);
void        ammonite_ui_draw_icon_dashed_border     (GtkSnapshot     *snapshot,
                                                     graphene_rect_t *rect,
                                                     GdkRGBA          color);
GtkSvg    * ammonite_ui_get_cut_svg                 (gboolean           large);
void        ammonite_ui_draw_svg                    (GtkSnapshot           *snapshot,
                                                     GtkSvg                *svg,
                                                     const graphene_rect_t *rect,
                                                     GdkRGBA                color);
GdkPaintable *
ammonite_ui_draw_stacked_icons (GQueue    *icons,
                                uint       size,
                                GtkWidget *widget,
                                guint      n_items);

gboolean    ammonite_date_time_is_between_dates     (GDateTime         *date,
                                                     GDateTime         *initial_date,
                                                     GDateTime         *end_date);

void
ammonite_show_ok_dialog (const char *heading,
                         const char *body,
                         GtkWidget  *parent);

typedef void (* TimedWaitCancelCallback) (gpointer callback_data);

/* Dialog for cancelling something that normally is fast enough not to need a dialog. */
void        ammonite_ui_timed_wait_start            (TimedWaitCancelCallback  cancel_callback,
                                                     gpointer                 callback_data,
                                                     const char              *wait_message,
                                                     GtkWindow               *parent_window);
void        ammonite_ui_timed_wait_start_full       (int                      duration,
                                                     TimedWaitCancelCallback  cancel_callback,
                                                     gpointer                 callback_data,
                                                     const char              *wait_message,
                                                     GtkWindow               *parent_window);
void        ammonite_ui_timed_wait_stop             (TimedWaitCancelCallback  cancel_callback,
                                                     gpointer                 callback_data);

void        show_unmount_progress_cb                (GMountOperation   *op,
                                                     const gchar       *message,
                                                     gint64             time_left,
                                                     gint64             bytes_left,
                                                     gpointer           user_data);
void        show_unmount_progress_aborted_cb        (GMountOperation   *op,
                                                     gpointer           user_data);
