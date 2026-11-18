/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * The "compact layout": items flow top to bottom, then left to right, one line
 * each, and every column is only as wide as its own widest name ("not all
 * columns equal"). GtkGridView can't do that, so this view lays out its own
 * cells. Only the cells in visible columns exist at any time.
 */

#include "ammonite-compact-view.h"

#include "ammonite-compact-cell.h"
#include "ammonite-enums.h"
#include "ammonite-file.h"
#include "ammonite-global-preferences.h"
#include "ammonite-list-base-private.h"
#include "ammonite-view-cell.h"
#include "ammonite-view-item.h"
#include "ammonite-view-model.h"

#include <glib/gi18n.h>

#define CELL_PADDING 8
#define CELL_SPACING 6
#define ROW_PADDING 4
#define MAX_NAME_CHARS 40
#define VIEW_MARGIN 12

/* ------------------------------------------------------------------------ */
/* Canvas: the widget that positions the cells                              */
/* ------------------------------------------------------------------------ */

#define AMMONITE_TYPE_COMPACT_CANVAS (ammonite_compact_canvas_get_type())
G_DECLARE_FINAL_TYPE (AmmoniteCompactCanvas, ammonite_compact_canvas, AMMONITE, COMPACT_CANVAS, GtkWidget)

struct _AmmoniteCompactCanvas
{
    GtkWidget parent_instance;

    AmmoniteListBase *list_base; /* unowned */
    GtkAdjustment *hadjustment;  /* unowned, owned by the scrolled window */
    AmmoniteViewModel *model;    /* owned */
    gulong items_changed_id;
    gulong selection_changed_id;

    guint icon_size;
    gboolean layout_dirty;
    int row_height;
    guint rows;
    int allocated_height;

    /* Per column layout. */
    int *col_x;
    int *col_w;
    guint n_cols;
    int total_width;

    GHashTable *cells; /* position -> cell, owned */

    guint cursor;
    guint anchor;

    /* Rubberband */
    gboolean rubberband_enabled;
    gboolean rubberbanding;
    graphene_point_t rb_start;
    graphene_point_t rb_end;
    GtkBitset *rb_initial;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (AmmoniteCompactCanvas, ammonite_compact_canvas, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

enum
{
    CANVAS_PROP_0,
    CANVAS_PROP_HADJUSTMENT,
    CANVAS_PROP_VADJUSTMENT,
    CANVAS_PROP_HSCROLL_POLICY,
    CANVAS_PROP_VSCROLL_POLICY,
};

static guint
canvas_n_items (AmmoniteCompactCanvas *self)
{
    return self->model != NULL ? g_list_model_get_n_items (G_LIST_MODEL (self->model)) : 0;
}

static AmmoniteViewItem *
canvas_get_view_item (AmmoniteCompactCanvas *self,
                      guint                  position)
{
    g_autoptr (GtkTreeListRow) row = g_list_model_get_item (G_LIST_MODEL (self->model), position);

    g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (row), NULL);
    return AMMONITE_VIEW_ITEM (gtk_tree_list_row_get_item (row));
}

static void
canvas_clear_cell (AmmoniteCompactCanvas *self,
                   guint                  position,
                   GtkWidget             *cell)
{
    g_autoptr (AmmoniteViewItem) item = ammonite_view_cell_get_item (AMMONITE_VIEW_CELL (cell));

    if (item != NULL)
    {
        ammonite_view_item_set_item_ui (item, NULL);
    }
    gtk_widget_unparent (cell);
}

static void
canvas_remove_all_cells (AmmoniteCompactCanvas *self)
{
    GHashTableIter iter;
    gpointer key;
    gpointer value;

    g_hash_table_iter_init (&iter, self->cells);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        canvas_clear_cell (self, GPOINTER_TO_UINT (key), value);
    }
    g_hash_table_remove_all (self->cells);
}

/* Measures every name and works out the column widths. */
static void
canvas_update_layout (AmmoniteCompactCanvas *self,
                      int                    height)
{
    guint n = canvas_n_items (self);
    PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);
    int text_height;
    int cap_width;
    int icon_row;
    guint rows;

    g_object_get (self->list_base, "icon-size", &self->icon_size, NULL);

    pango_layout_set_text (layout, "0", 1);
    pango_layout_get_pixel_size (layout, &cap_width, &text_height);
    cap_width *= MAX_NAME_CHARS;

    icon_row = MAX ((int) self->icon_size, text_height);
    self->row_height = icon_row + ROW_PADDING * 2;

    rows = MAX (1, (height - VIEW_MARGIN * 2) / self->row_height);
    self->rows = rows;
    self->n_cols = n == 0 ? 0 : (n + rows - 1) / rows;

    g_free (self->col_x);
    g_free (self->col_w);
    self->col_x = g_new0 (int, self->n_cols + 1);
    self->col_w = g_new0 (int, self->n_cols + 1);

    int x = VIEW_MARGIN;

    for (guint c = 0; c < self->n_cols; c++)
    {
        int widest = 0;

        for (guint i = c * rows; i < MIN (n, (c + 1) * rows); i++)
        {
            g_autoptr (AmmoniteViewItem) item = canvas_get_view_item (self, i);
            g_autofree char *name = NULL;
            int w;
            int h;

            if (item == NULL)
            {
                continue;
            }
            g_object_get (ammonite_view_item_get_file (item), "display-name", &name, NULL);
            pango_layout_set_text (layout, name != NULL ? name : "", -1);
            pango_layout_get_pixel_size (layout, &w, &h);
            widest = MAX (widest, MIN (w, cap_width));
        }

        self->col_x[c] = x;
        self->col_w[c] = widest + self->icon_size + CELL_SPACING + CELL_PADDING * 2 + 4;
        x += self->col_w[c];
    }

    self->total_width = x + VIEW_MARGIN;
    self->allocated_height = height;
    self->layout_dirty = FALSE;
    g_object_unref (layout);
}

