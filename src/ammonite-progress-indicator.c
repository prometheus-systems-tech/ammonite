/*
 * SPDX-FileCopyrightText: 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ammonite-progress-indicator.h"

#include "ammonite-file-operations.h"
#include "ammonite-progress-info.h"
#include "ammonite-progress-info-manager.h"
#include "ammonite-progress-info-widget.h"
#include "ammonite-progress-paintable.h"
#include "ammonite-window.h"

#define OPERATION_MINIMUM_TIME 2 /*s */
#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 /*ms */
#define REMOVE_FINISHED_OPERATIONS_TIEMOUT 3 /*s */
#define MAX_ITEMS_IN_SIDEBAR 4

enum
{
    PROP_0,
    PROP_POPOVER_POSITION,
    PROP_REVEAL,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

struct _AmmoniteProgressIndicator
{
    AdwBin parent_instance;

    guint start_operations_timeout_id;
    guint remove_finished_operations_timeout_id;
    guint operations_button_attention_timeout_id;

    GtkWidget *operations_button;
    GtkWidget *operations_popover;
    GtkWidget *operations_list;
    GListStore *progress_infos_model;
    GtkWidget *sidebar_list;

    gboolean reveal;
    GtkPositionType popover_position;

    AmmoniteProgressInfoManager *progress_manager;
};

G_DEFINE_FINAL_TYPE (AmmoniteProgressIndicator, ammonite_progress_indicator, ADW_TYPE_BIN);


static void update_operations (AmmoniteProgressIndicator *self);

static inline gboolean
should_show_progress_info (AmmoniteProgressInfo *info)
{
    return ammonite_progress_info_get_total_elapsed_time (info) +
           ammonite_progress_info_get_remaining_time (info) > OPERATION_MINIMUM_TIME;
}

static GList *
get_filtered_progress_infos (AmmoniteProgressIndicator *self)
{
    GList *l;
    GList *filtered_progress_infos;
    GList *progress_infos;

    progress_infos = ammonite_progress_info_manager_get_all_infos (self->progress_manager);
    filtered_progress_infos = NULL;

    for (l = progress_infos; l != NULL; l = l->next)
    {
        if (should_show_progress_info (l->data))
        {
            filtered_progress_infos = g_list_append (filtered_progress_infos, l->data);
        }
    }

    return filtered_progress_infos;
}

static gboolean
should_hide_operations_button (AmmoniteProgressIndicator *self)
{
    g_autoptr (GList) progress_infos = get_filtered_progress_infos (self);

    for (GList *l = progress_infos; l != NULL; l = l->next)
    {
        if (!ammonite_progress_info_get_is_cancelled (l->data) &&
            !ammonite_progress_info_get_is_finished (l->data))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void
ammonite_progress_indicator_set_reveal (AmmoniteProgressIndicator *self,
                                        gboolean                   reveal)
{
    if (reveal != self->reveal)
    {
        self->reveal = reveal;
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REVEAL]);
    }
}

static void
on_remove_finished_operations_timeout (AmmoniteProgressIndicator *self)
{
    ammonite_progress_info_manager_remove_finished_or_cancelled_infos (self->progress_manager);
    if (should_hide_operations_button (self))
    {
        ammonite_progress_indicator_set_reveal (self, FALSE);
    }
    else
    {
        update_operations (self);
    }

    self->remove_finished_operations_timeout_id = 0;
}

static void
unschedule_remove_finished_operations (AmmoniteProgressIndicator *self)
{
    if (self->remove_finished_operations_timeout_id != 0)
    {
        g_source_remove (self->remove_finished_operations_timeout_id);
        self->remove_finished_operations_timeout_id = 0;
    }
}

static void
schedule_remove_finished_operations (AmmoniteProgressIndicator *self)
{
    if (self->remove_finished_operations_timeout_id == 0)
    {
        self->remove_finished_operations_timeout_id =
            g_timeout_add_seconds_once (REMOVE_FINISHED_OPERATIONS_TIEMOUT,
                                        (GSourceOnceFunc) on_remove_finished_operations_timeout,
                                        self);
    }
}

static void
remove_operations_button_attention_style (AmmoniteProgressIndicator *self)
{
    gtk_widget_remove_css_class (GTK_WIDGET (self), "needs-attention");
}

static void
on_remove_operations_button_attention_style_timeout (AmmoniteProgressIndicator *self)
{
    remove_operations_button_attention_style (self);
    self->operations_button_attention_timeout_id = 0;
}

static void
unschedule_operations_button_attention_style (AmmoniteProgressIndicator *self)
{
    if (self->operations_button_attention_timeout_id != 0)
    {
        g_source_remove (self->operations_button_attention_timeout_id);
        self->operations_button_attention_timeout_id = 0;
    }
}

static void
add_operations_button_attention_style (AmmoniteProgressIndicator *self)
{
    unschedule_operations_button_attention_style (self);
    remove_operations_button_attention_style (self);

    gtk_widget_add_css_class (GTK_WIDGET (self), "needs-attention");
    self->operations_button_attention_timeout_id =
        g_timeout_add_once (NEEDS_ATTENTION_ANIMATION_TIMEOUT,
                            (GSourceOnceFunc) on_remove_operations_button_attention_style_timeout,
                            self);
}

static void
on_progress_info_cancelled (AmmoniteProgressIndicator *self)
{
    if (!ammonite_progress_manager_has_viewers (self->progress_manager))
    {
        schedule_remove_finished_operations (self);
    }
}

static void
on_progress_info_finished (AmmoniteProgressIndicator *self,
                           AmmoniteProgressInfo      *info)
{
    AmmoniteWindow *window;
    gchar *main_label;
    GFile *folder_to_open;

    window = AMMONITE_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

    if (!ammonite_progress_manager_has_viewers (self->progress_manager))
    {
        schedule_remove_finished_operations (self);
    }

    folder_to_open = ammonite_progress_info_get_destination (info);
    /* If destination is null, don't show a notification. This happens when the
     * operation is a trash operation, which we already show a different kind of
     * notification */
    if (!gtk_widget_is_visible (self->operations_popover) &&
        folder_to_open != NULL)
    {
        gboolean was_quick = ammonite_progress_info_get_total_elapsed_time (info) <= OPERATION_MINIMUM_TIME;
        if (!was_quick)
        {
            add_operations_button_attention_style (self);
        }

        main_label = ammonite_progress_info_get_status (info);
        ammonite_window_show_operation_notification (window,
                                                     main_label,
                                                     folder_to_open,
                                                     was_quick);
        g_free (main_label);
    }

    g_clear_object (&folder_to_open);
}

static GdkPaintable *
get_paintable (GtkListItem          *listitem,
               AmmoniteProgressInfo *info)
{
    if (info == NULL)
    {
        return NULL;
    }

    GtkWidget *box = gtk_list_item_get_child (listitem);
    GtkWidget *image = gtk_widget_get_first_child (box);

    GdkPaintable *paintable = ammonite_progress_paintable_new (image);

    g_object_bind_property (info, "icon-name", paintable, "icon-name", G_BINDING_SYNC_CREATE);
    g_signal_connect_object (info, "finished", G_CALLBACK (ammonite_progress_paintable_animate_done),
                             paintable, G_CONNECT_SWAPPED);

    g_object_bind_property (info, "progress", paintable, "progress", G_BINDING_SYNC_CREATE);

    return paintable;
}

static void
update_operations (AmmoniteProgressIndicator *self)
{
    g_autoptr (GList) progress_infos = get_filtered_progress_infos (self);
    gboolean should_show_progress_button = (progress_infos != NULL);

    g_list_store_remove_all (self->progress_infos_model);

    for (GList *l = progress_infos; l != NULL; l = l->next)
    {
        g_list_store_append (self->progress_infos_model, l->data);
    }

    if (should_show_progress_button && !self->reveal)
    {
        add_operations_button_attention_style (self);
        ammonite_progress_indicator_set_reveal (self, TRUE);
    }

    /* Since we removed the info widgets, we need to restore the focus */
    if (gtk_widget_get_visible (self->operations_popover))
    {
        gtk_widget_grab_focus (self->operations_popover);
    }
}

static gboolean
on_progress_info_started_timeout (AmmoniteProgressIndicator *self)
{
    GList *progress_infos;
    GList *filtered_progress_infos;

    update_operations (self);

    /* In case we didn't show the operations button because the operation total
     * time stimation is not good enough, update again to make sure we don't miss
     * a long time operation because of that */

    progress_infos = ammonite_progress_info_manager_get_all_infos (self->progress_manager);
    filtered_progress_infos = get_filtered_progress_infos (self);
    if (!ammonite_progress_manager_are_all_infos_finished_or_cancelled (self->progress_manager) &&
        g_list_length (progress_infos) != g_list_length (filtered_progress_infos))
    {
        g_list_free (filtered_progress_infos);
        return G_SOURCE_CONTINUE;
    }
    else
    {
        g_list_free (filtered_progress_infos);
        self->start_operations_timeout_id = 0;
        return G_SOURCE_REMOVE;
    }
}

static void
schedule_operations_start (AmmoniteProgressIndicator *self)
{
    if (self->start_operations_timeout_id == 0)
    {
        /* Timeout is a little more than what we require for a stimated operation
         * total time, to make sure the stimated total time is correct */
        self->start_operations_timeout_id =
            g_timeout_add (SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE * 1000 + 500,
                           (GSourceFunc) on_progress_info_started_timeout,
                           self);
    }
}

static void
unschedule_operations_start (AmmoniteProgressIndicator *self)
{
    if (self->start_operations_timeout_id != 0)
    {
        g_source_remove (self->start_operations_timeout_id);
        self->start_operations_timeout_id = 0;
    }
}

static void
on_progress_info_started (AmmoniteProgressIndicator *self)
{
    schedule_operations_start (self);
}

static void
on_new_progress_info (AmmoniteProgressInfoManager *manager,
                      AmmoniteProgressInfo        *info,
                      AmmoniteProgressIndicator   *self)
{
    g_signal_connect_object (info, "started",
                             G_CALLBACK (on_progress_info_started), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (info, "finished",
                             G_CALLBACK (on_progress_info_finished), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (info, "cancelled",
                             G_CALLBACK (on_progress_info_cancelled), self,
                             G_CONNECT_SWAPPED);
}

static void
on_operations_popover_notify_visible (AmmoniteProgressIndicator *self,
                                      GParamSpec                *pspec,
                                      GObject                   *popover)
{
    if (gtk_widget_get_visible (GTK_WIDGET (popover)))
    {
        unschedule_remove_finished_operations (self);
        ammonite_progress_manager_add_viewer (self->progress_manager,
                                              G_OBJECT (self));
    }
    else
    {
        ammonite_progress_manager_remove_viewer (self->progress_manager,
                                                 G_OBJECT (self));
    }
}

static void
on_progress_has_viewers_changed (AmmoniteProgressInfoManager *manager,
                                 AmmoniteProgressIndicator   *self)
{
    if (ammonite_progress_manager_has_viewers (manager))
    {
        unschedule_remove_finished_operations (self);
        return;
    }

    if (ammonite_progress_manager_are_all_infos_finished_or_cancelled (manager))
    {
        unschedule_remove_finished_operations (self);
        schedule_remove_finished_operations (self);
    }
}

static GtkWidget *
operations_list_create_widget (GObject  *item,
                               gpointer  user_data)
{
    AmmoniteProgressInfo *info = AMMONITE_PROGRESS_INFO (item);
    GtkWidget *widget;

    widget = ammonite_progress_info_widget_new (info);
    gtk_widget_set_visible (widget, TRUE);

    return widget;
}

static void
direction_changed (GtkWidget        *self,
                   GtkTextDirection  previous_direction,
                   gpointer          user_data)
{
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POPOVER_POSITION]);
}

static void
ammonite_progress_indicator_get_property (GObject    *object,
                                          guint       property_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
    AmmoniteProgressIndicator *self = AMMONITE_PROGRESS_INDICATOR (object);

    switch (property_id)
    {
        case (PROP_POPOVER_POSITION):
        {
            /* Handles the default value of zero coming from the breakpoint */
            if (self->popover_position <= GTK_POS_RIGHT)
            {
                guint popover_position = (gtk_widget_get_direction (GTK_WIDGET (self)) != GTK_TEXT_DIR_RTL) ?
                                         GTK_POS_RIGHT : GTK_POS_LEFT;

                g_value_set_enum (value, popover_position);
            }
            else
            {
                g_value_set_enum (value, self->popover_position);
            }
        }
        break;

        case (PROP_REVEAL):
        {
            g_value_set_boolean (value, self->reveal);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
ammonite_progress_indicator_set_property (GObject      *object,
                                          guint         property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
    AmmoniteProgressIndicator *self = AMMONITE_PROGRESS_INDICATOR (object);

    switch (property_id)
    {
        case (PROP_POPOVER_POSITION):
        {
            self->popover_position = g_value_get_enum (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
ammonite_progress_indicator_constructed (GObject *object)
{
    AmmoniteProgressIndicator *self = AMMONITE_PROGRESS_INDICATOR (object);

    G_OBJECT_CLASS (ammonite_progress_indicator_parent_class)->constructed (object);

    self->progress_manager = ammonite_progress_info_manager_dup_singleton ();
    g_signal_connect_object (self->progress_manager, "new-progress-info",
                             G_CALLBACK (on_new_progress_info), self,
                             G_CONNECT_DEFAULT);
    g_signal_connect_object (self->progress_manager, "has-viewers-changed",
                             G_CALLBACK (on_progress_has_viewers_changed), self,
                             G_CONNECT_DEFAULT);

    self->progress_infos_model = g_list_store_new (AMMONITE_TYPE_PROGRESS_INFO);
    gtk_list_box_bind_model (GTK_LIST_BOX (self->operations_list),
                             G_LIST_MODEL (self->progress_infos_model),
                             (GtkListBoxCreateWidgetFunc) operations_list_create_widget,
                             NULL,
                             NULL);

    update_operations (self);

    GtkSliceListModel *slice = gtk_slice_list_model_new (g_object_ref (G_LIST_MODEL (self->progress_infos_model)),
                                                         0, MAX_ITEMS_IN_SIDEBAR);
    g_autoptr (GtkNoSelection) selection_model = gtk_no_selection_new (G_LIST_MODEL (slice));

    gtk_list_view_set_model (GTK_LIST_VIEW (self->sidebar_list), GTK_SELECTION_MODEL (selection_model));

    g_signal_connect (self->operations_popover, "show",
                      (GCallback) gtk_widget_grab_focus, NULL);
    g_signal_connect_swapped (self->operations_popover, "closed",
                              (GCallback) gtk_widget_grab_focus, self);
}

static void
ammonite_progress_indicator_dispose (GObject *obj)
{
    AmmoniteProgressIndicator *self = AMMONITE_PROGRESS_INDICATOR (obj);

    adw_bin_set_child (ADW_BIN (self), NULL);
    gtk_widget_dispose_template (GTK_WIDGET (self), AMMONITE_TYPE_PROGRESS_INDICATOR);

    G_OBJECT_CLASS (ammonite_progress_indicator_parent_class)->dispose (obj);
}

static void
ammonite_progress_indicator_finalize (GObject *obj)
{
    AmmoniteProgressIndicator *self = AMMONITE_PROGRESS_INDICATOR (obj);

    unschedule_remove_finished_operations (self);
    unschedule_operations_start (self);
    unschedule_operations_button_attention_style (self);

    g_clear_object (&self->progress_infos_model);
    g_clear_object (&self->progress_manager);

    G_OBJECT_CLASS (ammonite_progress_indicator_parent_class)->finalize (obj);
}

static void
ammonite_progress_indicator_class_init (AmmoniteProgressIndicatorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = ammonite_progress_indicator_constructed;
    object_class->dispose = ammonite_progress_indicator_dispose;
    object_class->finalize = ammonite_progress_indicator_finalize;
    object_class->get_property = ammonite_progress_indicator_get_property;
    object_class->set_property = ammonite_progress_indicator_set_property;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/ammonite/ammonite/ui/ammonite-progress-indicator.ui");
    gtk_widget_class_bind_template_child (widget_class, AmmoniteProgressIndicator, operations_button);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteProgressIndicator, operations_popover);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteProgressIndicator, operations_list);
    gtk_widget_class_bind_template_child (widget_class, AmmoniteProgressIndicator, sidebar_list);

    gtk_widget_class_bind_template_callback (widget_class, get_paintable);
    gtk_widget_class_bind_template_callback (widget_class, on_operations_popover_notify_visible);

    properties[PROP_POPOVER_POSITION] = g_param_spec_enum ("popover-position", NULL, NULL,
                                                           GTK_TYPE_POSITION_TYPE, 0,
                                                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_REVEAL] = g_param_spec_boolean ("reveal", NULL, NULL, FALSE,
                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ammonite_progress_indicator_init (AmmoniteProgressIndicator *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self, "direction-changed", G_CALLBACK (direction_changed), NULL);
}
