/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 */

#define G_LOG_DOMAIN "test-sidebar"

#include <ammonite-application.h>
#include <ammonite-file-utilities.h>
#include <ammonite-global-preferences.h>
#include <ammonite-resources.h>
#include <ammonite-sidebar.h>
#include <ammonite-tag-manager.h>

#include <test-utilities.h>

#include <glib.h>
#include <gtk/gtk.h>

static void
test_sidebar_set_location (void)
{
    g_autoptr (AmmoniteSidebar) sidebar = AMMONITE_PLACES_SIDEBAR (ammonite_sidebar_new ());
    g_autoptr (GFile) home_location = g_file_new_for_path (g_get_home_dir ());

    /* Need to sink the object */
    g_object_ref_sink (sidebar);

    ammonite_sidebar_set_location (sidebar, home_location);

    g_autoptr (GFile) view_location = ammonite_sidebar_get_location (sidebar);

    /* Only locations on the sidebar will return non-null */
    g_assert_true (g_file_equal (view_location, home_location));
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (AmmoniteTagManager) tag_manager = NULL;

    gtk_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

    ammonite_register_resource ();
    ammonite_ensure_extension_points ();
    ammonite_global_preferences_init ();
    tag_manager = ammonite_tag_manager_new_dummy ();
    test_init_config_dir ();

    g_autoptr (AmmoniteApplication) app = ammonite_application_new ();

    g_test_add_func ("/sidebar/set_location",
                     test_sidebar_set_location);

    return g_test_run ();
}
