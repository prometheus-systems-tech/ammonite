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

#pragma once

#include "ammonite-types.h"

#include <gtk/gtk.h>

#define AMMONITE_TYPE_PROGRESS_INFO_WIDGET ammonite_progress_info_widget_get_type()
#define AMMONITE_PROGRESS_INFO_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), AMMONITE_TYPE_PROGRESS_INFO_WIDGET, AmmoniteProgressInfoWidget))
#define AMMONITE_PROGRESS_INFO_WIDGET_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), AMMONITE_TYPE_PROGRESS_INFO_WIDGET, AmmoniteProgressInfoWidgetClass))
#define AMMONITE_IS_PROGRESS_INFO_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), AMMONITE_TYPE_PROGRESS_INFO_WIDGET))
#define AMMONITE_IS_PROGRESS_INFO_WIDGET_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), AMMONITE_TYPE_PROGRESS_INFO_WIDGET))
#define AMMONITE_PROGRESS_INFO_WIDGET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), AMMONITE_TYPE_PROGRESS_INFO_WIDGET, AmmoniteProgressInfoWidgetClass))

typedef struct _AmmoniteProgressInfoWidgetPrivate AmmoniteProgressInfoWidgetPrivate;

typedef struct {
	GtkGrid parent;

	/* private */
	AmmoniteProgressInfoWidgetPrivate *priv;
} AmmoniteProgressInfoWidget;

typedef struct {
	GtkGridClass parent_class;
} AmmoniteProgressInfoWidgetClass;

GType ammonite_progress_info_widget_get_type (void);

GtkWidget * ammonite_progress_info_widget_new (AmmoniteProgressInfo *info);
