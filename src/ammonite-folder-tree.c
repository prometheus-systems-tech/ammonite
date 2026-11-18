/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ammonite-folder-tree.h"
#include "ammonite-scheme.h"

#include <glib/gi18n.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#include "ammonite-application.h"
#include "ammonite-bookmark.h"
#include "ammonite-bookmark-list.h"
#include "ammonite-dnd.h"
#include "ammonite-file.h"
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
    GListStore *roots;
    GVolumeMonitor *volume_monitor;
    AmmoniteBookmarkList *bookmark_list;
    gulong bookmarks_changed_id;

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

/* Number of ancestors of @file, used to rank how specific a root is. */
static guint
get_file_depth (GFile *file)
{
    g_autoptr (GFile) current = g_object_ref (file);
    guint depth = 0;

    for (;;)
    {
        GFile *parent = g_file_get_parent (current);

        if (parent == NULL)
        {
            break;
        }
        g_object_unref (current);
        current = parent;
        depth++;
    }

    return depth;
}

static gboolean
file_is_in_root (GFile *file,
                 GFile *root)
{
    return g_file_equal (file, root) || g_file_has_prefix (file, root);
}

/* Pick the most specific top-level root containing @target: a mount beats
 * Home, which beats Computer. */
static GFile *
find_root_for_target (AmmoniteFolderTree *self,
                      GFile              *target)
{
    GFile *best = NULL;
    guint best_depth = 0;
    guint n_roots = g_list_model_get_n_items (G_LIST_MODEL (self->roots));

    for (guint i = 0; i < n_roots; i++)
    {
        g_autoptr (GFileInfo) info = g_list_model_get_item (G_LIST_MODEL (self->roots), i);
        GFile *root = info_get_file (info);
        guint depth;

        if (root == NULL || !file_is_in_root (target, root) ||
            g_object_get_data (G_OBJECT (info), "ammonite-bookmark") != NULL)
        {
            continue;
        }
        depth = get_file_depth (root);
        if (best == NULL || depth > best_depth)
        {
            /* The root is kept alive by the store, which outlives this call. */
            best = root;
            best_depth = depth;
        }
    }

    return best;
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

    if (file == NULL || g_object_get_data (G_OBJECT (info), "ammonite-no-children") != NULL)
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
show_eject_error (GtkWidget  *parent,
                  const char *mount_name,
                  GError     *error)
{
    AdwDialog *dialog;
    g_autofree char *heading = g_strdup_printf (_("Unable to remove “%s”"), mount_name);

    dialog = adw_alert_dialog_new (heading, error->message);
    adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("_Close"));
    adw_dialog_present (dialog, parent);
}

static void
on_eject_finished (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    GMount *mount = G_MOUNT (source);
    GtkWidget *parent = GTK_WIDGET (user_data);
    g_autoptr (GError) error = NULL;
    gboolean ok;

    if (g_mount_can_eject (mount))
    {
        ok = g_mount_eject_with_operation_finish (mount, result, &error);
    }
    else
    {
        ok = g_mount_unmount_with_operation_finish (mount, result, &error);
    }

    if (!ok && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
    {
        g_autofree char *name = g_mount_get_name (mount);

        show_eject_error (parent, name, error);
    }
    g_object_unref (parent);
}

static void
on_eject_clicked (GtkButton *button,
                  gpointer   user_data)
{
    GMount *mount = G_MOUNT (g_object_get_data (G_OBJECT (button), "mount"));
    GtkWidget *popover = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER);
    GtkWidget *parent = gtk_widget_get_parent (popover);
    GtkRoot *root = gtk_widget_get_root (parent);
    g_autoptr (GMountOperation) operation = gtk_mount_operation_new (GTK_WINDOW (root));

    gtk_popover_popdown (GTK_POPOVER (popover));
    if (g_mount_can_eject (mount))
    {
        g_mount_eject_with_operation (mount, G_MOUNT_UNMOUNT_NONE, operation, NULL,
                                      on_eject_finished, g_object_ref (root));
    }
    else
    {
        g_mount_unmount_with_operation (mount, G_MOUNT_UNMOUNT_NONE, operation, NULL,
                                        on_eject_finished, g_object_ref (root));
    }
}

