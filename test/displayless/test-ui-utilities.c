#include <glib.h>

#include <ammonite-ui-utilities.h>


static void
test_string_capitalization (void)
{
    char *capitalized;

    g_assert_null (ammonite_capitalize_str (NULL));

    capitalized = ammonite_capitalize_str ("");
    g_assert_cmpstr (capitalized, ==, "");
    g_free (capitalized);

    capitalized = ammonite_capitalize_str ("foo");
    g_assert_cmpstr (capitalized, ==, "Foo");
    g_free (capitalized);

    capitalized = ammonite_capitalize_str ("Foo");
    g_assert_cmpstr (capitalized, ==, "Foo");
    g_free (capitalized);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();

    g_test_add_func ("/string-capitalization",
                     test_string_capitalization);

    return g_test_run ();
}
