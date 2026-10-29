/*
 * SPDX-FileCopyrightText: 2001 Maciej Stachowiak
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Maciej Stachowiak <mjs@noisehavoc.org>
 */

#include <config.h>
#include "ammonite-hash-queue.h"

#include <glib.h>

/**
 * `AmmoniteHashQueue` is a `GQueue` with 2 special features:
 * 1) It can find and remove items in constant time
 * 2) It doesn't allow duplicates.
 */
struct AmmoniteHashQueue
{
    GQueue parent;
    GHashTable *item_to_link_map;
    GHashTable *link_to_item_map;
    GDestroyNotify key_destroy_func;
    GDestroyNotify value_destroy_func;
};

/**
 * ammonite_hash_queue_new:
 * @hash_func: a function to create a hash value from a key
 * @key_equal_func: a function to check two keys for equality
 * @key_destroy_func: (nullable): a function to free the memory allocated for
 *     the key used when removing the entry from the #GHashTable, or `NULL` if
 *     you don't want to supply such a function.
 * @value_destroy_func: (nullable): a function to free the memory allocated for
 *     the value used when removing the entry from the #GHashTable, or `NULL`
 *     if you don't want to supply such a function.
 *
 * Creates a new #AmmoniteHashQueue.
 *
 * Returns: (transfer full): a new #AmmoniteHashQueue
 */
AmmoniteHashQueue *
ammonite_hash_queue_new (GHashFunc      hash_func,
                         GEqualFunc     equal_func,
                         GDestroyNotify key_destroy_func,
                         GDestroyNotify value_destroy_func)
{
    AmmoniteHashQueue *queue;

    queue = g_new0 (AmmoniteHashQueue, 1);
    g_queue_init ((GQueue *) queue);
    queue->item_to_link_map = g_hash_table_new_full (hash_func, equal_func,
                                                     key_destroy_func, NULL);
    queue->link_to_item_map = g_hash_table_new (NULL, NULL);
    queue->key_destroy_func = key_destroy_func;
    queue->value_destroy_func = value_destroy_func;

    return queue;
}

void
ammonite_hash_queue_destroy (AmmoniteHashQueue *queue)
{
    g_hash_table_destroy (queue->item_to_link_map);
    g_hash_table_destroy (queue->link_to_item_map);

    if (queue->value_destroy_func != NULL)
    {
        g_queue_clear_full ((GQueue *) queue, queue->value_destroy_func);
    }
    else
    {
        g_queue_clear ((GQueue *) queue);
    }

    g_free (queue);
}

static gboolean
ammonite_hash_queue_enqueue_internal (AmmoniteHashQueue *queue,
                                      gpointer           key,
                                      gpointer           value,
                                      gboolean           reenqueue)
{
    GList *link = g_hash_table_lookup (queue->item_to_link_map, key);

    if (link != NULL)
    {
        /* It's already on the queue. */
        if (queue->key_destroy_func != NULL)
        {
            queue->key_destroy_func (key);
        }
        if (queue->value_destroy_func != NULL)
        {
            queue->value_destroy_func (value);
        }

        if (reenqueue)
        {
            g_queue_unlink ((GQueue *) queue, link);
            g_queue_push_tail_link ((GQueue *) queue, link);
        }

        return FALSE;
    }

    g_queue_push_tail ((GQueue *) queue, value);
    g_hash_table_insert (queue->item_to_link_map, key, queue->parent.tail);
    g_hash_table_insert (queue->link_to_item_map, queue->parent.tail, key);

    return TRUE;
}

/** Add an item to the tail of the queue, unless it's already in the queue. */
gboolean
ammonite_hash_queue_enqueue (AmmoniteHashQueue *queue,
                             gpointer           key,
                             gpointer           value)
{
    return ammonite_hash_queue_enqueue_internal (queue, key, value, FALSE);
}

gboolean
ammonite_hash_queue_reenqueue (AmmoniteHashQueue *queue,
                               gpointer           key,
                               gpointer           value)
{
    return ammonite_hash_queue_enqueue_internal (queue, key, value, TRUE);
}

/**
 * Remove the item responding to the provided key from the queue in constant time.
 */
void
ammonite_hash_queue_remove (AmmoniteHashQueue *queue,
                            gconstpointer      key)
{
    gpointer map_key;
    GList *link;

    if (g_hash_table_steal_extended (queue->item_to_link_map, key, &map_key, (gpointer *) &link))
    {
        if (queue->value_destroy_func != NULL)
        {
            queue->value_destroy_func (link->data);
        }

        g_hash_table_remove (queue->link_to_item_map, link);
        g_queue_delete_link ((GQueue *) queue, link);

        if (queue->key_destroy_func != NULL)
        {
            queue->key_destroy_func (map_key);
        }
    }
}

void
ammonite_hash_queue_remove_head (AmmoniteHashQueue *queue)
{
    GList *link = g_queue_peek_head_link ((GQueue *) (queue));

    if (link != NULL)
    {
        gpointer map_key = g_hash_table_lookup (queue->link_to_item_map, link);

        ammonite_hash_queue_remove (queue, map_key);
    }
}

gpointer
ammonite_hash_queue_find_item (AmmoniteHashQueue *queue,
                               gconstpointer      key)
{
    GList *link = g_hash_table_lookup (queue->item_to_link_map, key);

    if (link == NULL)
    {
        return NULL;
    }

    return link->data;
}

void
ammonite_hash_queue_move_existing_to_head (AmmoniteHashQueue *queue,
                                           gconstpointer      key)
{
    GList *link = g_hash_table_lookup (queue->item_to_link_map, key);

    if (link == NULL)
    {
        return;
    }

    g_queue_unlink ((GQueue *) queue, link);
    g_queue_push_head_link ((GQueue *) queue, link);
}

void
ammonite_hash_queue_move_existing_to_tail (AmmoniteHashQueue *queue,
                                           gconstpointer      key)
{
    GList *link = g_hash_table_lookup (queue->item_to_link_map, key);

    if (link == NULL)
    {
        return;
    }

    g_queue_unlink ((GQueue *) queue, link);
    g_queue_push_tail_link ((GQueue *) queue, link);
}