static gboolean
canvas_position_geometry (AmmoniteCompactCanvas *self,
                          guint                  position,
                          graphene_rect_t       *rect)
{
    guint col = position / self->rows;
    guint row = position % self->rows;

    if (col >= self->n_cols)
    {
        return FALSE;
    }
    graphene_rect_init (rect,
                        self->col_x[col],
                        VIEW_MARGIN + row * self->row_height,
                        self->col_w[col],
                        self->row_height);
    return TRUE;
}

/* Returns G_MAXUINT if there's no item at the point (in canvas coordinates,
 * horizontally unscrolled). */
static guint
canvas_item_at (AmmoniteCompactCanvas *self,
                double                 x,
                double                 y)
{
    guint n = canvas_n_items (self);

    if (y < VIEW_MARGIN || self->rows == 0)
    {
        return G_MAXUINT;
    }

    guint row = (guint) ((y - VIEW_MARGIN) / self->row_height);

    if (row >= self->rows)
    {
        return G_MAXUINT;
    }

    for (guint c = 0; c < self->n_cols; c++)
    {
        if (x >= self->col_x[c] && x < self->col_x[c] + self->col_w[c])
        {
            guint pos = c * self->rows + row;

            return pos < n ? pos : G_MAXUINT;
        }
    }

    return G_MAXUINT;
}

static void
canvas_update_cell_state (AmmoniteCompactCanvas *self,
                          guint                  position,
                          GtkWidget             *cell)
{
    GtkStateFlags flags = gtk_widget_get_state_flags (cell) & ~GTK_STATE_FLAG_SELECTED;

    if (gtk_selection_model_is_selected (GTK_SELECTION_MODEL (self->model), position))
    {
        flags |= GTK_STATE_FLAG_SELECTED;
    }
    gtk_widget_set_state_flags (cell, flags, TRUE);
    gtk_widget_unset_state_flags (cell, ~flags & GTK_STATE_FLAG_SELECTED);

    if (position == self->cursor && gtk_widget_has_focus (GTK_WIDGET (self)))
    {
        gtk_widget_add_css_class (cell, "cursor");
    }
    else
    {
        gtk_widget_remove_css_class (cell, "cursor");
    }
}

static void
canvas_update_all_cell_states (AmmoniteCompactCanvas *self)
{
    GHashTableIter iter;
    gpointer key;
    gpointer value;

    g_hash_table_iter_init (&iter, self->cells);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        canvas_update_cell_state (self, GPOINTER_TO_UINT (key), value);
    }
}

static GtkWidget *
canvas_create_cell (AmmoniteCompactCanvas *self,
                    guint                  position)
{
    g_autoptr (AmmoniteViewItem) item = canvas_get_view_item (self, position);
    GtkWidget *cell = GTK_WIDGET (ammonite_compact_cell_new (self->list_base));

    g_object_bind_property (self->list_base, "icon-size", cell, "icon-size", G_BINDING_SYNC_CREATE);
    g_object_set (cell, "position", position, "item", item, NULL);
    setup_cell_controllers (AMMONITE_VIEW_CELL (cell), cell);
    ammonite_view_item_set_item_ui (item, cell);
    gtk_widget_set_parent (cell, GTK_WIDGET (self));
    canvas_update_cell_state (self, position, cell);

    return cell;
}

static void
canvas_queue_relayout (AmmoniteCompactCanvas *self)
{
    self->layout_dirty = TRUE;
    canvas_remove_all_cells (self);
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
canvas_measure (GtkWidget      *widget,
                GtkOrientation  orientation,
                int             for_size,
                int            *minimum,
                int            *natural,
                int            *minimum_baseline,
                int            *natural_baseline)
{
    AmmoniteCompactCanvas *self = AMMONITE_COMPACT_CANVAS (widget);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        int height = for_size > 0 ? for_size : MAX (self->allocated_height, 100);

        if (self->layout_dirty || height != self->allocated_height)
        {
            canvas_update_layout (self, height);
        }
        *minimum = *natural = self->total_width;
    }
    else
    {
        *minimum = 100;
        *natural = MAX (self->allocated_height, 100);
    }
}

