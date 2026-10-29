#include "test-utilities.h"

#include <src/ammonite-file-utilities.h>
#include <src/ammonite-global-preferences.h>
#include <src/ammonite-query.h>
#include <src/ammonite-search-engine.h>
#include <src/ammonite-search-hit.h>

static guint total_hits = 0;

static void
hits_added_cb (AmmoniteSearchEngine *engine,
               GPtrArray            *hits)
{
    g_print ("Hits added for search engine model!\n");
    for (guint i = 0; i < hits->len; i++)
    {
        g_print ("Hit %i: %s\n", i, ammonite_search_hit_get_uri (hits->pdata[i]));

        total_hits += 1;
    }
}

static void
finished_cb (GMainLoop *loop)
{
    g_print ("\nAmmonite search engine model finished!\n");

    delete_search_file_hierarchy ("model");

    g_main_loop_quit (loop);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (GMainLoop) loop = NULL;
    g_autoptr (AmmoniteQuery) query = NULL;
    g_autoptr (GFile) location = NULL;

    loop = g_main_loop_new (NULL, FALSE);

    ammonite_ensure_extension_points ();
    /* Needed for ammonite-query.c.
     * FIXME: tests are not installed, so the system does not
     * have the gschema. Installed tests is a long term GNOME goal.
     */
    ammonite_global_preferences_init ();

    g_autoptr (AmmoniteSearchEngine) engine =
        ammonite_search_engine_new (AMMONITE_SEARCH_TYPE_MODEL);
    g_signal_connect (engine, "hits-added",
                      G_CALLBACK (hits_added_cb), NULL);
    g_signal_connect_swapped (engine, "search-finished", G_CALLBACK (finished_cb), loop);

    query = ammonite_query_new ();
    ammonite_query_set_text (query, "engine_model");

    location = g_file_new_for_path (test_get_tmp_dir ());

    ammonite_query_set_location (query, location);

    create_search_file_hierarchy ("model");

    ammonite_search_engine_start (engine, query);
    g_main_loop_run (loop);

    g_assert_cmpint (total_hits, ==, 3);

    test_clear_tmp_dir ();

    return 0;
}
