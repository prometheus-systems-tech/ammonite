/*
 * Copyright © 2026 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#define G_LOG_DOMAIN "test-properties"

#include <ammonite-application.h>
#include <ammonite-file.h>
#include <ammonite-file-utilities.h>
#include <ammonite-global-preferences.h>
#include <ammonite-properties.h>
#include <ammonite-resources.h>
#include <ammonite-tag-manager.h>

#include <test-utilities.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <adwaita.h>

static gboolean
all_files_are_ready (GList *files)
{
    for (GList *l = files; l != NULL; l = l->next)
    {
        AmmoniteFile *file = l->data;

        if (!ammonite_file_check_if_ready (file, AMMONITE_ATTRIBUTE_THUMBNAIL_BUFFER))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void
test_directory_empty (void)
{
    g_autoptr (GFile) test_location = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (AmmoniteFile) test_file = ammonite_file_get (test_location);
    GtkWindow *properties_window =
        ammonite_properties_present_window (&(GList) { .data = test_file }, NULL);

    ITER_CONTEXT_WHILE (!ammonite_file_check_if_ready (test_file, AMMONITE_ATTRIBUTE_DEEP_COUNT));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (properties_window);

    test_clear_tmp_dir ();
}

static void
test_directory_root (void)
{
    g_autoptr (GFile) test_location = g_file_new_for_path ("/");
    g_autoptr (AmmoniteFile) test_file = ammonite_file_get (test_location);
    GtkWindow *properties_window =
        ammonite_properties_present_window (&(GList) { .data = test_file }, NULL);

    ITER_CONTEXT_WHILE (!ammonite_file_check_if_ready (test_file, AMMONITE_ATTRIBUTE_INFO));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (properties_window);

    test_clear_tmp_dir ();
}

static void
test_directory_unreadable (void)
{
    g_autoptr (GFile) unreadable_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                       "unaccessible",
                                                                       NULL);
    g_autoptr (AmmoniteFile) unreadable_file = ammonite_file_get (unreadable_location);
    GtkWindow *properties_window = NULL;

    g_assert_true (g_file_make_directory (unreadable_location, NULL, NULL));
    g_assert_true (g_file_set_attribute_uint32 (unreadable_location,
                                                G_FILE_ATTRIBUTE_UNIX_MODE,
                                                0000,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, NULL));
    properties_window =
        ammonite_properties_present_window (&(GList) { .data = unreadable_file }, NULL);

    ITER_CONTEXT_WHILE (!ammonite_file_check_if_ready (unreadable_file, AMMONITE_ATTRIBUTE_INFO));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (properties_window);

    test_clear_tmp_dir ();
}

static void
test_image (void)
{
    g_autoptr (GFile) image1_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                   "image1.png",
                                                                   NULL);
    g_autoptr (GFile) image2_location = g_file_new_build_filename (test_get_tmp_dir (),
                                                                   "image2.png",
                                                                   NULL);
    g_autoptr (AmmoniteFile) image1_file = ammonite_file_get (image1_location);
    g_autoptr (AmmoniteFile) image2_file = ammonite_file_get (image2_location);
    GtkWindow *properties_window = NULL;

    /* First image is thumbnailed */
    make_image_file_with_mtime (image1_location, 1, TRUE);
    properties_window = ammonite_properties_present_window (&(GList) { .data = image1_file }, NULL);

    ITER_CONTEXT_WHILE (!ammonite_file_check_if_ready (image1_file,
                                                       AMMONITE_ATTRIBUTE_THUMBNAIL_BUFFER));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (properties_window);

    /* Second image is not thumbnailed */
    make_image_file_with_mtime (image2_location, 1, FALSE);
    properties_window = ammonite_properties_present_window (&(GList) { .data = image2_file }, NULL);

    ITER_CONTEXT_WHILE (!ammonite_file_check_if_ready (image2_file,
                                                       AMMONITE_ATTRIBUTE_THUMBNAIL_BUFFER));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (properties_window);

    test_clear_tmp_dir ();
}

static void
test_images (void)
{
    GStrv hier = (char *[])
    {
        "%simage1.png",
        "%simage2.png",
        "%simage3.png",
        "%simage4.png",
        NULL
    };
    g_autolist (GFile) image_locations = file_hierarchy_get_files_list (hier, "", FALSE);
    g_autolist (GFile) thumb_image_locations = file_hierarchy_get_files_list (hier, "thumb_", FALSE);
    g_autolist (AmmoniteFile) image_files = NULL;
    g_autolist (AmmoniteFile) thumb_image_files = NULL;
    GtkWindow *properties_window = NULL;

    /* Many images that are not thumbnailed */
    for (GList *l = image_locations; l != NULL; l = l->next)
    {
        GFile *location = l->data;

        make_image_file_with_mtime (location, 1, FALSE);
        image_files = g_list_prepend (image_files, ammonite_file_get (location));
    }
    image_files = g_list_reverse (image_files);
    properties_window = ammonite_properties_present_window (image_files, NULL);

    ITER_CONTEXT_WHILE (!all_files_are_ready (image_files));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (properties_window);

    /* Many images that are thumbnailed */
    for (GList *l = thumb_image_locations; l != NULL; l = l->next)
    {
        GFile *location = l->data;

        make_image_file_with_mtime (location, 1, TRUE);
        thumb_image_files = g_list_prepend (thumb_image_files, ammonite_file_get (location));
    }
    thumb_image_files = g_list_reverse (thumb_image_files);
    properties_window = ammonite_properties_present_window (thumb_image_files, NULL);

    ITER_CONTEXT_WHILE (!all_files_are_ready (thumb_image_files));
    gtk_test_widget_wait_for_draw (GTK_WIDGET (properties_window));

    gtk_window_close (properties_window);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    if (ammonite_application_is_sandboxed ())
    {
        g_message ("Properties tests don't run inside a sandbox, due to issues "
                   "with window management.");

        return 77;
    }

    g_autoptr (AmmoniteTagManager) tag_manager = NULL;

    gtk_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);
    adw_init ();

    /* HACK: Pretend to be a snap to disable sandboxing in libgnome-desktop */
    g_setenv ("SNAP_NAME", "1", TRUE);

    ammonite_register_resource ();
    ammonite_ensure_extension_points ();
    ammonite_global_preferences_init ();
    tag_manager = ammonite_tag_manager_new_dummy ();

    g_test_add_func ("/properties/directory/empty",
                     test_directory_empty);
    g_test_add_func ("/properties/directory/root",
                     test_directory_root);
    g_test_add_func ("/properties/directory/unreadable",
                     test_directory_unreadable);
    g_test_add_func ("/properties/image/one",
                     test_image);
    g_test_add_func ("/properties/image/many",
                     test_images);

    return g_test_run ();
}
