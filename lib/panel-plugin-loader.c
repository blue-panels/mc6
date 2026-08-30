/*
   Dynamic panel plugin loader.

   Copyright (C) 2025-2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2025-2026.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file panel-plugin-loader.c
 *  \brief Source: dynamic panel plugin loader
 *
 *  Scans MC_PANEL_PLUGINS_DIR for shared objects exporting
 *  mc_panel_plugin_register(), loads them, and registers the returned
 *  mc_panel_plugin_t descriptor via mc_panel_plugin_add().
 */

#include <config.h>

#include <stdio.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/editor-plugin.h"
#include "lib/panel-plugin.h"
#include "lib/widget/hexstrip.h"  // named in mc_plugin_widgets below

#ifdef HAVE_GMODULE

#include <gmodule.h>

/*** global variables ****************************************************************************/

/* The widgets a plugin puts on a dialog and mc itself never does.
 *
 * lib/ is linked as a static archive, and a linker takes out of an archive only what the program
 * refers to.  A widget no part of mc uses is left behind, and the plugin that wanted it then fails
 * to load with an undefined symbol.  Naming one here is what keeps it in.
 */
void (*const mc_plugin_widgets[]) (void) = {
    (void (*) (void)) hexstrip_new,
    NULL,
};

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static GPtrArray *panel_plugin_modules = NULL;
static gboolean editor_plugins_loaded = FALSE;
static GPtrArray *editor_plugin_modules = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
module_filename_has_native_suffix (const gchar *filename)
{
    if (filename == NULL)
        return FALSE;

    return g_str_has_suffix (filename, ".so") || g_str_has_suffix (filename, ".dylib")
        || g_str_has_suffix (filename, ".bundle") || g_str_has_suffix (filename, ".dll");
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Try to load a single .so from the given directory.
 */
static void
mc_panel_plugin_try_load (const gchar *so_path, const gchar *filename)
{
    GModule *module;
    mc_panel_plugin_register_fn register_fn;
    const mc_panel_plugin_t *plugin;

    /* Eager binding: a plugin from an older install may reference a symbol this
       build no longer exports; lazy binding would crash mc later instead. */
    module = g_module_open (so_path, G_MODULE_BIND_LOCAL);
    if (module == NULL)
    {
        fprintf (stderr, "Panel plugin %s not loaded: %s\n", filename, g_module_error ());
        return;
    }

    if (!g_module_symbol (module, MC_PANEL_PLUGIN_ENTRY, (gpointer *) &register_fn))
    {
        fprintf (stderr, "Panel plugin %s: symbol %s not found\n", filename, MC_PANEL_PLUGIN_ENTRY);
        g_module_close (module);
        return;
    }

    plugin = register_fn ();
    if (plugin == NULL || !mc_panel_plugin_add (plugin))
    {
        /* register() may allocate module-global state even when preferences or
           validation reject the descriptor. Only the current ABI can safely
           expose the shutdown field. A duplicate name is left alone: the same
           .so reached by another path shares its globals with the live plugin. */
        if (plugin != NULL && plugin->api_version == MC_PANEL_PLUGIN_API_VERSION
            && plugin->shutdown != NULL && plugin->name != NULL
            && mc_panel_plugin_find_by_name (plugin->name) == NULL)
            plugin->shutdown ();
        /* duplicate name from user dir overriding system dir is expected, stay silent */
        g_module_close (module);
        return;
    }

    g_module_make_resident (module); /* prevent unload - plugin descriptor lives in .so */
    g_ptr_array_add (panel_plugin_modules, module);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Scan plugins_dir for subdirectories containing .so files.
 * Each plugin lives in its own subdirectory (e.g. panel-plugins/s3/mc-panel-s3.so).
 */
/* one package directory: every native module in it */
static void
mc_panel_plugins_load_package (const gchar *entry_path, const gchar *entry_name)
{
    GDir *subdir;
    const gchar *sub_name;

    if (!g_file_test (entry_path, G_FILE_TEST_IS_DIR))
        return;

    /* A user package shadows the system package with the same ID.  Do not
       even open the duplicate: its register() callback may have process-wide
       side effects before mc_panel_plugin_add() can reject it. */
    if (mc_panel_plugin_find_by_name (entry_name) != NULL)
        return;

    subdir = g_dir_open (entry_path, 0, NULL);
    if (subdir == NULL)
        return;
    while ((sub_name = g_dir_read_name (subdir)) != NULL)
    {
        if (module_filename_has_native_suffix (sub_name))
        {
            gchar *so_path;

            so_path = g_build_filename (entry_path, sub_name, (char *) NULL);
            mc_panel_plugin_try_load (so_path, sub_name);
            g_free (so_path);
        }
    }
    g_dir_close (subdir);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_panel_plugins_load_from_dir (const gchar *plugins_dir)
{
    GDir *dir;
    const gchar *entry_name;

    dir = g_dir_open (plugins_dir, 0, NULL);
    if (dir == NULL)
        return;

    while ((entry_name = g_dir_read_name (dir)) != NULL)
    {
        gchar *entry_path;

        entry_path = g_build_filename (plugins_dir, entry_name, (char *) NULL);
        mc_panel_plugins_load_package (entry_path, entry_name);
        g_free (entry_path);
    }

    g_dir_close (dir);
}

/* --------------------------------------------------------------------------------------------- */

static gchar *
mc_panel_plugins_user_dir (void)
{
    return g_build_filename (g_get_home_dir (), ".local", "lib", "mc", "panel-plugins",
                             (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_panel_plugins_load (void)
{
    gchar *system_dir;
    gchar *user_dir;

    if (panel_plugin_modules == NULL)
        panel_plugin_modules = g_ptr_array_new ();

    /* Load user packages first so they can shadow system packages. */
    user_dir = mc_panel_plugins_user_dir ();
    mc_panel_plugins_load_from_dir (user_dir);
    g_free (user_dir);

    system_dir = g_build_filename (MC_PANEL_PLUGINS_DIR, (char *) NULL);
    mc_panel_plugins_load_from_dir (system_dir);
    g_free (system_dir);
}

/* --------------------------------------------------------------------------------------------- */

/* Load one package by name, the user copy first; the rest stay unloaded. */
const mc_panel_plugin_t *
mc_panel_plugin_load_named (const char *name)
{
    gchar *dir, *path;
    const mc_panel_plugin_t *plugin;

    plugin = mc_panel_plugin_find_by_name (name);
    if (plugin != NULL)
        return plugin;

    if (panel_plugin_modules == NULL)
        panel_plugin_modules = g_ptr_array_new ();

    dir = mc_panel_plugins_user_dir ();
    path = g_build_filename (dir, name, (char *) NULL);
    mc_panel_plugins_load_package (path, name);
    g_free (path);
    g_free (dir);

    if (mc_panel_plugin_find_by_name (name) == NULL)
    {
        path = g_build_filename (MC_PANEL_PLUGINS_DIR, name, (char *) NULL);
        mc_panel_plugins_load_package (path, name);
        g_free (path);
    }

    return mc_panel_plugin_find_by_name (name);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_editor_plugins_load_from_dir (const gchar *plugins_dir)
{
    GDir *dir;
    const gchar *filename;

    dir = g_dir_open (plugins_dir, 0, NULL);
    if (dir == NULL)
        return;

    while ((filename = g_dir_read_name (dir)) != NULL)
    {
        GModule *module;
        mc_editor_plugin_register_fn register_fn;
        const mc_editor_plugin_t *plugin;
        gchar *path;

        if (!module_filename_has_native_suffix (filename))
            continue;

        path = g_build_filename (plugins_dir, filename, (char *) NULL);
        module = g_module_open (path, 0);
        if (module == NULL)
        {
            fprintf (stderr, "Editor plugin %s not loaded: %s\n", filename, g_module_error ());
            g_free (path);
            continue;
        }

        if (!g_module_symbol (module, MC_EDITOR_PLUGIN_ENTRY, (gpointer *) &register_fn))
        {
            fprintf (stderr, "Editor plugin %s: symbol %s not found\n", filename,
                     MC_EDITOR_PLUGIN_ENTRY);
            g_module_close (module);
            g_free (path);
            continue;
        }

        plugin = register_fn ();
        if (plugin == NULL || !mc_editor_plugin_add (plugin))
        {
            g_module_close (module);
            g_free (path);
            continue;
        }

        g_module_make_resident (module);
        g_ptr_array_add (editor_plugin_modules, module);
        g_free (path);
    }

    g_dir_close (dir);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_editor_plugins_load (void)
{
    gchar *system_dir;
    gchar *user_dir;

    if (editor_plugins_loaded)
        return;

    editor_plugins_loaded = TRUE;
    editor_plugin_modules = g_ptr_array_new ();

    /* load from system plugin directory */
    system_dir = g_build_filename (MC_EDITOR_PLUGINS_DIR, (char *) NULL);
    mc_editor_plugins_load_from_dir (system_dir);
    g_free (system_dir);

    /* load from user plugin directory (~/.local/lib/mc/editor-plugins) */
    user_dir = g_build_filename (g_get_home_dir (), ".local", "lib", MC_USERCONF_DIR,
                                 "editor-plugins", (char *) NULL);
    mc_editor_plugins_load_from_dir (user_dir);
    g_free (user_dir);
}

/* --------------------------------------------------------------------------------------------- */

#else /* !HAVE_GMODULE */

void
mc_panel_plugins_load (void)
{
    // GModule not available - dynamic panel plugins disabled
}

/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *
mc_panel_plugin_load_named (const char *name)
{
    (void) name;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_editor_plugins_load (void)
{
    /* GModule not available - dynamic editor plugins disabled */
}

/* --------------------------------------------------------------------------------------------- */

#endif /* HAVE_GMODULE */
