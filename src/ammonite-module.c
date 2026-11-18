/*
 * ammonite-module.h - Interface to ammonite extensions
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 * SPDX-FileCopyrightText: 2026 Ross WD Cameron <rwd.cameron@prometheus-systems.co.za>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#include <config.h>
#include "ammonite-module.h"

#include <gmodule.h>

#define AMMONITE_TYPE_MODULE            (ammonite_module_get_type ())
#define AMMONITE_MODULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), AMMONITE_TYPE_MODULE, AmmoniteModule))
#define AMMONITE_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), AMMONITE_TYPE_MODULE, AmmoniteModule))
#define AMMONITE_IS_MODULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), AMMONITE_TYPE_MODULE))
#define AMMONITE_IS_MODULE_CLASS(klass) (G_TYPE_CLASS_CHECK_CLASS_TYPE ((klass), AMMONITE_TYPE_MODULE))

typedef struct _AmmoniteModule AmmoniteModule;
typedef struct _AmmoniteModuleClass AmmoniteModuleClass;

struct _AmmoniteModule
{
    GTypeModule parent;

    GModule *library;

    char *path;

    void (*initialize) (GTypeModule *module);
    void (*shutdown)   (void);

    void (*list_types) (const GType **types,
                        int          *num_types);
};

struct _AmmoniteModuleClass
{
    GTypeModuleClass parent;
};

static GList *module_objects = NULL;
static GList *installed_modules = NULL;

static GType ammonite_module_get_type (void);

G_DEFINE_TYPE (AmmoniteModule, ammonite_module, G_TYPE_TYPE_MODULE);

static gboolean
ammonite_module_load (GTypeModule *gmodule)
{
    AmmoniteModule *module;

    module = AMMONITE_MODULE (gmodule);

    module->library = g_module_open (module->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

    if (!module->library)
    {
        g_warning ("%s", g_module_error ());
        return FALSE;
    }

    if (!g_module_symbol (module->library,
                          "ammonite_module_initialize",
                          (gpointer *) &module->initialize) ||
        !g_module_symbol (module->library,
                          "ammonite_module_shutdown",
                          (gpointer *) &module->shutdown) ||
        !g_module_symbol (module->library,
                          "ammonite_module_list_types",
                          (gpointer *) &module->list_types))
    {
        g_warning ("%s", g_module_error ());
        g_module_close (module->library);

        return FALSE;
    }

    g_module_make_resident (module->library);
    module->initialize (gmodule);

    return TRUE;
}

static void
ammonite_module_finalize (GObject *object)
{
    AmmoniteModule *module;

    module = AMMONITE_MODULE (object);

    g_free (module->path);

    G_OBJECT_CLASS (ammonite_module_parent_class)->finalize (object);
}

static void
ammonite_module_init (AmmoniteModule *module)
{
}

static void
ammonite_module_class_init (AmmoniteModuleClass *class)
{
    G_OBJECT_CLASS (class)->finalize = ammonite_module_finalize;
    G_TYPE_MODULE_CLASS (class)->load = ammonite_module_load;
}

static void
module_object_weak_notify (gpointer  user_data,
                           GObject  *object)
{
    module_objects = g_list_remove (module_objects, object);
}

static void
add_module_objects (AmmoniteModule *module)
{
    const GType *types;
    int num_types;
    int i;

    module->list_types (&types, &num_types);

    for (i = 0; i < num_types; i++)
    {
        if (types[i] == 0)           /* Work around broken extensions */
        {
            break;
        }
        ammonite_module_add_type (types[i]);
    }
}

static void
ammonite_module_load_file (const char *filename)
{
    AmmoniteModule *module;

    module = g_object_new (AMMONITE_TYPE_MODULE, NULL);
    module->path = g_strdup (filename);

    if (g_type_module_use (G_TYPE_MODULE (module)))
    {
        add_module_objects (module);
        installed_modules = g_list_prepend (installed_modules, module);
        return;
    }
    else
    {
        g_object_unref (module);
    }
}

char *
ammonite_module_get_installed_module_names (void)
{
    g_autoptr (GStrvBuilder) builder = g_strv_builder_new ();
    g_auto (GStrv) names = NULL;

    for (GList *l = installed_modules; l != NULL; l = l->next)
    {
        AmmoniteModule *module = l->data;
        g_strv_builder_add (builder, module->path);
    }

    names = g_strv_builder_end (builder);

    return g_strjoinv ("\n", names);
}

static void
load_module_dir (const char *dirname)
{
    GDir *dir;

    dir = g_dir_open (dirname, 0, NULL);

    if (dir)
    {
        const char *name;

        while ((name = g_dir_read_name (dir)))
        {
            if (g_str_has_suffix (name, "." G_MODULE_SUFFIX))
            {
                char *filename;

                filename = g_build_filename (dirname,
                                             name,
                                             NULL);
                ammonite_module_load_file (filename);
                g_free (filename);
            }
        }

        g_dir_close (dir);
    }
}

void
ammonite_module_teardown (void)
{
    GList *l, *next;

    for (l = module_objects; l != NULL; l = next)
    {
        next = l->next;
        g_object_unref (l->data);
    }

    g_list_free (module_objects);

    for (l = installed_modules; l != NULL; l = l->next)
    {
        AmmoniteModule *module = l->data;
        module->shutdown ();
    }

    /* We can't actually free the modules themselves. */
    g_clear_pointer (&installed_modules, g_list_free);
}

void
ammonite_module_setup (void)
{
    static gboolean initialized = FALSE;
    const gchar *disable_plugins;

    disable_plugins = g_getenv ("AMMONITE_DISABLE_PLUGINS");
    if (g_strcmp0 (disable_plugins, "TRUE") == 0)
    {
        /* Troublingshooting envvar is set to disable extensions */
        return;
    }

    if (!initialized)
    {
        initialized = TRUE;

        load_module_dir (AMMONITE_EXTENSIONDIR);
    }
}

GList *
ammonite_module_get_extensions_for_type (GType type)
{
    GList *l;
    GList *ret = NULL;

    for (l = module_objects; l != NULL; l = l->next)
    {
        if (G_TYPE_CHECK_INSTANCE_TYPE (G_OBJECT (l->data),
                                        type))
        {
            g_object_ref (l->data);
            ret = g_list_prepend (ret, l->data);
        }
    }

    return ret;
}

void
ammonite_module_extension_list_free (GList *extensions)
{
    GList *l, *next;

    for (l = extensions; l != NULL; l = next)
    {
        next = l->next;
        g_object_unref (l->data);
    }
    g_list_free (extensions);
}

void
ammonite_module_add_type (GType type)
{
    GObject *object;

    object = g_object_new (type, NULL);
    g_object_weak_ref (object,
                       (GWeakNotify) module_object_weak_notify,
                       NULL);

    module_objects = g_list_prepend (module_objects, object);
}