static void
canvas_size_allocate (GtkWidget *widget,
                      int        width,
                      int        height,
                      int        baseline)
{
    AmmoniteCompactCanvas *self = AMMONITE_COMPACT_CANVAS (widget);
    guint n = canvas_n_items (self);
    double scroll_x = self->hadjustment != NULL ? gtk_adjustment_get_value (self->hadjustment) : 0;
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    GHashTable *wanted = g_hash_table_new (g_direct_hash, g_direct_equal);

    if (self->layout_dirty || height != self->allocated_height)
    {
        canvas_update_layout (self, height);
    }

    /* Which positions are within [scroll_x, scroll_x + width]? */
    for (guint c = 0; c < self->n_cols; c++)
    {
        if (self->col_x[c] + self->col_w[c] < scroll_x ||
            self->col_x[c] > scroll_x + width)
        {
            continue;
        }

        for (guint i = c * self->rows; i < MIN (n, (c + 1) * self->rows); i++)
        {
            g_hash_table_add (wanted, GUINT_TO_POINTER (i));
        }
    }

    /* Drop cells that scrolled out. */
    g_hash_table_iter_init (&iter, self->cells);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        if (!g_hash_table_contains (wanted, key))
        {
            canvas_clear_cell (self, GPOINTER_TO_UINT (key), value);
            g_hash_table_iter_remove (&iter);
        }
    }

    g_hash_table_iter_init (&iter, wanted);
    while (g_hash_table_iter_next (&iter, &key, NULL))
    {
        guint position = GPOINTER_TO_UINT (key);
        GtkWidget *cell = g_hash_table_lookup (self->cells, key);
        graphene_rect_t rect;

        if (cell == NULL)
        {
            cell = canvas_create_cell (self, position);
            g_hash_table_insert (self->cells, key, cell);
        }

        if (canvas_position_geometry (self, position, &rect))
        {
            /* The adjustment scrolls us via the viewport, so cell
             * coordinates are absolute within the canvas. */
            gtk_widget_allocate (cell,
                                 rect.size.width, rect.size.height, -1,
                                 gsk_transform_translate (NULL, &rect.origin));
        }
    }

    g_hash_table_destroy (wanted);
}

static void
canvas_snapshot (GtkWidget   *widget,
                 GtkSnapshot *snapshot)
{
    AmmoniteCompactCanvas *self = AMMONITE_COMPACT_CANVAS (widget);
    GtkWidget *child;

    for (child = gtk_widget_get_first_child (widget); child != NULL; child = gtk_widget_get_next_sibling (child))
    {
        gtk_widget_snapshot_child (widget, child, snapshot);
    }

    if (self->rubberbanding)
    {
        graphene_rect_t rect;
        GdkRGBA color;

        graphene_rect_init (&rect,
                            MIN (self->rb_start.x, self->rb_end.x),
                            MIN (self->rb_start.y, self->rb_end.y),
                            fabsf (self->rb_start.x - self->rb_end.x),
                            fabsf (self->rb_start.y - self->rb_end.y));
        gtk_widget_get_color (widget, &color);
        color.alpha = 0.18f;
        gtk_snapshot_append_color (snapshot, &color, &rect);
        color.alpha = 0.6f;
        GskRoundedRect outline;

        gsk_rounded_rect_init_from_rect (&outline, &rect, 0);
        gtk_snapshot_append_border (snapshot,
                                    &outline,
                                    (float[4]) { 1, 1, 1, 1 },
                                    (GdkRGBA[4]) { color, color, color, color });
    }
}

/* --- Selection and cursor ----------------------------------------------- */

static void
canvas_scroll_to_position (AmmoniteCompactCanvas *self,
                           guint                  position)
{
    graphene_rect_t rect;

    if (self->hadjustment == NULL || !canvas_position_geometry (self, position, &rect))
    {
        return;
    }

    double value = gtk_adjustment_get_value (self->hadjustment);
    double page = gtk_adjustment_get_page_size (self->hadjustment);

    if (rect.origin.x < value)
    {
        gtk_adjustment_set_value (self->hadjustment, rect.origin.x - VIEW_MARGIN);
    }
    else if (rect.origin.x + rect.size.width > value + page)
    {
        gtk_adjustment_set_value (self->hadjustment, rect.origin.x + rect.size.width - page + VIEW_MARGIN);
    }
}

static void
canvas_set_cursor (AmmoniteCompactCanvas *self,
                   guint                  position,
                   gboolean               scroll)
{
    self->cursor = position;
    if (scroll)
    {
        canvas_scroll_to_position (self, position);
    }
    canvas_update_all_cell_states (self);
}

static void
canvas_select_range (AmmoniteCompactCanvas *self,
                     guint                  from,
                     guint                  to,
                     gboolean               extend)
{
    GtkSelectionModel *model = GTK_SELECTION_MODEL (self->model);
    guint lo = MIN (from, to);
    guint hi = MAX (from, to);
    g_autoptr (GtkBitset) set = gtk_bitset_new_empty ();
    g_autoptr (GtkBitset) mask = gtk_bitset_new_empty ();

    gtk_bitset_add_range (set, lo, hi - lo + 1);
    if (extend)
    {
        gtk_bitset_add_range (mask, lo, hi - lo + 1);
        gtk_selection_model_set_selection (model, set, mask);
    }
    else
    {
        gtk_bitset_add_range (mask, 0, MAX (canvas_n_items (self), 1));
        gtk_selection_model_set_selection (model, set, mask);
    }
}

