/*
 * SPDX-FileCopyrightText: 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ammonite-network-view.h"

#include <glib/gi18n.h>

#include "ammonite-file.h"
#include "ammonite-list-base-private.h"
#include "ammonite-network-cell.h"
#include "ammonite-scheme.h"
#include "ammonite-view-item.h"
#include "ammonite-view-model.h"

struct _AmmoniteNetworkView
{
    AmmoniteListBase parent_instance;

    GtkListView *view_ui;
};

G_DEFINE_FINAL_TYPE (AmmoniteNetworkView, ammonite_network_view, AMMONITE_TYPE_LIST_BASE)

#define get_view_item(li) \
        (AMMONITE_VIEW_ITEM (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (gtk_list_item_get_item (li)))))

static const AmmoniteViewInfo network_view_info =
{
    .view_id = AMMONITE_VIEW_NETWORK_ID,
    .zoom_level_min = AMMONITE_LIST_ZOOM_LEVEL_SMALL,
    .zoom_level_max = AMMONITE_LIST_ZOOM_LEVEL_SMALL,
    .zoom_level_standard = AMMONITE_LIST_ZOOM_LEVEL_SMALL,
};

static AmmoniteViewInfo
real_get_view_info (AmmoniteListBase *list_base)
{
    return network_view_info;
}

enum
{
    SECTION_CONNECTED,
    SECTION_PREVIOUS,
    SECTION_AVAILABLE,
};

static inline gint
get_section (AmmoniteViewItem *item)
{
    AmmoniteFile *file = ammonite_view_item_get_file (item);

    if (ammonite_file_can_unmount (file))
    {
        return SECTION_CONNECTED;
    }

    g_autoptr (GFile) location = ammonite_file_get_location (file);

    if (g_file_has_uri_scheme (location, SCHEME_NETWORK))
    {
        return SECTION_AVAILABLE;
    }
    else
    {
        return SECTION_PREVIOUS;
    }
}

static gint
sort_network_sections (gconstpointer a,
                       gconstpointer b,
                       gpointer      user_data)
{
    GtkTreeListRow *row_a = GTK_TREE_LIST_ROW ((gpointer) a);
    GtkTreeListRow *row_b = GTK_TREE_LIST_ROW ((gpointer) b);
    g_autoptr (AmmoniteViewItem) item_a = AMMONITE_VIEW_ITEM (gtk_tree_list_row_get_item (row_a));
    g_autoptr (AmmoniteViewItem) item_b = AMMONITE_VIEW_ITEM (gtk_tree_list_row_get_item (row_b));

    return get_section (item_a) - get_section (item_b);
}

static gint
sort_network_items (gconstpointer a,
                    gconstpointer b,
                    gpointer      user_data)
{
    AmmoniteViewItem *item_a = AMMONITE_VIEW_ITEM ((gpointer) a);
    AmmoniteViewItem *item_b = AMMONITE_VIEW_ITEM ((gpointer) b);
    AmmoniteFile *file_a = ammonite_view_item_get_file (item_a);
    AmmoniteFile *file_b = ammonite_view_item_get_file (item_b);

    if (get_section (item_a) == SECTION_PREVIOUS &&
        get_section (item_b) == SECTION_PREVIOUS)
    {
        return ammonite_file_compare_for_sort (file_a, file_b,
                                               AMMONITE_FILE_SORT_BY_ATIME,
                                               FALSE, TRUE /* reversed */);
    }

    return ammonite_file_compare_for_sort (file_a, file_b,
                                           AMMONITE_FILE_SORT_BY_DISPLAY_NAME,
                                           FALSE, FALSE);
}

static void
real_set_zoom_level (AmmoniteListBase *list_base,
                     int               new_level)
{
    g_warn_if_fail (new_level == network_view_info.zoom_level_standard);
}

/* We only care about the keyboard activation part that GtkListView provides,
 * but we don't need any special filtering here. Indeed, we ask GtkListView
 * to not activate on single click, and we get to handle double clicks before
 * GtkListView does (as one of widget subclassing's goal is to modify the parent
 * class's behavior), while claiming the click gestures, so it means GtkListView
 * will never react to a click event to emit this signal. So we should be pretty
 * safe here with regards to our custom item click handling.
 */
static void
on_list_view_item_activated (GtkListView *list_view,
                             guint        position,
                             gpointer     user_data)
{
    AmmoniteNetworkView *self = AMMONITE_NETWORK_VIEW (user_data);

    ammonite_list_base_activate_selection (AMMONITE_LIST_BASE (self), FALSE);
}