static void
on_unbookmark_clicked (GtkButton *button,
                       gpointer   user_data)
{
    GFile *location = G_FILE (g_object_get_data (G_OBJECT (button), "location"));
    GtkWidget *popover = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER);
    AmmoniteBookmarkList *bookmarks = ammonite_application_get_bookmarks (AMMONITE_APPLICATION (g_application_get_default ()));

    gtk_popover_popdown (GTK_POPOVER (popover));
    ammonite_bookmark_list_remove (bookmarks, location);
}

static void
on_row_context_click (GtkGestureClick *gesture,
                      int              n_press,
                      double           x,
                      double           y,
                      gpointer         user_data)
{
    GtkTreeExpander *expander = GTK_TREE_EXPANDER (user_data);
    GtkTreeListRow *row = gtk_tree_expander_get_list_row (expander);
    g_autoptr (GFileInfo) info = NULL;
    GMount *mount;
    GtkWidget *popover;
    GtkWidget *button;
    GdkRectangle rect = { (int) x, (int) y, 1, 1 };

    if (row == NULL || gtk_tree_list_row_get_depth (row) != 0)
    {
        return;
    }

    info = gtk_tree_list_row_get_item (row);
    mount = g_object_get_data (G_OBJECT (info), "ammonite-mount");
    if (g_object_get_data (G_OBJECT (info), "ammonite-bookmark") != NULL)
    {
        button = gtk_button_new_with_label (_("Unbookmark"));
        gtk_widget_add_css_class (button, "flat");
        g_object_set_data_full (G_OBJECT (button), "location",
                                g_object_ref (info_get_file (info)), g_object_unref);
        g_signal_connect (button, "clicked", G_CALLBACK (on_unbookmark_clicked), NULL);
    }
    else if (mount == NULL || !(g_mount_can_eject (mount) || g_mount_can_unmount (mount)))
    {
        /* Only offer the action if the user is actually allowed to do it. */
        return;
    }
    else
    {
        button = gtk_button_new_with_label (g_mount_can_eject (mount) ? _("Eject") : _("Unmount"));
        gtk_widget_add_css_class (button, "flat");
        g_object_set_data_full (G_OBJECT (button), "mount", g_object_ref (mount), g_object_unref);
        g_signal_connect (button, "clicked", G_CALLBACK (on_eject_clicked), NULL);
    }

    popover = gtk_popover_new ();
    gtk_popover_set_child (GTK_POPOVER (popover), button);
    gtk_popover_set_has_arrow (GTK_POPOVER (popover), FALSE);
    gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
    gtk_widget_set_parent (popover, GTK_WIDGET (expander));
    g_signal_connect (popover, "closed", G_CALLBACK (gtk_widget_unparent), NULL);
    gtk_popover_popup (GTK_POPOVER (popover));

    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

/* Returns the directory a tree row represents, or NULL. */
static GFile *
get_row_file (GtkTreeExpander *expander)
{
    GtkTreeListRow *row = gtk_tree_expander_get_list_row (expander);
    g_autoptr (GFileInfo) info = NULL;

    if (row == NULL)
    {
        return NULL;
    }
    info = gtk_tree_list_row_get_item (row);

    return info != NULL ? g_object_ref (info_get_file (info)) : NULL;
}

static GdkDragAction
on_row_drop_motion (GtkDropTarget *target,
                    gdouble        x,
                    gdouble        y,
                    gpointer       user_data)
{
    AmmoniteFolderTree *self = user_data;
    GtkTreeExpander *expander = GTK_TREE_EXPANDER (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (target)));
    g_autoptr (GFile) location = get_row_file (expander);
    g_autoptr (AmmoniteFile) target_file = NULL;
    const GValue *value = gtk_drop_target_get_value (target);
    GSList *items;

    if (location == NULL || self->window_slot == NULL ||
        ammonite_window_slot_get_mode (self->window_slot) != AMMONITE_MODE_BROWSE)
    {
        gtk_drop_target_reject (target);
        return 0;
    }

    if (value == NULL || !G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        return 0;
    }

    items = g_value_get_boxed (value);
    target_file = ammonite_file_get (location);

    return ammonite_dnd_get_preferred_action (target_file, items != NULL ? items->data : NULL);
}

