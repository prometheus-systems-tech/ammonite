/*
 * SPDX-FileCopyrightText: 2001 Maciej Stachowiak
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Maciej Stachowiak <mjs@noisehavoc.org>
 */

#pragma once


#include <glib.h>

/** Function to create a hashable key from an item. */
typedef gpointer (* KeyCreateFunc) (gpointer item);

typedef struct AmmoniteHashQueue AmmoniteHashQueue;

AmmoniteHashQueue * ammonite_hash_queue_new     (GHashFunc      hash_func,
                                                 GEqualFunc     equal_func,
                                                 GDestroyNotify key_destroy_func,
                                                 GDestroyNotify value_destroy_func);
void               ammonite_hash_queue_destroy  (AmmoniteHashQueue *queue);

gboolean           ammonite_hash_queue_enqueue  (AmmoniteHashQueue *queue,
                                                 gpointer           key,
                                                 gpointer           value);
gboolean           ammonite_hash_queue_reenqueue             (AmmoniteHashQueue *queue,
                                                              gpointer           key,
                                                              gpointer           value);
void               ammonite_hash_queue_remove   (AmmoniteHashQueue *queue,
                                                 gconstpointer      key);
gpointer           ammonite_hash_queue_find_item             (AmmoniteHashQueue *queue,
                                                              gconstpointer      key);
void               ammonite_hash_queue_move_existing_to_head (AmmoniteHashQueue *queue,
                                                              gconstpointer      key);
void               ammonite_hash_queue_move_existing_to_tail (AmmoniteHashQueue *queue,
                                                              gconstpointer      key);
void               ammonite_hash_queue_remove_head           (AmmoniteHashQueue *queue);

/* Get the file at the head of the queue without removing or unrefing it. */
#define ammonite_hash_queue_peek_head(queue) (g_queue_peek_head ((GQueue *) (queue)))

#define ammonite_hash_queue_is_empty(queue) (g_queue_is_empty ((GQueue *) (queue)))

#define ammonite_hash_queue_get_length(queue) (g_queue_get_length ((GQueue *) (queue)))