static void
canvas_move (AmmoniteCompactCanvas *self,
             int                    delta,
             GdkModifierType        state,
             gboolean               absolute)
{
    guint n = canvas_n_items (self);
    gint64 target;

    if (n == 0)
    {
        return;
    }

    target = absolute ? delta : (gint64) self->cursor + delta;
    target = CLAMP (target, 0, (gint64) n - 1);

    if (state & GDK_SHIFT_MASK)
    {
        canvas_select_range (self, self->anchor, target, FALSE);
    }
    else if (state & GDK_CONTROL_MASK)
    {
        /* Move the cursor only. */
    }
    else
    {
        self->anchor = target;
        gtk_selection_model_select_item (GTK_SELECTION_MODEL (self->model), target, TRUE);
    }
    canvas_set_cursor (self, target, TRUE);
}

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                gpointer               user_data)
{
    AmmoniteCompactCanvas *self = user_data;
    gboolean rtl = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
    int rows = (int) self->rows;
    guint n = canvas_n_items (self);

    state &= gtk_accelerator_get_default_mod_mask ();

    switch (keyval)
    {
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            canvas_move (self, -1, state, FALSE);
            return TRUE;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            canvas_move (self, 1, state, FALSE);
            return TRUE;

        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
            canvas_move (self, rtl ? rows : -rows, state, FALSE);
            return TRUE;

        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
            canvas_move (self, rtl ? -rows : rows, state, FALSE);
            return TRUE;

        case GDK_KEY_Page_Up:
        case GDK_KEY_KP_Page_Up:
            canvas_move (self, -rows * 3, state, FALSE);
            return TRUE;

        case GDK_KEY_Page_Down:
        case GDK_KEY_KP_Page_Down:
            canvas_move (self, rows * 3, state, FALSE);
            return TRUE;

        case GDK_KEY_Home:
        case GDK_KEY_KP_Home:
            canvas_move (self, 0, state, TRUE);
            return TRUE;

        case GDK_KEY_End:
        case GDK_KEY_KP_End:
            canvas_move (self, (int) n - 1, state, TRUE);
            return TRUE;

        case GDK_KEY_space:
            if ((state & GDK_CONTROL_MASK) && self->cursor < n)
            {
                GtkSelectionModel *model = GTK_SELECTION_MODEL (self->model);

                if (gtk_selection_model_is_selected (model, self->cursor))
                {
                    gtk_selection_model_unselect_item (model, self->cursor);
                }
                else
                {
                    gtk_selection_model_select_item (model, self->cursor, FALSE);
                }
                self->anchor = self->cursor;
                return TRUE;
            }
            return FALSE;

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_ISO_Enter:
            ammonite_list_base_activate_selection (self->list_base, FALSE);
            return TRUE;

        default:
            return FALSE;
    }
}

static void
on_focus_changed (AmmoniteCompactCanvas *self)
{
    canvas_update_all_cell_states (self);
}

/* --- Pointer ------------------------------------------------------------ */

static void
on_click_pressed (GtkGestureClick *gesture,
                  int              n_press,
                  double           x,
                  double           y,
                  gpointer         user_data)
{
    AmmoniteCompactCanvas *self = user_data;
    GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
    GtkSelectionModel *model = GTK_SELECTION_MODEL (self->model);
    guint position = canvas_item_at (self, x, y);

    gtk_widget_grab_focus (GTK_WIDGET (self));

    if (position == G_MAXUINT || n_press != 1)
    {
        return;
    }

    if (state & GDK_SHIFT_MASK)
    {
        canvas_select_range (self, self->anchor, position, FALSE);
    }
    else if (state & GDK_CONTROL_MASK)
    {
        if (gtk_selection_model_is_selected (model, position))
        {
            gtk_selection_model_unselect_item (model, position);
        }
        else
        {
            gtk_selection_model_select_item (model, position, FALSE);
        }
        self->anchor = position;
    }
    else
    {
        /* Keep multi-selections intact so they can be dragged. */
        if (!gtk_selection_model_is_selected (model, position))
        {
            gtk_selection_model_select_item (model, position, TRUE);
        }
        self->anchor = position;
    }
    canvas_set_cursor (self, position, FALSE);
}