static void
real_popup_background_context_menu (AmmoniteListBase *self,
                                    double            x,
                                    double            y)
{
    g_signal_stop_emission_by_name (G_OBJECT (self), "popup-background-context-menu");
}

static guint
real_get_icon_size (AmmoniteListBase *list_base_view)
{
    return AMMONITE_LIST_ICON_SIZE_SMALL;
}

static int
real_get_zoom_level (AmmoniteListBase *list_base_view)
{
    return network_view_info.zoom_level_standard;
}

static void
real_scroll_to (AmmoniteListBase   *list_base_view,
                guint               position,
                GtkListScrollFlags  flags,
                GtkScrollInfo      *scroll)
{
    AmmoniteNetworkView *self = AMMONITE_NETWORK_VIEW (list_base_view);

    gtk_list_view_scroll_to (self->view_ui, position, flags, scroll);
}

static GVariant *
real_get_sort_state (AmmoniteListBase *list_base)
{
    return g_variant_take_ref (g_variant_new ("(sb)", "invalid", FALSE));
}

static void
real_set_enable_rubberband (AmmoniteListBase *list_base,
                            gboolean          enabled)
{
    /* We ignore setting this value as it should always stay disabled. */
}

static void
real_set_sort_state (AmmoniteListBase *list_base,
                     GVariant         *value)
{
    /* No op */
}

static void
ammonite_network_view_dispose (GObject *object)
{
    G_OBJECT_CLASS (ammonite_network_view_parent_class)->dispose (object);
}

static void
ammonite_network_view_finalize (GObject *object)
{
    G_OBJECT_CLASS (ammonite_network_view_parent_class)->finalize (object);
}

static void
bind_cell (GtkSignalListItemFactory *factory,
           GtkListItem              *listitem,
           gpointer                  user_data)
{
    GtkWidget *cell = gtk_list_item_get_child (listitem);
    g_autoptr (AmmoniteViewItem) item = get_view_item (listitem);

    ammonite_view_item_set_item_ui (item, cell);
}

static void
unbind_cell (GtkSignalListItemFactory *factory,
             GtkListItem              *listitem,
             gpointer                  user_data)
{
    g_autoptr (AmmoniteViewItem) item = get_view_item (listitem);

    /* item may be NULL when row has just been destroyed. */
    if (item != NULL)
    {
        ammonite_view_item_set_item_ui (item, NULL);
    }
}

static void
setup_cell (GtkSignalListItemFactory *factory,
            GtkListItem              *listitem,
            gpointer                  user_data)
{
    AmmoniteNetworkView *self = AMMONITE_NETWORK_VIEW (user_data);
    AmmoniteViewCell *cell;
    GtkExpression *expression;

    cell = ammonite_network_cell_new (AMMONITE_LIST_BASE (self));
    gtk_list_item_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), cell, GTK_WIDGET (cell));

    /* Use file display name as accessible label. Explaining in pseudo-code:
     * listitem:accessible-name :- listitem:item:item:file:display-name */
    expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, NULL, "item");
    expression = gtk_property_expression_new (GTK_TYPE_TREE_LIST_ROW, expression, "item");
    expression = gtk_property_expression_new (AMMONITE_TYPE_VIEW_ITEM, expression, "file");
    expression = gtk_property_expression_new (AMMONITE_TYPE_FILE, expression, "display-name");
    gtk_expression_bind (expression, listitem, "accessible-label", listitem);
}

static void
bind_header (GtkSignalListItemFactory *factory,
             GtkListHeader            *listheader,
             gpointer                  user_data)
{
    GtkWidget *label = gtk_list_header_get_child (listheader);
    GtkTreeListRow *row = GTK_TREE_LIST_ROW (gtk_list_header_get_item (listheader));
    g_autoptr (AmmoniteViewItem) item = AMMONITE_VIEW_ITEM (gtk_tree_list_row_get_item (row));

    switch (get_section (item))
    {
        case SECTION_CONNECTED:
        {
            /* Translators: This refers to network places which are currently mounted */
            gtk_label_set_label (GTK_LABEL (label), _("Connected"));
        }
        break;

        case SECTION_PREVIOUS:
        {
            /* Translators: This refers to network servers the user has previously connected to */
            gtk_label_set_label (GTK_LABEL (label), _("Previous"));
        }
        break;

        case SECTION_AVAILABLE:
        {
            gtk_label_set_label (GTK_LABEL (label), _("Available on Current Network"));
            /* TODO: Use network name from NMClient:primary-connection:id to
             * match design mockup: "Available on Network1234"
             */
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }
}

static void
setup_header (GtkSignalListItemFactory *factory,
              GtkListHeader            *listheader,
              gpointer                  user_data)
{
    GtkWidget *label = gtk_label_new (NULL);

    gtk_widget_add_css_class (label, "heading");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);

    gtk_list_header_set_child (listheader, label);
}

