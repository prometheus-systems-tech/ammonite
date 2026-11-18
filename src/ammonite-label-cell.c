/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-label-cell.h"

/* Needed for AmmoniteColumn (full GType). */
#include <ammonite-extension.h>

#include "ammonite-file.h"
#include "ammonite-global-preferences.h"
#include "ammonite-view-item.h"

struct _AmmoniteLabelCell
{
    AmmoniteViewCell parent_instance;

    GSignalGroup *item_signal_group;

    AmmoniteColumn *column;
    GQuark attribute_q;

    GtkLabel *label;
};

G_DEFINE_FINAL_TYPE (AmmoniteLabelCell, ammonite_label_cell, AMMONITE_TYPE_VIEW_CELL)

enum
{
    PROP_0,
    PROP_COLUMN,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
update_label (AmmoniteLabelCell *self)
{
    g_autoptr (AmmoniteViewItem) item = NULL;
    AmmoniteFile *file;
    g_autofree gchar *string = NULL;

    item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = ammonite_view_item_get_file (item);

    string = ammonite_file_get_string_attribute_q (file, self->attribute_q);
    gtk_label_set_text (self->label, string);
}

static void
ammonite_label_cell_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
    AmmoniteLabelCell *self = AMMONITE_LABEL_CELL (object);

    switch (prop_id)
    {
        case PROP_COLUMN:
        {
            self->column = g_value_get_object (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_label_cell_init (AmmoniteLabelCell *self)
{
    GtkWidget *child;

    child = gtk_label_new (NULL);
    gtk_widget_set_parent (child, GTK_WIDGET (self));
    gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (child, "dimmed");
    self->label = GTK_LABEL (child);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (AMMONITE_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    G_CALLBACK (update_label), self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             G_CALLBACK (update_label), self,
                             G_CONNECT_SWAPPED);
    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);
}

static void
ammonite_label_cell_constructed (GObject *object)
{
    AmmoniteLabelCell *self = AMMONITE_LABEL_CELL (object);
    g_autofree gchar *column_name = NULL;
    gfloat xalign;

    G_OBJECT_CLASS (ammonite_label_cell_parent_class)->constructed (object);

    g_object_get (self->column,
                  "attribute_q", &self->attribute_q,
                  "name", &column_name,
                  "xalign", &xalign,
                  NULL);
    gtk_label_set_xalign (self->label, xalign);

    if (ammonite_file_is_date_sort_attribute_q (self->attribute_q))
    {
        g_signal_connect_object (ammonite_preferences, "changed::" AMMONITE_PREFERENCES_DATE_TIME_FORMAT,
                                 G_CALLBACK (update_label), self,
                                 G_CONNECT_SWAPPED);
    }

    if (g_strcmp0 (column_name, "permissions") == 0)
    {
        gtk_widget_add_css_class (GTK_WIDGET (self->label), "monospace");
    }
    else
    {
        gtk_widget_add_css_class (GTK_WIDGET (self->label), "numeric");
    }
}

static void
ammonite_label_cell_dispose (GObject *object)
{
    AmmoniteLabelCell *self = (AmmoniteLabelCell *) object;

    g_clear_object (&self->item_signal_group);
    if (self->label)
    {
        gtk_widget_unparent (GTK_WIDGET (self->label));
        self->label = NULL;
    }
    G_OBJECT_CLASS (ammonite_label_cell_parent_class)->dispose (object);
}

static void
ammonite_label_cell_class_init (AmmoniteLabelCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->set_property = ammonite_label_cell_set_property;
    object_class->constructed = ammonite_label_cell_constructed;
    object_class->dispose = ammonite_label_cell_dispose;

    properties[PROP_COLUMN] = g_param_spec_object ("column",
                                                   "", "",
                                                   AMMONITE_TYPE_COLUMN,
                                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

AmmoniteViewCell *
ammonite_label_cell_new (AmmoniteListBase *view,
                         AmmoniteColumn   *column)
{
    return AMMONITE_VIEW_CELL (g_object_new (AMMONITE_TYPE_LABEL_CELL,
                                             "view", view,
                                             "column", column,
                                             NULL));
}
