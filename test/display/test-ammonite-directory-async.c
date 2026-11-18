/*
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 */

#include <src/ammonite-directory.h>
#include <src/ammonite-file-utilities.h>

static GMainLoop *main_loop;
void *client1, *client2;

static void
files_added (AmmoniteDirectory *directory,
             GList             *added_files)
{
#if 0
    GList *list;

    for (list = added_files; list != NULL; list = list->next)
    {
        AmmoniteFile *file = list->data;

        g_print (" - %s\n", ammonite_file_get_uri (file));
    }
#endif

    g_print ("files added: %d files\n",
             g_list_length (added_files));
}

static void
files_changed (AmmoniteDirectory *directory,
               GList             *changed_files)
{
#if 0
    GList *list;

    for (list = changed_files; list != NULL; list = list->next)
    {
        AmmoniteFile *file = list->data;

        g_print (" - %s\n", ammonite_file_get_uri (file));
    }
#endif
    g_print ("files changed: %d\n",
             g_list_length (changed_files));
}

static void
done_loading (AmmoniteDirectory *directory)
{
    g_print ("done loading\n");
    g_main_loop_quit (main_loop);
}

int
main (int    argc,
      char **argv)
{
    AmmoniteDirectory *directory;
    AmmoniteAttributes attributes;
    const char *uri;

    client1 = g_new0 (int, 1);
    client2 = g_new0 (int, 1);

    main_loop = g_main_loop_new (NULL, FALSE);

    ammonite_ensure_extension_points ();

    if (argv[1] == NULL)
    {
        uri = "file:///tmp";
    }
    else
    {
        uri = argv[1];
    }
    g_print ("loading %s", uri);
    directory = ammonite_directory_get_by_uri (uri);

    g_signal_connect (directory, "files-added", G_CALLBACK (files_added), NULL);
    g_signal_connect (directory, "files-changed", G_CALLBACK (files_changed), NULL);
    g_signal_connect (directory, "done-loading", G_CALLBACK (done_loading), NULL);

    attributes =
        AMMONITE_ATTRIBUTE_THUMBNAIL_BUFFER |
        AMMONITE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
        AMMONITE_ATTRIBUTE_INFO |
        AMMONITE_ATTRIBUTE_MOUNT |
        AMMONITE_ATTRIBUTE_EXTENSION_INFO;

    ammonite_directory_file_monitor_add (directory, client1, TRUE,
                                         attributes,
                                         NULL, NULL);


    g_main_loop_run (main_loop);

    g_main_loop_unref (main_loop);

    return 0;
}