static void
on_click_released (GtkGestureClick *gesture,
                   int              n_press,
                   double           x,
                   double           y,
                   gpointer         user_data)
{
    AmmoniteCompactCanvas *self = user_data;
    GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
    guint position = canvas_item_at (self, x, y);

    /* A plain click on part of a multi-selection narrows it to that item. */
    if (position != G_MAXUINT && n_press == 1 &&
        !(state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
        gtk_selection_model_is_selected (GTK_SELECTION_MODEL (self->model), position))
    {
        gtk_selection_model_select_item (GTK_SELECTION_MODEL (self->model), position, TRUE);
    }
}

static void
rubberband_apply (AmmoniteCompactCanvas *self)
{
    guint n = canvas_n_items (self);
    graphene_rect_t band;
    g_autoptr (GtkBitset) set = gtk_bitset_copy (self->rb_initial);
    g_autoptr (GtkBitset) mask = gtk_bitset_new_empty ();

    graphene_rect_init (&band,
                        MIN (self->rb_start.x, self->rb_end.x),
                        MIN (self->rb_start.y, self->rb_end.y),
                        fabsf (self->rb_start.x - self->rb_end.x),
                        fabsf (self->rb_start.y - self->rb_end.y));

    for (guint c = 0; c < self->n_cols; c++)
    {
        if (self->col_x[c] + self->col_w[c] < band.origin.x ||
            self->col_x[c] > band.origin.x + band.size.width)
        {
            continue;
        }

        for (guint i = c * self->rows; i < MIN (n, (c + 1) * self->rows); i++)
        {
            graphene_rect_t rect;

            if (canvas_position_geometry (self, i, &rect) && graphene_rect_intersection (&rect, &band, NULL))
            {
                gtk_bitset_add (set, i);
            }
        }
    }

    gtk_bitset_add_range (mask, 0, MAX (n, 1));
    gtk_selection_model_set_selection (GTK_SELECTION_MODEL (self->model), set, mask);
}

static void
on_drag_begin (GtkGestureDrag *gesture,
               double          x,
               double          y,
               gpointer        user_data)
{
    AmmoniteCompactCanvas *self = user_data;
    GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));

    if (!self->rubberband_enabled || canvas_item_at (self, x, y) != G_MAXUINT)
    {
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
        return;
    }

    self->rb_start.x = x;
    self->rb_start.y = y;
    self->rb_end = self->rb_start;
    g_clear_pointer (&self->rb_initial, gtk_bitset_unref);
    self->rb_initial = (state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
                       ? gtk_selection_model_get_selection (GTK_SELECTION_MODEL (self->model))
                       : gtk_bitset_new_empty ();
}

