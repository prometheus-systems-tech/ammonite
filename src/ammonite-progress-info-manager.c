/*
 * SPDX-FileCopyrightText: 2011 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#include <config.h>

#include "ammonite-progress-info-manager.h"

#include "ammonite-progress-info.h"

struct _AmmoniteProgressInfoManager
{
    GObject parent_instance;

    GList *progress_infos;
    GList *current_viewers;
};

enum
{
    NEW_PROGRESS_INFO,
    HAS_VIEWERS_CHANGED,
    LAST_SIGNAL
};

static AmmoniteProgressInfoManager *singleton = NULL;

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_FINAL_TYPE (AmmoniteProgressInfoManager, ammonite_progress_info_manager, G_TYPE_OBJECT);

static void remove_viewer (AmmoniteProgressInfoManager *self,
                           GObject                     *viewer);

static void
ammonite_progress_info_manager_finalize (GObject *obj)
{
    GList *l;
    AmmoniteProgressInfoManager *self = AMMONITE_PROGRESS_INFO_MANAGER (obj);

    if (self->progress_infos != NULL)
    {
        g_list_free_full (self->progress_infos, g_object_unref);
    }

    for (l = self->current_viewers; l != NULL; l = l->next)
    {
        g_object_weak_unref (l->data, (GWeakNotify) remove_viewer, self);
    }
    g_list_free (self->current_viewers);

    G_OBJECT_CLASS (ammonite_progress_info_manager_parent_class)->finalize (obj);
}

static GObject *
ammonite_progress_info_manager_constructor (GType                  type,
                                            guint                  n_props,
                                            GObjectConstructParam *props)
{
    GObject *retval;

    if (singleton != NULL)
    {
        return G_OBJECT (g_object_ref (singleton));
    }

    retval = G_OBJECT_CLASS (ammonite_progress_info_manager_parent_class)->constructor
                 (type, n_props, props);

    singleton = AMMONITE_PROGRESS_INFO_MANAGER (retval);
    g_object_add_weak_pointer (retval, (gpointer) & singleton);

    return retval;
}

static void
ammonite_progress_info_manager_init (AmmoniteProgressInfoManager *self)
{
}

static void
ammonite_progress_info_manager_class_init (AmmoniteProgressInfoManagerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->constructor = ammonite_progress_info_manager_constructor;
    oclass->finalize = ammonite_progress_info_manager_finalize;

    signals[NEW_PROGRESS_INFO] =
        g_signal_new ("new-progress-info",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE,
                      1,
                      AMMONITE_TYPE_PROGRESS_INFO);

    signals[HAS_VIEWERS_CHANGED] =
        g_signal_new ("has-viewers-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
}

AmmoniteProgressInfoManager *
ammonite_progress_info_manager_dup_singleton (void)
{
    return g_object_new (AMMONITE_TYPE_PROGRESS_INFO_MANAGER, NULL);
}

void
ammonite_progress_info_manager_add_new_info (AmmoniteProgressInfoManager *self,
                                             AmmoniteProgressInfo        *info)
{
    if (g_list_find (self->progress_infos, info) != NULL)
    {
        g_warning ("Adding two times the same progress info object to the manager");
        return;
    }

    self->progress_infos =
        g_list_prepend (self->progress_infos, g_object_ref (info));

    g_signal_emit (self, signals[NEW_PROGRESS_INFO], 0, info);
}

void
ammonite_progress_info_manager_remove_finished_or_cancelled_infos (AmmoniteProgressInfoManager *self)
{
    GList *l;
    GList *next;

    l = self->progress_infos;
    while (l != NULL)
    {
        next = l->next;
        if (ammonite_progress_info_get_is_finished (l->data) ||
            ammonite_progress_info_get_is_cancelled (l->data))
        {
            g_object_unref (l->data);
            self->progress_infos = g_list_remove (self->progress_infos,
                                                  l->data);
        }
        l = next;
    }
}

GList *
ammonite_progress_info_manager_get_all_infos (AmmoniteProgressInfoManager *self)
{
    return self->progress_infos;
}

gboolean
ammonite_progress_manager_are_all_infos_finished_or_cancelled (AmmoniteProgressInfoManager *self)
{
    GList *l;

    for (l = self->progress_infos; l != NULL; l = l->next)
    {
        if (!(ammonite_progress_info_get_is_finished (l->data) ||
              ammonite_progress_info_get_is_cancelled (l->data)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void
remove_viewer (AmmoniteProgressInfoManager *self,
               GObject                     *viewer)
{
    self->current_viewers = g_list_remove (self->current_viewers, viewer);

    if (self->current_viewers == NULL)
    {
        g_signal_emit (self, signals[HAS_VIEWERS_CHANGED], 0);
    }
}

void
ammonite_progress_manager_add_viewer (AmmoniteProgressInfoManager *self,
                                      GObject                     *viewer)
{
    GList *viewers;

    viewers = self->current_viewers;
    if (g_list_find (viewers, viewer) == NULL)
    {
        g_object_weak_ref (viewer, (GWeakNotify) remove_viewer, self);
        viewers = g_list_append (viewers, viewer);
        self->current_viewers = viewers;

        if (viewers != NULL && viewers->next == NULL)
        {
            g_signal_emit (self, signals[HAS_VIEWERS_CHANGED], 0);
        }
    }
}

void
ammonite_progress_manager_remove_viewer (AmmoniteProgressInfoManager *self,
                                         GObject                     *viewer)
{
    if (g_list_find (self->current_viewers, viewer) != NULL)
    {
        g_object_weak_unref (viewer, (GWeakNotify) remove_viewer, self);
        remove_viewer (self, viewer);
    }
}

gboolean
ammonite_progress_manager_has_viewers (AmmoniteProgressInfoManager *self)
{
    return self->current_viewers != NULL;
}
