/*
   Panel plugin mcpeek for the M-Commander
   C# text through an external decompiler

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
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/plugin-prefs.h"
#include "lib/util.h"

#include "mcpeek-cs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MCPEEK_INI "mcpeek.ini"

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static char *cs_command = NULL;
static gboolean cs_command_resolved = FALSE;

/* "<assembly>\n<mtime>\n<type>" -> decompiled text */
static GHashTable *cs_cache = NULL;

/* assembly and decompiler -> the directory its project was exported to */
static GHashTable *project_dirs = NULL;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
config_path (void)
{
    char *path;

    path = g_build_filename (mc_config_get_path (), MCPEEK_INI, (char *) NULL);
    if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;
    g_free (path);

    return g_build_filename (mc_global.sysconfig_dir, MCPEEK_INI, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* Run @argv, return its standard output, or NULL with @error set.  The
   decompiler writes diagnostics to stderr and the source to stdout, so a
   non-empty stderr is not by itself a failure. */
static char *
run_capture (char **argv, GError **error)
{
    char *out = NULL;
    char *err = NULL;
    int status;
    gboolean ok;

    ok = g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &out, &err, &status,
                       error);
    if (!ok)
    {
        g_free (out);
        g_free (err);
        return NULL;
    }

    if (!g_spawn_check_wait_status (status, NULL))
    {
        g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "%s",
                     err != NULL && *err != '\0' ? err : _ ("the decompiler failed"));
        g_free (out);
        g_free (err);
        return NULL;
    }

    g_free (err);
    return out;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* The PDB numbers the lines of the decompiler's own output, which begins one
   line above what -p writes to disk.  Exported sources are therefore shifted
   by one against their own symbols, and every breakpoint set by eye would
   land on the next statement.  One line at the top of each file removes the
   difference, and says why it is there. */
static void
align_sources_to_pdb (const char *dir)
{
    GDir *d;
    const char *name;

    d = g_dir_open (dir, 0, NULL);
    if (d == NULL)
        return;

    while ((name = g_dir_read_name (d)) != NULL)
    {
        char *path;

        path = g_build_filename (dir, name, (char *) NULL);

        if (g_file_test (path, G_FILE_TEST_IS_DIR))
            align_sources_to_pdb (path);
        else if (g_str_has_suffix (name, ".cs"))
        {
            char *contents;
            gsize len;

            if (g_file_get_contents (path, &contents, &len, NULL))
            {
                char *shifted;

                shifted = g_strconcat ("// line numbers match the generated PDB\n", contents,
                                       (char *) NULL);
                (void) g_file_set_contents (path, shifted, -1, NULL);
                g_free (shifted);
                g_free (contents);
            }
        }

        g_free (path);
    }

    g_dir_close (d);
}

/* --------------------------------------------------------------------------------------------- */

const char *
mcpeek_cs_command (void)
{
    char *path;
    char *found;

    if (cs_command_resolved)
        return cs_command;

    cs_command_resolved = TRUE;

    path = config_path ();
    cs_command = mc_plugin_prefs_read_config_string (path, "mcpeek", "decompiler");
    g_free (path);

    if (cs_command != NULL)
        return cs_command;

    found = g_find_program_in_path ("ilspycmd");
    if (found != NULL)
    {
        cs_command = found;
        return cs_command;
    }

    /* a dotnet global tool is not on PATH unless the user put it there */
    path = g_build_filename (g_get_home_dir (), ".dotnet", "tools", "ilspycmd", (char *) NULL);
    if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE))
        cs_command = path;
    else
        g_free (path);

    return cs_command;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_cs_version (void)
{
    const char *cmd = mcpeek_cs_command ();
    char *argv[3];
    char *out;
    char *nl;

    if (cmd == NULL)
        return NULL;

    argv[0] = (char *) cmd;
    argv[1] = (char *) "--version";
    argv[2] = NULL;

    out = run_capture (argv, NULL);
    if (out == NULL)
        return NULL;

    nl = strchr (out, '\n');
    if (nl != NULL)
        *nl = '\0';

    return out;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_cs_type (const char *assembly, const char *type_full_name, GError **error)
{
    const char *cmd = mcpeek_cs_command ();
    char *argv[7];
    char *key;
    char *out;
    struct stat st;

    if (cmd == NULL)
    {
        g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT, "%s",
                     _ ("no decompiler configured"));
        return NULL;
    }

    if (stat (assembly, &st) != 0)
        st.st_mtime = 0;

    key = g_strdup_printf ("%s\n%s\n%ld\n%s", cmd, assembly, (long) st.st_mtime, type_full_name);

    if (cs_cache != NULL)
    {
        const char *hit = g_hash_table_lookup (cs_cache, key);

        if (hit != NULL)
        {
            g_free (key);
            return g_strdup (hit);
        }
    }

    argv[0] = (char *) cmd;
    argv[1] = (char *) "--disable-updatecheck";
    argv[2] = (char *) "-t";
    argv[3] = (char *) type_full_name;
    argv[4] = (char *) assembly;
    argv[5] = NULL;

    out = run_capture (argv, error);
    if (out == NULL)
    {
        g_free (key);
        return NULL;
    }

    if (cs_cache == NULL)
        cs_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert (cs_cache, key, g_strdup (out));

    return out;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcpeek_cs_export (const char *assembly, const char *dir, gboolean align, GError **error)
{
    const char *cmd = mcpeek_cs_command ();
    char *argv[8];
    char *out;

    if (cmd == NULL)
    {
        g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT, "%s",
                     _ ("no decompiler configured"));
        return FALSE;
    }

    argv[0] = (char *) cmd;
    argv[1] = (char *) "--disable-updatecheck";
    argv[2] = (char *) "-p";
    argv[3] = (char *) "-o";
    argv[4] = (char *) dir;
    argv[5] = (char *) assembly;
    argv[6] = NULL;

    out = run_capture (argv, error);
    if (out == NULL)
        return FALSE;
    g_free (out);

    /* -p and -genpdb are exclusive: -p wins and writes no PDB, so the symbols
       that make the exported source debuggable need a second run.  Their
       absence does not spoil the export. */
    argv[2] = (char *) "-genpdb";
    argv[3] = (char *) "-o";
    argv[4] = (char *) dir;
    argv[5] = (char *) assembly;
    argv[6] = NULL;

    out = run_capture (argv, NULL);
    if (out != NULL)
    {
        g_free (out);
        if (align)
            align_sources_to_pdb (dir);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

int
mcpeek_cs_find_member (const char *text, const char *member)
{
    const char *p = text;
    int line = 1;
    int statement_line = 0;
    size_t len;

    if (text == NULL || member == NULL || *member == '\0')
        return 0;

    len = strlen (member);

    while (*p != '\0')
    {
        const char *eol = strchr (p, '\n');
        size_t n = eol != NULL ? (size_t) (eol - p) : strlen (p);
        const char *hit = g_strstr_len (p, n, member);
        size_t last = n;

        while (last > 0 && (p[last - 1] == ' ' || p[last - 1] == '\r'))
            last--;

        /* a declaration, not a use: the name has to be preceded by a space
           and followed by something that ends a name */
        if (hit != NULL && hit != p && hit[-1] == ' '
            && (hit[len] == '(' || hit[len] == ' ' || hit[len] == '<' || hit[len] == ';'))
        {
            /* a line that ends in ';' is a statement, so most likely a call;
               it is kept in case the declaration is one, as an abstract
               method's is */
            if (last == 0 || p[last - 1] != ';' || hit[len] == ';')
                return line;
            if (statement_line == 0)
                statement_line = line;
        }

        if (eol == NULL)
            break;
        p = eol + 1;
        line++;
    }

    return statement_line;
}

/* --------------------------------------------------------------------------------------------- */

void
mcpeek_cs_shutdown (void)
{
    if (cs_cache != NULL)
    {
        g_hash_table_destroy (cs_cache);
        cs_cache = NULL;
    }
    if (project_dirs != NULL)
    {
        g_hash_table_destroy (project_dirs);
        project_dirs = NULL;
    }

    MC_PTR_FREE (cs_command);
    cs_command_resolved = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

const char *
mcpeek_cs_project_dir (const char *assembly)
{
    const char *cmd = mcpeek_cs_command ();
    struct stat st;
    char *key;
    char *dir;

    if (cmd == NULL)
        return NULL;

    if (stat (assembly, &st) != 0)
        st.st_mtime = 0;

    /* another decompiler writes another project */
    key = g_strdup_printf ("%s\n%s\n%ld", cmd, assembly, (long) st.st_mtime);

    if (project_dirs == NULL)
        project_dirs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    dir = g_hash_table_lookup (project_dirs, key);
    if (dir != NULL)
    {
        g_free (key);
        return dir;
    }

    {
        char *base;
        char *hash;

        base = g_path_get_basename (assembly);
        hash = g_compute_checksum_for_string (G_CHECKSUM_SHA256, key, -1);
        dir = g_build_filename (g_get_user_cache_dir (), "mc-mcpeek", hash, base, (char *) NULL);
        g_free (base);
        g_free (hash);
    }

    if (g_mkdir_with_parents (dir, 0700) != 0 || !mcpeek_cs_export (assembly, dir, FALSE, NULL))
    {
        g_free (dir);
        g_free (key);
        return NULL;
    }

    g_hash_table_insert (project_dirs, key, dir);

    return dir;
}
