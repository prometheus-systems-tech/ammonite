/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ammonite-folder-tree.h"

#include <glib/gi18n.h>

#include "ammonite-window-slot.h"

#define TREE_ATTRIBUTES "standard::file,standard::name,standard::display-name,standard::type," \
        "standard::is-hidden,standard::symbolic-icon"

struct _AmmoniteFolderTree
{
    AdwBin parent_instance;

    AmmoniteWindowSlot *window_slot;
    GtkListView *list_view;
    GtkTreeListModel *tree_model;
    GtkSingleSelection *selection;

    /* Location to reveal once the rows leading to it have loaded */
    GFile *pending_location;
};

G_DEFINE_FINAL_TYPE (AmmoniteFolderTree, ammonite_folder_tree, ADW_TYPE_BIN)

enum
{
    PROP_0,
    PROP_WINDOW_SLOT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static GFile *
info_get_file (GFileInfo *info)
{
    return G_FILE (g_file_info_get_attribute_object (info, "standard::file"));
}

static GFileInfo *
create_root_info (GFile      *file,
                  const char *display_name)
{
    GFileInfo *info = g_file_query_info (file, TREE_ATTRIBUTES,
                                         G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (info == NULL)
    {
        info = g_file_info_new ();
        g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
    }
    g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));
    if (display_name != NULL)
    {
        g_file_info_set_display_name (info, display_name);
    }

    return info;
}

static gboolean
filter_directories (gpointer item,
                    gpointer user_data)
{
    GFileInfo *info = G_FILE_INFO (item);

    return g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY &&
           !g_file_info_get_is_hidden (info);
}

static int
compare_names (gconstpointer a,
               gconstpointer b,
               gpointer      user_data)
{
    g_autofree char *key_a = g_utf8_collate_key_for_filename (g_file_info_get_display_name ((GFileInfo *) a), -1);
    g_autofree char *key_b = g_utf8_collate_key_for_filename (g_file_info_get_display_name ((GFileInfo *) b), -1);

    return strcmp (key_a, key_b);
}

static GListModel *
create_children_model (gpointer item,
                       gpointer user_data)
{
    GFileInfo *info = G_FILE_INFO (item);
    GFile *file = info_get_file (info);
    GtkDirectoryList *dir_list;
    GtkFilterListModel *filtered;
    GtkSortListModel *sorted;

    if (file == NULL)
    {
        return NULL;
    }

    dir_list = gtk_directory_list_new (TREE_ATTRIBUTES, file);
    gtk_directory_list_set_io_priority (dir_list, G_PRIORITY_LOW);
    filtered = gtk_filter_list_model_new (G_LIST_MODEL (dir_list),
                                          GTK_FILTER (gtk_custom_filter_new (filter_directories, NULL, NULL)));
    sorted = gtk_sort_list_model_new (G_LIST_MODEL (filtered),
                                      GTK_SORTER (gtk_custom_sorter_new (compare_names, NULL, NULL)));

    return G_LIST_MODEL (sorted);
}

static void
on_row_double_click (GtkGestureClick *gesture,
                     int              n_press,
                     double           x,
                     double           y,
                     gpointer         user_data)
{
    GtkTreeExpander *expander = GTK_TREE_EXPANDER (user_data);
    GtkTreeListRow *row;

    if (n_press != 2)
    {
        return;
    }

    row = gtk_tree_expander_get_list_row (expander);
    if (row != NULL && gtk_tree_list_row_is_expandable (row))
    {
        gtk_tree_list_row_set_expanded (row, !gtk_tree_list_row_get_expanded (row));
    }
}

static void
factory_setup (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item,
               gpointer                  user_data)
{
    GtkWidget *expander = gtk_tree_expander_new ();
    GtkGesture *click = gtk_gesture_click_new ();

    g_signal_connect (click, "pressed", G_CALLBACK (on_row_double_click), expander);
    gtk_widget_add_controller (expander, GTK_EVENT_CONTROLLER (click));
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *image = gtk_image_new ();
    GtkWidget *label = gtk_label_new (NULL);

    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_box_append (GTK_BOX (box), image);
    gtk_box_append (GTK_BOX (box), label);
    gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), box);
    gtk_list_item_set_child (list_item, expander);
}

static void
factory_bind (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item,
              gpointer                  user_data)
{
    GtkTreeExpander *expander = GTK_TREE_EXPANDER (gtk_list_item_get_child (list_item));
    GtkTreeListRow *row = GTK_TREE_LIST_ROW (gtk_list_item_get_item (list_item));
    GtkWidget *box = gtk_tree_expander_get_child (expander);
    GtkWidget *image = gtk_widget_get_first_child (box);
    GtkWidget *label = gtk_widget_get_last_child (box);
    g_autoptr (GFileInfo) info = gtk_tree_list_row_get_item (row);
    GIcon *icon = g_file_info_get_symbolic_icon (info);

    gtk_tree_expander_set_list_row (expander, row);
    gtk_image_set_from_gicon (GTK_IMAGE (image), icon != NULL ? icon : g_themed_icon_new ("folder-symbolic"));
    gtk_label_set_text (GTK_LABEL (label), g_file_info_get_display_name (info));
}

static void
factory_unbind (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item,
                gpointer                  user_data)
{
    GtkTreeExpander *expander = GTK_TREE_EXPANDER (gtk_list_item_get_child (list_item));

    gtk_tree_expander_set_list_row (expander, NULL);
}

static void
on_activate (GtkListView *list_view,
             guint        position,
             gpointer     user_data)
{
    AmmoniteFolderTree *self = AMMONITE_FOLDER_TREE (user_data);
    g_autoptr (GtkTreeListRow) row = gtk_tree_list_model_get_row (self->tree_model, position);
    g_autoptr (GFileInfo) info = NULL;
    GFile *file;

    if (row == NULL || self->window_slot == NULL)
    {
        return;
    }

    info = gtk_tree_list_row_get_item (row);
    file = info_get_file (info);
    if (file != NULL)
    {
        ammonite_window_slot_open_location_full (self->window_slot, file, NULL);
    }
}

/* Find the deepest row whose file is @target or one of its ancestors. */
static void
reveal_pending_location (AmmoniteFolderTree *self)
{
    GFile *target = self->pending_location;
    guint n_items;
    guint best_pos = G_MAXUINT;
    guint best_depth = 0;

    if (target == NULL)
    {
        return;
    }

    n_items = g_list_model_get_n_items (G_LIST_MODEL (self->tree_model));
    for (guint i = 0; i < n_items; i++)
    {
        g_autoptr (GtkTreeListRow) row = gtk_tree_list_model_get_row (self->tree_model, i);
        g_autoptr (GFileInfo) info = gtk_tree_list_row_get_item (row);
        GFile *file = info_get_file (info);
        guint depth = gtk_tree_list_row_get_depth (row);

        if (file == NULL)
        {
            continue;
        }
        if (g_file_equal (file, target))
        {
            best_pos = i;
            best_depth = depth;
            break;
        }
        if (g_file_has_prefix (target, file) && (best_pos == G_MAXUINT || depth >= best_depth))
        {
            best_pos = i;
            best_depth = depth;
        }
    }

    if (best_pos == G_MAXUINT)
    {
        /* Not under any root. */
        gtk_single_selection_set_selected (self->selection, GTK_INVALID_LIST_POSITION);
        g_clear_object (&self->pending_location);
        return;
    }

    {
        g_autoptr (GtkTreeListRow) row = gtk_tree_list_model_get_row (self->tree_model, best_pos);
        g_autoptr (GFileInfo) info = gtk_tree_list_row_get_item (row);

        if (g_file_equal (info_get_file (info), target))
        {
            gtk_single_selection_set_selected (self->selection, best_pos);
            gtk_list_view_scroll_to (self->list_view, best_pos, GTK_LIST_SCROLL_NONE, NULL);
            g_clear_object (&self->pending_location);
        }
        else if (!gtk_tree_list_row_get_expanded (row))
        {
            /* Expanding changes the model, which calls us again once loaded. */
            gtk_tree_list_row_set_expanded (row, TRUE);
        }
        /* else: expanded but child not present (yet, or hidden): keep waiting. */
    }
}

static void
on_tree_items_changed (GListModel *model,
                       guint       position,
                       guint       removed,
                       guint       added,
                       gpointer    user_data)
{
    AmmoniteFolderTree *self = AMMONITE_FOLDER_TREE (user_data);

    if (self->pending_location != NULL)
    {
        reveal_pending_location (self);
    }
}

static void
on_slot_location_changed (AmmoniteFolderTree *self)
{
    GFile *location = self->window_slot != NULL
                      ? ammonite_window_slot_get_location (self->window_slot)
                      : NULL;

    g_clear_object (&self->pending_location);
    if (location != NULL)
    {
        self->pending_location = g_object_ref (location);
        reveal_pending_location (self);
    }
}

static void
ammonite_folder_tree_set_window_slot (AmmoniteFolderTree *self,
                                      AmmoniteWindowSlot *slot)
{
    if (self->window_slot == slot)
    {
        return;
    }

    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_func (self->window_slot, on_slot_location_changed, self);
        g_clear_weak_pointer (&self->window_slot);
    }
    g_set_weak_pointer (&self->window_slot, slot);
    if (slot != NULL)
    {
        g_signal_connect_swapped (slot, "notify::location",
                                  G_CALLBACK (on_slot_location_changed), self);
        on_slot_location_changed (self);
    }
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WINDOW_SLOT]);
}

static void
ammonite_folder_tree_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
    AmmoniteFolderTree *self = AMMONITE_FOLDER_TREE (object);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            g_value_set_object (value, self->window_slot);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_folder_tree_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
    AmmoniteFolderTree *self = AMMONITE_FOLDER_TREE (object);

    switch (prop_id)
    {
        case PROP_WINDOW_SLOT:
        {
            ammonite_folder_tree_set_window_slot (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
ammonite_folder_tree_dispose (GObject *object)
{
    AmmoniteFolderTree *self = AMMONITE_FOLDER_TREE (object);

    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_func (self->window_slot, on_slot_location_changed, self);
        g_clear_weak_pointer (&self->window_slot);
    }
    g_clear_object (&self->pending_location);
    adw_bin_set_child (ADW_BIN (self), NULL);

    G_OBJECT_CLASS (ammonite_folder_tree_parent_class)->dispose (object);
}

static void
ammonite_folder_tree_class_init (AmmoniteFolderTreeClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = ammonite_folder_tree_dispose;
    object_class->get_property = ammonite_folder_tree_get_property;
    object_class->set_property = ammonite_folder_tree_set_property;

    properties[PROP_WINDOW_SLOT] =
        g_param_spec_object ("window-slot", NULL, NULL,
                             AMMONITE_TYPE_WINDOW_SLOT,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ammonite_folder_tree_init (AmmoniteFolderTree *self)
{
    g_autoptr (GListStore) roots = g_list_store_new (G_TYPE_FILE_INFO);
    g_autoptr (GFile) home = g_file_new_for_path (g_get_home_dir ());
    g_autoptr (GFile) filesystem_root = g_file_new_for_path ("/");
    g_autoptr (GFileInfo) home_info = create_root_info (home, _("Home"));
    g_autoptr (GFileInfo) root_info = create_root_info (filesystem_root, _("Computer"));
    GtkListItemFactory *factory;
    GtkWidget *scrolled;

    g_list_store_append (roots, home_info);
    g_list_store_append (roots, root_info);

    self->tree_model = gtk_tree_list_model_new (g_object_ref (G_LIST_MODEL (roots)),
                                                FALSE, FALSE,
                                                create_children_model, NULL, NULL);
    self->selection = gtk_single_selection_new (g_object_ref (G_LIST_MODEL (self->tree_model)));
    gtk_single_selection_set_autoselect (self->selection, FALSE);
    gtk_single_selection_set_can_unselect (self->selection, TRUE);
    gtk_single_selection_set_selected (self->selection, GTK_INVALID_LIST_POSITION);

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (factory_setup), self);
    g_signal_connect (factory, "bind", G_CALLBACK (factory_bind), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (factory_unbind), self);

    self->list_view = GTK_LIST_VIEW (gtk_list_view_new (GTK_SELECTION_MODEL (self->selection), factory));
    gtk_list_view_set_single_click_activate (self->list_view, TRUE);
    gtk_widget_add_css_class (GTK_WIDGET (self->list_view), "navigation-sidebar");
    g_signal_connect (self->list_view, "activate", G_CALLBACK (on_activate), self);
    g_signal_connect (self->tree_model, "items-changed", G_CALLBACK (on_tree_items_changed), self);

    scrolled = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), GTK_WIDGET (self->list_view));
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    adw_bin_set_child (ADW_BIN (self), scrolled);
}

GtkWidget *
ammonite_folder_tree_new (void)
{
    return g_object_new (AMMONITE_TYPE_FOLDER_TREE, NULL);
}