static gboolean
on_row_drop (GtkDropTarget *target,
             const GValue  *value,
             gdouble        x,
             gdouble        y,
             gpointer       user_data)
{
    AmmoniteFolderTree *self = user_data;
    GtkTreeExpander *expander = GTK_TREE_EXPANDER (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (target)));
    g_autoptr (GFile) location = get_row_file (expander);
    AmmoniteFilesView *view;
    GdkDragAction action;

    if (location == NULL || self->window_slot == NULL ||
        ammonite_window_slot_get_mode (self->window_slot) != AMMONITE_MODE_BROWSE)
    {
        return FALSE;
    }

    view = ammonite_window_slot_get_current_view (self->window_slot);
    if (view == NULL)
    {
        return FALSE;
    }

    action = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));

    #ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (expander))))
    {
        /* Same workaround as the path bar: the preferred action isn't
         * reported correctly on X11 (GTK MR 4982). */
        GdkDrag *drag = gdk_drop_get_drag (gtk_drop_target_get_current_drop (target));
        action = gdk_drag_get_selected_action (drag);
    }
    #endif

    return ammonite_dnd_perform_drop (view, value, action, location);
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
    {
        GtkGesture *context_click = gtk_gesture_click_new ();

        gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (context_click), GDK_BUTTON_SECONDARY);
        g_signal_connect (context_click, "pressed", G_CALLBACK (on_row_context_click), expander);
        gtk_widget_add_controller (expander, GTK_EVENT_CONTROLLER (context_click));
    }
    {
        GtkDropTarget *drop_target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_ALL);

        gtk_drop_target_set_preload (drop_target, TRUE);
        gtk_drop_target_set_gtypes (drop_target, (GType[1]) { GDK_TYPE_FILE_LIST }, 1);
        g_signal_connect (drop_target, "enter", G_CALLBACK (on_row_drop_motion), user_data);
        g_signal_connect (drop_target, "motion", G_CALLBACK (on_row_drop_motion), user_data);
        g_signal_connect (drop_target, "drop", G_CALLBACK (on_row_drop), user_data);
        gtk_widget_add_controller (expander, GTK_EVENT_CONTROLLER (drop_target));
    }
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

/* Whether the top-level ancestor of @row is a bookmark entry. */
static gboolean
row_is_under_bookmark (GtkTreeListRow *row)
{
    g_autoptr (GtkTreeListRow) current = g_object_ref (row);
    g_autoptr (GFileInfo) info = NULL;

    while (gtk_tree_list_row_get_depth (current) > 0)
    {
        GtkTreeListRow *parent = gtk_tree_list_row_get_parent (current);

        g_object_unref (current);
        current = parent;
    }
    info = gtk_tree_list_row_get_item (current);

    return g_object_get_data (G_OBJECT (info), "ammonite-bookmark") != NULL;
}

/* Return the file of the top-level (depth 0) ancestor of @row, or of @row itself. */
static GFile *
get_root_file (GtkTreeListRow *row)
{
    g_autoptr (GtkTreeListRow) current = g_object_ref (row);
    g_autoptr (GFileInfo) info = NULL;
    GFile *file;

    while (gtk_tree_list_row_get_depth (current) > 0)
    {
        GtkTreeListRow *parent = gtk_tree_list_row_get_parent (current);

        g_object_unref (current);
        current = parent;
    }

    info = gtk_tree_list_row_get_item (current);
    file = info_get_file (info);

    return file != NULL ? g_object_ref (file) : NULL;
}

/* Find the deepest row whose file is @target or one of its ancestors, searching
 * only the most specific root (a mount, else Home, else Computer). */
