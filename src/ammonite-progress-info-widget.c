/*
 * ammonite-progress-info-widget.h: file operation progress user interface.
 *
 * SPDX-FileCopyrightText: 2007, 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#include "config.h"
#include "ammonite-progress-info-widget.h"

#include "ammonite-progress-info.h"

#include <glib/gi18n.h>

struct _AmmoniteProgressInfoWidgetPrivate
{
    AmmoniteProgressInfo *info;

    GtkWidget *status;     /* GtkLabel */
    GtkWidget *details;     /* GtkLabel */
    GtkWidget *progress_bar;
    GtkWidget *button;
};

enum
{
    PROP_INFO = 1,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

G_DEFINE_TYPE_WITH_PRIVATE (AmmoniteProgressInfoWidget, ammonite_progress_info_widget,
                            GTK_TYPE_GRID);

static void
info_finished (AmmoniteProgressInfoWidget *self)
{
    gtk_widget_set_sensitive (self->priv->button, FALSE);
    if (!ammonite_progress_info_get_is_cancelled (self->priv->info))
    {
        gtk_button_set_icon_name (GTK_BUTTON (self->priv->button), "file-operation-finished-symbolic");
        /* Translators: This describes an operation, such as copying or compressing files, as being completed. */
        gtk_widget_set_tooltip_text (GTK_WIDGET (self->priv->button), _("Operation Completed"));
    }
}

static void
info_cancelled (AmmoniteProgressInfoWidget *self)
{
    gtk_widget_set_sensitive (self->priv->button, FALSE);
    gtk_button_set_icon_name (GTK_BUTTON (self->priv->button), "cancel-operation-symbolic");
    gtk_widget_set_has_tooltip (GTK_WIDGET (self->priv->button), FALSE);
}

static void
update_data (AmmoniteProgressInfoWidget *self)
{
    char *status, *details;
    char *markup;

    status = ammonite_progress_info_get_status (self->priv->info);
    gtk_label_set_text (GTK_LABEL (self->priv->status), status);
    g_free (status);

    details = ammonite_progress_info_get_details (self->priv->info);
    markup = g_markup_printf_escaped ("<span size='small'>%s</span>", details);
    gtk_label_set_markup (GTK_LABEL (self->priv->details), markup);
    g_free (details);
    g_free (markup);
}

static void
update_progress (AmmoniteProgressInfoWidget *self)
{
    double progress;

    progress = ammonite_progress_info_get_progress (self->priv->info);
    if (progress < 0)
    {
        gtk_progress_bar_pulse (GTK_PROGRESS_BAR (self->priv->progress_bar));
    }
    else
    {
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->priv->progress_bar), progress);
    }
}

static void
button_clicked (GtkWidget                  *button,
                AmmoniteProgressInfoWidget *self)
{
    if (!ammonite_progress_info_get_is_finished (self->priv->info))
    {
        ammonite_progress_info_cancel (self->priv->info);
    }
}

static void
ammonite_progress_info_widget_dispose (GObject *obj)
{
    AmmoniteProgressInfoWidget *self = AMMONITE_PROGRESS_INFO_WIDGET (obj);

    if (self->priv->info != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->priv->info, self);
    }
    g_clear_object (&self->priv->info);

    gtk_widget_dispose_template (GTK_WIDGET (self), AMMONITE_TYPE_PROGRESS_INFO_WIDGET);

    G_OBJECT_CLASS (ammonite_progress_info_widget_parent_class)->dispose (obj);
}

static void
ammonite_progress_info_widget_constructed (GObject *obj)
{
    AmmoniteProgressInfoWidget *self = AMMONITE_PROGRESS_INFO_WIDGET (obj);

    G_OBJECT_CLASS (ammonite_progress_info_widget_parent_class)->constructed (obj);

    if (ammonite_progress_info_get_is_finished (self->priv->info) &&
        !ammonite_progress_info_get_is_cancelled (self->priv->info))
    {
        info_finished (self);
    }
    else if (ammonite_progress_info_get_is_cancelled (self->priv->info))
    {
        info_cancelled (self);
    }

    gtk_widget_set_sensitive (self->priv->button,
                              !ammonite_progress_info_get_is_finished (self->priv->info) &&
                              !ammonite_progress_info_get_is_cancelled (self->priv->info));

    g_signal_connect_swapped (self->priv->info,
                              "changed",
                              G_CALLBACK (update_data), self);
    g_signal_connect_swapped (self->priv->info,
                              "progress-changed",
                              G_CALLBACK (update_progress), self);
    g_signal_connect_swapped (self->priv->info,
                              "finished",
                              G_CALLBACK (info_finished), self);
    g_signal_connect_swapped (self->priv->info,
                              "cancelled",
                              G_CALLBACK (info_cancelled), self);

    update_data (self);
    update_progress (self);
}

static void
ammonite_progress_info_widget_set_property (GObject      *object,
                                            guint         property_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
    AmmoniteProgressInfoWidget *self = AMMONITE_PROGRESS_INFO_WIDGET (object);

    switch (property_id)
    {
        case PROP_INFO:
        {
            self->priv->info = g_value_dup_object (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
ammonite_progress_info_widget_init (AmmoniteProgressInfoWidget *self)
{
    self->priv = ammonite_progress_info_widget_get_instance_private (self);

    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self->priv->button, "clicked",
                      G_CALLBACK (button_clicked), self);
}

static void
ammonite_progress_info_widget_class_init (AmmoniteProgressInfoWidgetClass *klass)
{
    GObjectClass *oclass;
    GtkWidgetClass *widget_class;

    widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);
    oclass->set_property = ammonite_progress_info_widget_set_property;
    oclass->constructed = ammonite_progress_info_widget_constructed;
    oclass->dispose = ammonite_progress_info_widget_dispose;

    properties[PROP_INFO] =
        g_param_spec_object ("info",
                             "AmmoniteProgressInfo",
                             "The AmmoniteProgressInfo associated with this widget",
                             AMMONITE_TYPE_PROGRESS_INFO,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/ammonite/ammonite/ui/ammonite-progress-info-widget.ui");

    gtk_widget_class_bind_template_child_private (widget_class, AmmoniteProgressInfoWidget, status);
    gtk_widget_class_bind_template_child_private (widget_class, AmmoniteProgressInfoWidget, details);
    gtk_widget_class_bind_template_child_private (widget_class, AmmoniteProgressInfoWidget, progress_bar);
    gtk_widget_class_bind_template_child_private (widget_class, AmmoniteProgressInfoWidget, button);
}

GtkWidget *
ammonite_progress_info_widget_new (AmmoniteProgressInfo *info)
{
    return g_object_new (AMMONITE_TYPE_PROGRESS_INFO_WIDGET,
                         "info", info,
                         NULL);
}
