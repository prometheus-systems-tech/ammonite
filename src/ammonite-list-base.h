/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "ammonite-types.h"

#include "ammonite-view-info.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define AMMONITE_TYPE_LIST_BASE (ammonite_list_base_get_type())

G_DECLARE_DERIVABLE_TYPE (AmmoniteListBase, ammonite_list_base, AMMONITE, LIST_BASE, AdwBin)

struct _AmmoniteListBaseClass
{
        AdwBinClass parent_class;

        /* Subclasses must provide implementation. */
        AmmoniteViewInfo (*get_view_info)     (AmmoniteListBase *self);
        guint      (*get_icon_size)  (AmmoniteListBase *self);
        GVariant  *(*get_sort_state)          (AmmoniteListBase *self);
        int        (*get_zoom_level)          (AmmoniteListBase *self);
        void       (*scroll_to)      (AmmoniteListBase   *self,
                                      guint               position,
                                      GtkListScrollFlags  flags,
                                      GtkScrollInfo      *scroll);
        void       (*set_enable_rubberband)   (AmmoniteListBase *self,
                                               gboolean          enabled);
        void       (*set_sort_state)          (AmmoniteListBase *self,
                                               GVariant         *sort_state);
        void       (*set_zoom_level)          (AmmoniteListBase *self,
                                               int               new_zoom_level);

        /* Subclasses may override base implementation. */
        void (*popup_background_context_menu) (AmmoniteListBase *self,
                                               double            x,
                                               double            y);
        AmmoniteViewItem *(*get_backing_item) (AmmoniteListBase *self);
        void       (*preview_selection_event) (AmmoniteListBase *self,
                                               GtkDirectionType  direction);

        /* Subclass override must chain-up to base implementation. */
        void       (*setup_directory) (AmmoniteListBase  *self,
                                       AmmoniteDirectory *directory);
};

void ammonite_list_base_disable_dnd (AmmoniteListBase *self);
AmmoniteViewItem *ammonite_list_base_get_backing_item (AmmoniteListBase *self);
GtkWidget *ammonite_list_base_get_selected_item_ui (AmmoniteListBase  *self);
AmmoniteViewInfo ammonite_list_base_get_view_info  (AmmoniteListBase  *self) G_GNUC_PURE;
int     ammonite_list_base_get_zoom_level          (AmmoniteListBase  *self);
void ammonite_list_base_preview_selection_event (AmmoniteListBase *self,
                                                 GtkDirectionType  direction);
void ammonite_list_base_set_cursor (AmmoniteListBase *self,
                                    guint             position,
                                    gboolean          select,
                                    gboolean          scroll_to);
void    ammonite_list_base_set_model               (AmmoniteListBase  *self,
                                                    AmmoniteViewModel *model);
void    ammonite_list_base_add_overlay             (AmmoniteListBase  *self,
                                                    GtkWidget         *widget);
void    ammonite_list_base_set_zoom_level          (AmmoniteListBase  *self,
                                                    int                new_zoom_level);
void    ammonite_list_base_setup_directory         (AmmoniteListBase  *self,
                                                    AmmoniteDirectory *directory);

G_END_DECLS