static void
on_model_changed (AmmoniteNetworkView *self)
{
    AmmoniteViewModel *model = ammonite_list_base_get_model (AMMONITE_LIST_BASE (self));

    if (model != NULL)
    {
        g_autoptr (GtkCustomSorter) sorter = gtk_custom_sorter_new (sort_network_items, self, NULL);
        g_autoptr (GtkCustomSorter) sections_sorter = gtk_custom_sorter_new (sort_network_sections, self, NULL);

        ammonite_view_model_set_sorter (model, GTK_SORTER (sorter));
        ammonite_view_model_set_section_sorter (model, GTK_SORTER (sections_sorter));
    }

    gtk_list_view_set_model (self->view_ui, GTK_SELECTION_MODEL (model));
}

static GtkListView *
create_view_ui (AmmoniteNetworkView *self)
{
    g_autoptr (GtkListItemFactory) factory = gtk_signal_list_item_factory_new ();
    g_autoptr (GtkListItemFactory) header_factory = gtk_signal_list_item_factory_new ();
    GtkListView *list_view;

    g_signal_connect (factory, "setup", G_CALLBACK (setup_cell), self);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_cell), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cell), self);

    list_view = GTK_LIST_VIEW (gtk_list_view_new (NULL, g_steal_pointer (&factory)));

    g_signal_connect (header_factory, "setup", G_CALLBACK (setup_header), self);
    g_signal_connect (header_factory, "bind", G_CALLBACK (bind_header), self);
    gtk_list_view_set_header_factory (list_view, header_factory);

    /* We don't use the built-in child activation feature for clicks because it
     * doesn't fill all our needs nor does it match our expected behavior.
     * Instead, we roll our own event handling and double/single click mode.
     * However, GtkListView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_list_view_set_single_click_activate (list_view, FALSE);
    gtk_list_view_set_enable_rubberband (list_view, FALSE);
    gtk_list_view_set_tab_behavior (list_view, GTK_LIST_TAB_ITEM);

    gtk_accessible_update_property (GTK_ACCESSIBLE (list_view),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL,
                                    _("Content View"),
                                    GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION,
                                    _("View of the current location"),
                                    -1);

    /* While we don't want to use GTK's click activation, we'll let it handle
     * the key activation part (with Enter). */
    g_signal_connect (list_view, "activate", G_CALLBACK (on_list_view_item_activated), self);

    return list_view;
}

static void
ammonite_network_view_class_init (AmmoniteNetworkViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    AmmoniteListBaseClass *list_base_view_class = AMMONITE_LIST_BASE_CLASS (klass);

    object_class->dispose = ammonite_network_view_dispose;
    object_class->finalize = ammonite_network_view_finalize;

    list_base_view_class->get_icon_size = real_get_icon_size;
    list_base_view_class->get_sort_state = real_get_sort_state;
    list_base_view_class->get_view_info = real_get_view_info;
    list_base_view_class->get_zoom_level = real_get_zoom_level;
    list_base_view_class->popup_background_context_menu = real_popup_background_context_menu;
    list_base_view_class->scroll_to = real_scroll_to;
    list_base_view_class->set_enable_rubberband = real_set_enable_rubberband;
    list_base_view_class->set_sort_state = real_set_sort_state;
    list_base_view_class->set_zoom_level = real_set_zoom_level;
}

static void
ammonite_network_view_init (AmmoniteNetworkView *self)
{
    GtkWidget *scrolled_window = ammonite_list_base_get_scrolled_window (AMMONITE_LIST_BASE (self));

    gtk_widget_add_css_class (GTK_WIDGET (self), "ammonite-network-view");

    self->view_ui = create_view_ui (self);
    ammonite_list_base_setup_gestures (AMMONITE_LIST_BASE (self));

    g_signal_connect_swapped (self, "notify::model", G_CALLBACK (on_model_changed), self);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                   GTK_WIDGET (self->view_ui));
    ammonite_list_base_set_zoom_level (AMMONITE_LIST_BASE (self), network_view_info.zoom_level_standard);
}

AmmoniteNetworkView *
ammonite_network_view_new (void)
{
    return g_object_new (AMMONITE_TYPE_NETWORK_VIEW, NULL);
}
