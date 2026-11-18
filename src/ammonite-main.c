/*
 * SPDX-FileCopyrightText: 1999, 2000 Red Hat, Inc.
 * SPDX-FileCopyrightText: 1999, 2000 Eazel, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          John Sullivan <sullivan@eazel.com>
 */

/* ammonite-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>

#include "ammonite-application.h"
#include "ammonite-resources.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <locale.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <unistd.h>

int
main (int   argc,
      char *argv[])
{
    gint retval;
    AmmoniteApplication *application;
    /* Initialize gettext support */
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    g_set_prgname (APPLICATION_ID);

    /* In the sandbox XDG_DATA_HOME points at the app's private dir, and the
     * manifest's XDG_DATA_DIRS lacks the user's own flatpak exports, so apps
     * installed per-user (and ~/.local/share/applications launchers) would be
     * missing from the application chooser. Append the host user locations. */
    if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    {
        const char *home = g_get_home_dir ();
        const char *old_dirs = g_getenv ("XDG_DATA_DIRS");
        g_autofree char *new_dirs = g_strdup_printf ("%s%s%s/.local/share/flatpak/exports/share:%s/.local/share",
                                                     old_dirs != NULL ? old_dirs : "",
                                                     old_dirs != NULL ? ":" : "",
                                                     home, home);
        g_setenv ("XDG_DATA_DIRS", new_dirs, TRUE);
    }

    if (getuid () == 0)
    {
        g_warning (_("Running as root is not supported. "
                     "Consider running `ammonite admin:///` instead."));

        exit (ENOTSUP);
    }

    ammonite_register_resource ();
    /* Run the ammonite application. */
    application = ammonite_application_new ();

    /* hold indefinitely if we're asked to persist */
    if (g_getenv ("AMMONITE_PERSIST") != NULL)
    {
        g_application_hold (G_APPLICATION (application));
    }

    retval = g_application_run (G_APPLICATION (application),
                                argc, argv);

    g_object_unref (application);

    return retval;
}