static void
on_drag_update (GtkGestureDrag *gesture,
                double          dx,
                double          dy,
                gpointer        user_data)
{
    AmmoniteCompactCanvas *self = user_data;

    if (!self->rubberband_enabled || self->rb_initial == NULL)
    {
        return;
    }

    if (!self->rubberbanding && (fabs (dx) > 4 || fabs (dy) > 4))
    {
        self->rubberbanding = TRUE;
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }

    if (self->rubberbanding)
    {
        self->rb_end.x = self->rb_start.x + dx;
        self->rb_end.y = self->rb_start.y + dy;
        rubberband_apply (self);
        gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
on_drag_end (GtkGestureDrag *gesture,
             double          dx,
             double          dy,
             gpointer        user_data)
{
    AmmoniteCompactCanvas *self = user_data;

    self->rubberbanding = FALSE;
    g_clear_pointer (&self->rb_initial, gtk_bitset_unref);
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

/* --- Model -------------------------------------------------------------- */

static void
on_model_items_changed (GListModel *model,
                        guint       position,
                        guint       removed,
                        guint       added,
                        gpointer    user_data)
{
    AmmoniteCompactCanvas *self = user_data;

    if (self->cursor >= g_list_model_get_n_items (model))
    {
        self->cursor = 0;
    }
    canvas_queue_relayout (self);
}

static void
on_model_selection_changed (GtkSelectionModel *model,
                            guint              position,
                            guint              n_items,
                            gpointer           user_data)
{
    canvas_update_all_cell_states (user_data);
}

static void
canvas_set_model (AmmoniteCompactCanvas *self,
                  AmmoniteViewModel     *model)
{
    if (self->model != NULL)
    {
        g_clear_signal_handler (&self->items_changed_id, self->model);
        g_clear_signal_handler (&self->selection_changed_id, self->model);
        canvas_remove_all_cells (self);
        g_clear_object (&self->model);
    }

    if (model != NULL)
    {
        self->model = g_object_ref (model);
        self->items_changed_id = g_signal_connect (model, "items-changed",
                                                   G_CALLBACK (on_model_items_changed), self);
        self->selection_changed_id = g_signal_connect (model, "selection-changed",
                                                       G_CALLBACK (on_model_selection_changed), self);
    }

    self->cursor = 0;
    self->anchor = 0;
    canvas_queue_relayout (self);
}

/* --- GtkScrollable ------------------------------------------------------ */

static void
on_hadjustment_changed (AmmoniteCompactCanvas *self)
{
    gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
canvas_set_hadjustment (AmmoniteCompactCanvas *self,
                        GtkAdjustment         *adjustment)
{
    if (self->hadjustment == adjustment)
    {
        return;
    }
    if (self->hadjustment != NULL)
    {
        g_signal_handlers_disconnect_by_func (self->hadjustment, on_hadjustment_changed, self);
        g_object_unref (self->hadjustment);
    }
    self->hadjustment = adjustment != NULL ? g_object_ref_sink (adjustment) : NULL;
    if (self->hadjustment != NULL)
    {
        g_signal_connect_swapped (self->hadjustment, "value-changed",
                                  G_CALLBACK (on_hadjustment_changed), self);
    }
}

static void
canvas_set_property (GObject      *object,
                     guint         prop_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
    AmmoniteCompactCanvas *self = AMMONITE_COMPACT_CANVAS (object);

    switch (prop_id)
    {
        case CANVAS_PROP_HADJUSTMENT:
            canvas_set_hadjustment (self, g_value_get_object (value));
            break;

        case CANVAS_PROP_VADJUSTMENT:
        case CANVAS_PROP_HSCROLL_POLICY:
        case CANVAS_PROP_VSCROLL_POLICY:
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
canvas_get_property (GObject    *object,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
    AmmoniteCompactCanvas *self = AMMONITE_COMPACT_CANVAS (object);

    switch (prop_id)
    {
        case CANVAS_PROP_HADJUSTMENT:
            g_value_set_object (value, self->hadjustment);
            break;

        case CANVAS_PROP_VADJUSTMENT:
            g_value_set_object (value, NULL);
            break;

        case CANVAS_PROP_HSCROLL_POLICY:
            g_value_set_enum (value, GTK_SCROLL_MINIMUM);
            break;

        case CANVAS_PROP_VSCROLL_POLICY:
            g_value_set_enum (value, GTK_SCROLL_NATURAL);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
canvas_dispose (GObject *object)
{
    AmmoniteCompactCanvas *self = AMMONITE_COMPACT_CANVAS (object);

    canvas_set_model (self, NULL);
    canvas_set_hadjustment (self, NULL);
    g_clear_pointer (&self->cells, g_hash_table_unref);
    g_clear_pointer (&self->rb_initial, gtk_bitset_unref);
    g_clear_pointer (&self->col_x, g_free);
    g_clear_pointer (&self->col_w, g_free);

    G_OBJECT_CLASS (ammonite_compact_canvas_parent_class)->dispose (object);
}

static void
ammonite_compact_canvas_class_init (AmmoniteCompactCanvasClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = canvas_dispose;
    object_class->set_property = canvas_set_property;
    object_class->get_property = canvas_get_property;
    widget_class->measure = canvas_measure;
    widget_class->size_allocate = canvas_size_allocate;
    widget_class->snapshot = canvas_snapshot;

    g_object_class_override_property (object_class, CANVAS_PROP_HADJUSTMENT, "hadjustment");
    g_object_class_override_property (object_class, CANVAS_PROP_VADJUSTMENT, "vadjustment");
    g_object_class_override_property (object_class, CANVAS_PROP_HSCROLL_POLICY, "hscroll-policy");
    g_object_class_override_property (object_class, CANVAS_PROP_VSCROLL_POLICY, "vscroll-policy");

    gtk_widget_class_set_css_name (widget_class, "compactview");
}

static void
ammonite_compact_canvas_init (AmmoniteCompactCanvas *self)
{
    GtkEventController *controller;
    GtkGesture *gesture;

    self->cells = g_hash_table_new (g_direct_hash, g_direct_equal);
    self->layout_dirty = TRUE;
    self->rubberband_enabled = TRUE;

    gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
    gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);

    controller = gtk_event_controller_key_new ();
    g_signal_connect (controller, "key-pressed", G_CALLBACK (on_key_pressed), self);
    gtk_widget_add_controller (GTK_WIDGET (self), controller);

    controller = gtk_event_controller_focus_new ();
    g_signal_connect_swapped (controller, "enter", G_CALLBACK (on_focus_changed), self);
    g_signal_connect_swapped (controller, "leave", G_CALLBACK (on_focus_changed), self);
    gtk_widget_add_controller (GTK_WIDGET (self), controller);

    gesture = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
    g_signal_connect (gesture, "pressed", G_CALLBACK (on_click_pressed), self);
    g_signal_connect (gesture, "released", G_CALLBACK (on_click_released), self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

    gesture = gtk_gesture_drag_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
    g_signal_connect (gesture, "drag-begin", G_CALLBACK (on_drag_begin), self);
    g_signal_connect (gesture, "drag-update", G_CALLBACK (on_drag_update), self);
    g_signal_connect (gesture, "drag-end", G_CALLBACK (on_drag_end), self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

/* ------------------------------------------------------------------------ */
/* The list base subclass                                                    */
/* ------------------------------------------------------------------------ */

struct _AmmoniteCompactView
{
    AmmoniteListBase parent_instance;

    AmmoniteCompactCanvas *canvas;

    gint zoom_level;
    gboolean directories_first;

    GQuark sort_attribute;
    gboolean reversed;
};

G_DEFINE_FINAL_TYPE (AmmoniteCompactView, ammonite_compact_view, AMMONITE_TYPE_LIST_BASE)

static const AmmoniteViewInfo compact_view_info =
{
    .view_id = AMMONITE_VIEW_COMPACT_ID,
    .zoom_level_min = AMMONITE_COMPACT_ZOOM_LEVEL_SMALL,
    .zoom_level_max = AMMONITE_COMPACT_ZOOM_LEVEL_LARGE,
    .zoom_level_standard = AMMONITE_COMPACT_ZOOM_LEVEL_LARGE,
};

static AmmoniteViewInfo
real_get_view_info (AmmoniteListBase *list_base)
{
    return compact_view_info;
}

static guint
real_get_icon_size (AmmoniteListBase *list_base)
{
    AmmoniteCompactView *self = AMMONITE_COMPACT_VIEW (list_base);

    switch ((AmmoniteCompactZoomLevel) self->zoom_level)
    {
        case AMMONITE_COMPACT_ZOOM_LEVEL_SMALL:
            return AMMONITE_COMPACT_ICON_SIZE_SMALL;

        case AMMONITE_COMPACT_ZOOM_LEVEL_MEDIUM:
            return AMMONITE_COMPACT_ICON_SIZE_MEDIUM;

        case AMMONITE_COMPACT_ZOOM_LEVEL_LARGE:
            return AMMONITE_COMPACT_ICON_SIZE_LARGE;
    }

    g_return_val_if_reached (AMMONITE_COMPACT_ICON_SIZE_SMALL);
}

static int
real_get_zoom_level (AmmoniteListBase *list_base)
{
    return AMMONITE_COMPACT_VIEW (list_base)->zoom_level;
}

static void
real_set_zoom_level (AmmoniteListBase *list_base,
                     int               new_level)
{
    AmmoniteCompactView *self = AMMONITE_COMPACT_VIEW (list_base);

    g_return_if_fail (new_level >= compact_view_info.zoom_level_min &&
                      new_level <= compact_view_info.zoom_level_max);

    self->zoom_level = new_level;
    g_object_notify (G_OBJECT (self), "icon-size");
    canvas_queue_relayout (self->canvas);
}

static void
real_scroll_to (AmmoniteListBase   *list_base,
                guint               position,
                GtkListScrollFlags  flags,
                GtkScrollInfo      *scroll)
{
    AmmoniteCompactView *self = AMMONITE_COMPACT_VIEW (list_base);
    AmmoniteCompactCanvas *canvas = self->canvas;

    if (position >= canvas_n_items (canvas))
    {
        return;
    }

    if (flags & GTK_LIST_SCROLL_SELECT)
    {
        gtk_selection_model_select_item (GTK_SELECTION_MODEL (canvas->model), position, TRUE);
        canvas->anchor = position;
    }

    if (flags & GTK_LIST_SCROLL_FOCUS)
    {
        gboolean horizontal = scroll == NULL || gtk_scroll_info_get_enable_horizontal (scroll);

        canvas_set_cursor (canvas, position, horizontal);
    }
    else if (scroll == NULL || gtk_scroll_info_get_enable_horizontal (scroll))
    {
        canvas_scroll_to_position (canvas, position);
    }
}

static void
real_preview_selection_event (AmmoniteListBase *list_base,
                              GtkDirectionType  direction)
{
    AmmoniteCompactCanvas *canvas = AMMONITE_COMPACT_VIEW (list_base)->canvas;
    gboolean rtl = gtk_widget_get_direction (GTK_WIDGET (canvas)) == GTK_TEXT_DIR_RTL;
    int rows = (int) canvas->rows;
    int delta = 0;
    guint n = canvas_n_items (canvas);
    guint first;
    g_autoptr (GtkBitset) selection = NULL;

    if (n == 0)
    {
        return;
    }

    selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (canvas->model));
    first = gtk_bitset_get_minimum (selection);
    if (first == G_MAXUINT)
    {
        return;
    }
    canvas->cursor = first;

    switch (direction)
    {
        case GTK_DIR_UP:
            delta = -1;
            break;

        case GTK_DIR_DOWN:
            delta = 1;
            break;

        case GTK_DIR_LEFT:
            delta = rtl ? rows : -rows;
            break;

        case GTK_DIR_RIGHT:
            delta = rtl ? -rows : rows;
            break;

        default:
            g_return_if_reached ();
    }

    canvas_move (canvas, delta, 0, FALSE);
}

static void
real_set_enable_rubberband (AmmoniteListBase *list_base,
                            gboolean          enabled)
{
    AMMONITE_COMPACT_VIEW (list_base)->canvas->rubberband_enabled = enabled;
}

static GVariant *
real_get_sort_state (AmmoniteListBase *list_base)
{
    AmmoniteCompactView *self = AMMONITE_COMPACT_VIEW (list_base);

    return g_variant_take_ref (g_variant_new ("(sb)",
                                              g_quark_to_string (self->sort_attribute),
                                              self->reversed));
}

static gint
compact_view_sort (gconstpointer a,
                   gconstpointer b,
                   gpointer      user_data)
{
    AmmoniteCompactView *self = user_data;
    AmmoniteFile *file_a = ammonite_view_item_get_file ((AmmoniteViewItem *) a);
    AmmoniteFile *file_b = ammonite_view_item_get_file ((AmmoniteViewItem *) b);

    return ammonite_file_compare_for_sort_by_attribute_q (file_a, file_b,
                                                          self->sort_attribute,
                                                          self->directories_first,
                                                          self->reversed);
}

static void
real_set_sort_state (AmmoniteListBase *list_base,
                     GVariant         *value)
{
    AmmoniteCompactView *self = AMMONITE_COMPACT_VIEW (list_base);
    const gchar *target_name;
    AmmoniteViewModel *model = ammonite_list_base_get_model (list_base);
    g_autoptr (GtkCustomSorter) sorter = NULL;

    g_return_if_fail (model != NULL);

    g_variant_get (value, "(&sb)", &target_name, &self->reversed);
    self->sort_attribute = g_quark_from_string (target_name);

    sorter = gtk_custom_sorter_new (compact_view_sort, self, NULL);
    ammonite_view_model_set_sorter (model, GTK_SORTER (sorter));
}

static void
update_sort_directories_first (AmmoniteCompactView *self)
{
    AmmoniteFile *directory_as_file = ammonite_list_base_get_directory_as_file (AMMONITE_LIST_BASE (self));
    AmmoniteViewModel *model = ammonite_list_base_get_model (AMMONITE_LIST_BASE (self));

    if (ammonite_file_is_in_search (directory_as_file) ||
        ammonite_file_is_in_recent (directory_as_file))
    {
        self->directories_first = FALSE;
    }
    else
    {
        self->directories_first = g_settings_get_boolean (gtk_filechooser_preferences,
                                                          AMMONITE_PREFERENCES_SORT_DIRECTORIES_FIRST);
    }

    if (model != NULL)
    {
        ammonite_view_model_sort (model);
    }
}

static void
real_setup_directory (AmmoniteListBase  *list_base,
                      AmmoniteDirectory *new_directory)
{
    AMMONITE_LIST_BASE_CLASS (ammonite_compact_view_parent_class)->setup_directory (list_base, new_directory);

    update_sort_directories_first (AMMONITE_COMPACT_VIEW (list_base));
}

static void
on_model_changed (AmmoniteCompactView *self)
{
    AmmoniteViewModel *model = ammonite_list_base_get_model (AMMONITE_LIST_BASE (self));

    canvas_set_model (self->canvas, model);
    if (model != NULL)
    {
        self->canvas->rubberband_enabled = !ammonite_view_model_get_single_selection (model);
    }
}

static void
ammonite_compact_view_constructed (GObject *object)
{
    AmmoniteCompactView *self = AMMONITE_COMPACT_VIEW (object);

    G_OBJECT_CLASS (ammonite_compact_view_parent_class)->constructed (object);

    ammonite_list_base_set_zoom_level (AMMONITE_LIST_BASE (self), compact_view_info.zoom_level_standard);
}

static void
ammonite_compact_view_class_init (AmmoniteCompactViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    AmmoniteListBaseClass *list_base_class = AMMONITE_LIST_BASE_CLASS (klass);

    object_class->constructed = ammonite_compact_view_constructed;

    list_base_class->get_icon_size = real_get_icon_size;
    list_base_class->get_sort_state = real_get_sort_state;
    list_base_class->get_view_info = real_get_view_info;
    list_base_class->get_zoom_level = real_get_zoom_level;
    list_base_class->preview_selection_event = real_preview_selection_event;
    list_base_class->scroll_to = real_scroll_to;
    list_base_class->set_enable_rubberband = real_set_enable_rubberband;
    list_base_class->set_sort_state = real_set_sort_state;
    list_base_class->set_zoom_level = real_set_zoom_level;
    list_base_class->setup_directory = real_setup_directory;
}

static void
ammonite_compact_view_init (AmmoniteCompactView *self)
{
    GtkWidget *scrolled_window = ammonite_list_base_get_scrolled_window (AMMONITE_LIST_BASE (self));

    gtk_widget_add_css_class (GTK_WIDGET (self), "ammonite-compact-view");

    self->canvas = g_object_new (AMMONITE_TYPE_COMPACT_CANVAS, NULL);
    self->canvas->list_base = AMMONITE_LIST_BASE (self);
    gtk_widget_set_vexpand (GTK_WIDGET (self->canvas), TRUE);

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window), GTK_WIDGET (self->canvas));

    ammonite_list_base_setup_gestures (AMMONITE_LIST_BASE (self));

    g_signal_connect_swapped (self, "notify::model", G_CALLBACK (on_model_changed), self);

    g_signal_connect_object (gtk_filechooser_preferences,
                             "changed::" AMMONITE_PREFERENCES_SORT_DIRECTORIES_FIRST,
                             G_CALLBACK (update_sort_directories_first),
                             self,
                             G_CONNECT_SWAPPED);
}

AmmoniteCompactView *
ammonite_compact_view_new (void)
{
    return g_object_new (AMMONITE_TYPE_COMPACT_VIEW, NULL);
}