static void
reveal_pending_location (AmmoniteFolderTree *self)
{
    GFile *target = self->pending_location;
    GFile *target_root;
    guint n_items;
    guint best_pos = G_MAXUINT;
    guint best_depth = 0;

    if (target == NULL)
    {
        return;
    }

    target_root = find_root_for_target (self, target);

    n_items = g_list_model_get_n_items (G_LIST_MODEL (self->tree_model));
    for (guint i = 0; i < n_items; i++)
    {
        g_autoptr (GtkTreeListRow) row = gtk_tree_list_model_get_row (self->tree_model, i);
        g_autoptr (GFileInfo) info = gtk_tree_list_row_get_item (row);
        g_autoptr (GFile) root = get_root_file (row);
        GFile *file = info_get_file (info);
        guint depth = gtk_tree_list_row_get_depth (row);

        if (file == NULL || root == NULL || row_is_under_bookmark (row))
        {
            continue;
        }
        if (target_root == NULL || !g_file_equal (root, target_root))
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
on_slot_location_changed (AmmoniteFolderTree *self);

static int
compare_bookmarks (gconstpointer a,
                   gconstpointer b)
{
    g_autofree char *key_a = g_utf8_collate_key_for_filename (ammonite_bookmark_get_name ((AmmoniteBookmark *) a), -1);
    g_autofree char *key_b = g_utf8_collate_key_for_filename (ammonite_bookmark_get_name ((AmmoniteBookmark *) b), -1);

    return strcmp (key_a, key_b);
}

/* Rebuild the bookmark entries, which sit directly below Network. */
static void
rebuild_bookmark_roots (AmmoniteFolderTree *self)
{
    guint n_roots = g_list_model_get_n_items (G_LIST_MODEL (self->roots));
    guint insert_at = 0;
    g_autoptr (GList) bookmarks = NULL;
    guint i;

    /* Drop the existing bookmark entries, remembering where Network is. */
    for (i = 0; i < n_roots;)
    {
        g_autoptr (GFileInfo) info = g_list_model_get_item (G_LIST_MODEL (self->roots), i);

        if (g_object_get_data (G_OBJECT (info), "ammonite-bookmark") != NULL)
        {
            g_list_store_remove (self->roots, i);
            n_roots--;
            continue;
        }
        if (g_object_get_data (G_OBJECT (info), "ammonite-network") != NULL)
        {
            insert_at = i + 1;
        }
        i++;
    }

    /* Always alphabetical, whatever order the bookmarks file has. */
    bookmarks = g_list_sort (g_list_copy (ammonite_bookmark_list_get_all (self->bookmark_list)),
                             compare_bookmarks);
    for (GList *l = bookmarks; l != NULL; l = l->next)
    {
        AmmoniteBookmark *bookmark = l->data;
        GFile *location = ammonite_bookmark_get_location (bookmark);
        GIcon *icon = ammonite_bookmark_get_symbolic_icon (bookmark);
        g_autoptr (GFileInfo) info = g_file_info_new ();

        g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
        g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (location));
        g_file_info_set_display_name (info, ammonite_bookmark_get_name (bookmark));
        if (icon != NULL)
        {
            g_file_info_set_symbolic_icon (info, icon);
        }
        g_object_set_data (G_OBJECT (info), "ammonite-no-children", GINT_TO_POINTER (1));
        g_object_set_data (G_OBJECT (info), "ammonite-bookmark", GINT_TO_POINTER (1));
        g_list_store_insert (self->roots, insert_at++, info);
    }

    on_slot_location_changed (self);
}

static void
on_bookmarks_changed (AmmoniteBookmarkList *list,
                      AmmoniteFolderTree   *self)
{
    rebuild_bookmark_roots (self);
}

static gboolean
mount_is_listed (GMount *mount)
{
    return !g_mount_is_shadowed (mount);
}

static void
add_mount_root (AmmoniteFolderTree *self,
                GMount             *mount)
{
    g_autoptr (GFile) root = g_mount_get_root (mount);
    g_autofree char *name = g_mount_get_name (mount);
    g_autoptr (GIcon) icon = g_mount_get_symbolic_icon (mount);
    g_autoptr (GFileInfo) info = NULL;

    if (!mount_is_listed (mount))
    {
        return;
    }

    info = create_root_info (root, name);
    if (icon != NULL)
    {
        g_file_info_set_symbolic_icon (info, icon);
    }
    g_object_set_data_full (G_OBJECT (info), "ammonite-mount", g_object_ref (mount), g_object_unref);
    g_list_store_append (self->roots, info);
}

static void
remove_mount_root (AmmoniteFolderTree *self,
                   GMount             *mount)
{
    guint n_roots = g_list_model_get_n_items (G_LIST_MODEL (self->roots));

    for (guint i = 0; i < n_roots; i++)
    {
        g_autoptr (GFileInfo) info = g_list_model_get_item (G_LIST_MODEL (self->roots), i);

        if (g_object_get_data (G_OBJECT (info), "ammonite-mount") == mount)
        {
            g_list_store_remove (self->roots, i);
            return;
        }
    }
}

static void
on_mount_added (GVolumeMonitor     *monitor,
                GMount             *mount,
                AmmoniteFolderTree *self)
{
    add_mount_root (self, mount);
    on_slot_location_changed (self);
}

static void
on_mount_removed (GVolumeMonitor     *monitor,
                  GMount             *mount,
                  AmmoniteFolderTree *self)
{
    remove_mount_root (self, mount);
    on_slot_location_changed (self);
}

static void
on_mount_changed (GVolumeMonitor     *monitor,
                  GMount             *mount,
                  AmmoniteFolderTree *self)
{
    /* Shadowing or the name may have changed: rebuild this entry. */
    remove_mount_root (self, mount);
    add_mount_root (self, mount);
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
    if (self->volume_monitor != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->volume_monitor, self);
        g_clear_object (&self->volume_monitor);
    }
    if (self->bookmark_list != NULL)
    {
        g_clear_signal_handler (&self->bookmarks_changed_id, self->bookmark_list);
        self->bookmark_list = NULL;
    }
    g_clear_object (&self->roots);
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
    g_autoptr (GFile) home = g_file_new_for_path (g_get_home_dir ());
    g_autoptr (GFile) filesystem_root = g_file_new_for_path ("/");
    g_autoptr (GFileInfo) home_info = create_root_info (home, _("Home"));
    g_autoptr (GFileInfo) root_info = create_root_info (filesystem_root, _("Computer"));
    GtkListItemFactory *factory;
    GtkWidget *scrolled;
    GList *mounts;

    self->roots = g_list_store_new (G_TYPE_FILE_INFO);
    g_list_store_append (self->roots, home_info);
    g_list_store_append (self->roots, root_info);

    /* Network: opens the network view, where servers can be browsed and
     * connected to. It is a leaf; mounted shares get their own roots below. */
    {
        g_autoptr (GFile) network = g_file_new_for_uri (SCHEME_NETWORK_VIEW ":///");
        g_autoptr (GFileInfo) network_info = g_file_info_new ();
        g_autoptr (GIcon) network_icon = g_themed_icon_new ("network-workgroup-symbolic");

        g_file_info_set_file_type (network_info, G_FILE_TYPE_DIRECTORY);
        g_file_info_set_attribute_object (network_info, "standard::file", G_OBJECT (network));
        g_file_info_set_display_name (network_info, _("Network"));
        g_file_info_set_symbolic_icon (network_info, network_icon);
        g_object_set_data (G_OBJECT (network_info), "ammonite-no-children", GINT_TO_POINTER (1));
        g_object_set_data (G_OBJECT (network_info), "ammonite-network", GINT_TO_POINTER (1));
        g_list_store_append (self->roots, network_info);
    }

    self->bookmark_list = ammonite_application_get_bookmarks (AMMONITE_APPLICATION (g_application_get_default ()));
    self->bookmarks_changed_id = g_signal_connect (self->bookmark_list, "changed",
                                                   G_CALLBACK (on_bookmarks_changed), self);
    rebuild_bookmark_roots (self);

    self->volume_monitor = g_volume_monitor_get ();
    mounts = g_volume_monitor_get_mounts (self->volume_monitor);
    for (GList *l = mounts; l != NULL; l = l->next)
    {
        add_mount_root (self, G_MOUNT (l->data));
    }
    g_list_free_full (mounts, g_object_unref);
    g_signal_connect (self->volume_monitor, "mount-added", G_CALLBACK (on_mount_added), self);
    g_signal_connect (self->volume_monitor, "mount-removed", G_CALLBACK (on_mount_removed), self);
    g_signal_connect (self->volume_monitor, "mount-changed", G_CALLBACK (on_mount_changed), self);

    self->tree_model = gtk_tree_list_model_new (g_object_ref (G_LIST_MODEL (self->roots)),
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
