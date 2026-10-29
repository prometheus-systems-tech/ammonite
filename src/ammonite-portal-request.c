/*
 * SPDX-FileCopyrightText: 2016 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Matthias Clasen <mclasen@redhat.com>
 */

#include "ammonite-portal-request.h"

#include <string.h>

static void request_skeleton_iface_init (XdpImplRequestIface *iface);

struct _AmmonitePortalRequest
{
    XdpImplRequestSkeleton parent_instance;

    gboolean exported;
    char *sender;
    char *app_id;
    char *id;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (AmmonitePortalRequest, ammonite_portal_request, XDP_IMPL_TYPE_REQUEST_SKELETON,
                               G_IMPLEMENT_INTERFACE (XDP_IMPL_TYPE_REQUEST, request_skeleton_iface_init))

static gboolean
handle_close (XdpImplRequest        *object,
              GDBusMethodInvocation *invocation)
{
    AmmonitePortalRequest *self = (AmmonitePortalRequest *) object;

    if (self->exported)
    {
        ammonite_portal_request_unexport (self);
    }

    xdp_impl_request_complete_close (XDP_IMPL_REQUEST (self), invocation);

    return TRUE;
}

static void
request_skeleton_iface_init (XdpImplRequestIface *iface)
{
    iface->handle_close = handle_close;
}

static void
ammonite_portal_request_init (AmmonitePortalRequest *self)
{
}

static void
portal_request_finalize (GObject *object)
{
    AmmonitePortalRequest *self = (AmmonitePortalRequest *) object;

    g_free (self->sender);
    g_free (self->app_id);
    g_free (self->id);

    G_OBJECT_CLASS (ammonite_portal_request_parent_class)->finalize (object);
}

static void
ammonite_portal_request_class_init (AmmonitePortalRequestClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = portal_request_finalize;
}

AmmonitePortalRequest *
ammonite_portal_request_new (const char *sender,
                             const char *app_id,
                             const char *id)
{
    AmmonitePortalRequest *self = g_object_new (ammonite_portal_request_get_type (), NULL);

    self->sender = g_strdup (sender);
    self->app_id = g_strdup (app_id);
    self->id = g_strdup (id);

    return self;
}

void
ammonite_portal_request_export (AmmonitePortalRequest *self,
                                GDBusConnection       *connection)
{
    g_autoptr (GError) error = NULL;

    if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self),
                                           connection,
                                           self->id,
                                           &error))
    {
        g_warning ("error exporting request: %s\n", error->message);
        g_clear_error (&error);
    }

    g_object_ref (self);
    self->exported = TRUE;
}

void
ammonite_portal_request_unexport (AmmonitePortalRequest *self)
{
    self->exported = FALSE;
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self));
    g_object_unref (self);
}
