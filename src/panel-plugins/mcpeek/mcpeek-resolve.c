/*
   Panel plugin mcpeek for the M-Commander
   finding the file behind an assembly reference

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "mcpeek-resolve.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope variables ************************************************************************/

/* base_dir -> char** of directories */
static GHashTable *path_cache = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
add_dir (GPtrArray *dirs, char *path)
{
    if (path == NULL)
        return;

    if (g_file_test (path, G_FILE_TEST_IS_DIR))
        g_ptr_array_add (dirs, path);
    else
        g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

/* "10.0.1" after "9.0.5": runs of digits compare as numbers. */
static gint
version_cmp (gconstpointer a, gconstpointer b)
{
    const char *x = *(const char *const *) a;
    const char *y = *(const char *const *) b;

    while (*x != '\0' && *y != '\0')
    {
        if (g_ascii_isdigit (*x) && g_ascii_isdigit (*y))
        {
            guint64 nx = g_ascii_strtoull (x, (char **) &x, 10);
            guint64 ny = g_ascii_strtoull (y, (char **) &y, 10);

            if (nx != ny)
                return nx < ny ? -1 : 1;
        }
        else
        {
            if (*x != *y)
                return (guchar) *x < (guchar) *y ? -1 : 1;
            x++;
            y++;
        }
    }

    return (*x != '\0') - (*y != '\0');
}

/* --------------------------------------------------------------------------------------------- */

/* Every version directory under @parent, newest last so that later entries
   win when a name is present in several of them. */
static void
add_version_dirs (GPtrArray *dirs, const char *parent, const char *tail)
{
    GDir *d;
    const char *name;
    GPtrArray *found;
    guint i;

    d = g_dir_open (parent, 0, NULL);
    if (d == NULL)
        return;

    found = g_ptr_array_new ();
    while ((name = g_dir_read_name (d)) != NULL)
        g_ptr_array_add (found, g_strdup (name));
    g_dir_close (d);

    g_ptr_array_sort (found, version_cmp);

    for (i = found->len; i > 0; i--)
    {
        const char *v = g_ptr_array_index (found, i - 1);

        if (tail != NULL)
            add_dir (dirs, g_build_filename (parent, v, tail, (char *) NULL));
        else
            add_dir (dirs, g_build_filename (parent, v, (char *) NULL));
    }

    g_ptr_array_free (found, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* runtimes/<rid>/lib/<tfm>/ beside an application */
static void
add_runtime_dirs (GPtrArray *dirs, const char *base_dir)
{
    char *runtimes;
    GDir *d;
    const char *rid;

    runtimes = g_build_filename (base_dir, "runtimes", (char *) NULL);
    d = g_dir_open (runtimes, 0, NULL);
    if (d == NULL)
    {
        g_free (runtimes);
        return;
    }

    while ((rid = g_dir_read_name (d)) != NULL)
    {
        char *lib;

        lib = g_build_filename (runtimes, rid, "lib", (char *) NULL);
        add_version_dirs (dirs, lib, NULL);
        g_free (lib);
    }

    g_dir_close (d);
    g_free (runtimes);
}

/* --------------------------------------------------------------------------------------------- */

static void
add_dotnet_root (GPtrArray *dirs, const char *root)
{
    char *shared, *packs;

    shared = g_build_filename (root, "shared", "Microsoft.NETCore.App", (char *) NULL);
    add_version_dirs (dirs, shared, NULL);
    g_free (shared);

    packs = g_build_filename (root, "packs", "Microsoft.NETCore.App.Ref", (char *) NULL);
    {
        GDir *d;
        const char *v;

        d = g_dir_open (packs, 0, NULL);
        if (d != NULL)
        {
            while ((v = g_dir_read_name (d)) != NULL)
            {
                char *ref;

                ref = g_build_filename (packs, v, "ref", (char *) NULL);
                add_version_dirs (dirs, ref, NULL);
                g_free (ref);
            }
            g_dir_close (d);
        }
    }
    g_free (packs);
}

/* --------------------------------------------------------------------------------------------- */

static char **
build_paths (const char *base_dir)
{
    GPtrArray *dirs;
    const char *env;

    dirs = g_ptr_array_new ();

    /* the application's own directory answers most references */
    if (base_dir != NULL)
    {
        add_dir (dirs, g_strdup (base_dir));
        add_runtime_dirs (dirs, base_dir);
    }

    env = g_getenv ("DOTNET_ROOT");
    if (env != NULL)
        add_dotnet_root (dirs, env);

    add_dotnet_root (dirs, "/usr/lib/dotnet");
    add_dotnet_root (dirs, "/usr/share/dotnet");
    add_dotnet_root (dirs, "/opt/dotnet");

    add_dir (dirs,
             g_build_filename (g_get_home_dir (), ".dotnet", "shared", "Microsoft.NETCore.App",
                               (char *) NULL));

    add_dir (dirs, g_strdup ("/usr/lib/mono/4.5"));
    add_dir (dirs, g_strdup ("/usr/lib/mono/gac"));

    g_ptr_array_add (dirs, NULL);

    return (char **) g_ptr_array_free (dirs, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* The NuGet cache is keyed by a lowercased package name, and the assembly
   sits under lib/<tfm>/; it is looked at separately because the package name
   is the assembly name only most of the time. */
static char *
find_in_nuget (const char *name)
{
    char *pkg_dir;
    char *lower;
    char *found = NULL;
    GDir *d;
    const char *name_in_dir;
    GPtrArray *versions;
    guint v;

    lower = g_ascii_strdown (name, -1);
    pkg_dir = g_build_filename (g_get_home_dir (), ".nuget", "packages", lower, (char *) NULL);
    g_free (lower);

    d = g_dir_open (pkg_dir, 0, NULL);
    if (d == NULL)
    {
        g_free (pkg_dir);
        return NULL;
    }

    /* the newest version first, as the runtime would pick it */
    versions = g_ptr_array_new_with_free_func (g_free);
    while ((name_in_dir = g_dir_read_name (d)) != NULL)
        g_ptr_array_add (versions, g_strdup (name_in_dir));
    g_dir_close (d);
    g_ptr_array_sort (versions, version_cmp);

    for (v = versions->len; found == NULL && v > 0; v--)
    {
        const char *ver = g_ptr_array_index (versions, v - 1);
        char *lib;
        GDir *ld;
        const char *tfm;

        lib = g_build_filename (pkg_dir, ver, "lib", (char *) NULL);
        ld = g_dir_open (lib, 0, NULL);
        if (ld != NULL)
        {
            while (found == NULL && (tfm = g_dir_read_name (ld)) != NULL)
            {
                char *candidate, *file;

                file = g_strconcat (name, ".dll", (char *) NULL);
                candidate = g_build_filename (lib, tfm, file, (char *) NULL);
                g_free (file);

                if (g_file_test (candidate, G_FILE_TEST_IS_REGULAR))
                    found = candidate;
                else
                    g_free (candidate);
            }
            g_dir_close (ld);
        }
        g_free (lib);
    }

    g_ptr_array_free (versions, TRUE);
    g_free (pkg_dir);

    return found;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char **
mcpeek_resolve_paths (const char *base_dir)
{
    char **dirs;
    const char *key = base_dir != NULL ? base_dir : "";

    if (path_cache == NULL)
        path_cache =
            g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_strfreev);

    dirs = g_hash_table_lookup (path_cache, key);
    if (dirs == NULL)
    {
        dirs = build_paths (base_dir);
        g_hash_table_insert (path_cache, g_strdup (key), dirs);
    }

    return g_strdupv (dirs);
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_resolve_assembly (const char *name, const char *base_dir)
{
    static const char *const exts[] = { ".dll", ".exe" };
    char **dirs;
    char *found = NULL;
    int i;

    if (name == NULL || *name == '\0')
        return NULL;

    dirs = mcpeek_resolve_paths (base_dir);

    for (i = 0; found == NULL && dirs[i] != NULL; i++)
    {
        gsize e;

        for (e = 0; found == NULL && e < G_N_ELEMENTS (exts); e++)
        {
            char *file, *candidate;

            file = g_strconcat (name, exts[e], (char *) NULL);
            candidate = g_build_filename (dirs[i], file, (char *) NULL);
            g_free (file);

            if (g_file_test (candidate, G_FILE_TEST_IS_REGULAR))
                found = candidate;
            else
                g_free (candidate);
        }
    }

    g_strfreev (dirs);

    if (found == NULL)
        found = find_in_nuget (name);

    return found;
}

/* --------------------------------------------------------------------------------------------- */

void
mcpeek_resolve_shutdown (void)
{
    if (path_cache != NULL)
    {
        g_hash_table_destroy (path_cache);
        path_cache = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */
