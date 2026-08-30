/*
   mcstruct - def-file search path, aliases and settings

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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

#include <config.h>

#include <stdio.h>
#include <string.h>

#include "lib/global.h"
#include "lib/mcconfig.h"

#include "slv_load.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#ifndef MC_PLUGIN_DIR
#define MC_PLUGIN_DIR "/usr/lib/mc/panel-plugins/mcstruct"
#endif

#define ALIAS_FILE "stl.als"
#define INI_FILE   "mcstruct.ini"
#define INI_GROUP  "mcstruct"

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static char **override_dirs = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
find_in_dirs (char **dirs, const char *name)
{
    int i;

    for (i = 0; dirs[i] != NULL; i++)
    {
        char *path = g_build_filename (dirs[i], name, (char *) NULL);

        if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
            return path;
        g_free (path);
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
with_stl_ext (const char *name)
{
    gsize len = strlen (name);

    if (len > 4 && g_ascii_strcasecmp (name + len - 4, ".stl") == 0)
        return g_strdup (name);
    return g_strconcat (name, ".stl", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* "@offset:hexbytes" against the first bytes of the file */
static gboolean
match_signature (const char *pat, const unsigned char *head, gsize head_len)
{
    char *end;
    gint64 off;
    const char *hex;
    gsize i, n;

    off = g_ascii_strtoll (pat + 1, &end, 0);
    if (end == pat + 1 || *end != ':' || off < 0)
        return FALSE;
    hex = end + 1;
    n = strlen (hex);
    if (n == 0 || n % 2 != 0)
        return FALSE;
    if ((gsize) off + n / 2 > head_len)
        return FALSE;
    for (i = 0; i < n / 2; i++)
    {
        int hi = g_ascii_xdigit_value (hex[2 * i]);
        int lo = g_ascii_xdigit_value (hex[2 * i + 1]);

        if (hi < 0 || lo < 0 || head[off + i] != (unsigned char) (hi * 16 + lo))
            return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* stl.als: "target.stl: *.dll *.drv @0:7F454C46"; name patterns match the lower-cased
   base name, @offset:hex patterns the first bytes of the file */
static char *
match_alias_file (const char *path, const char *lower_base, const unsigned char *head,
                  gsize head_len)
{
    char *data = NULL;
    char **lines;
    int i;
    char *result = NULL;

    if (!g_file_get_contents (path, &data, NULL, NULL))
        return NULL;
    lines = g_strsplit (data, "\n", -1);
    g_free (data);

    for (i = 0; lines[i] != NULL && result == NULL; i++)
    {
        char *line = g_strstrip (lines[i]);
        char *colon, **pats;
        int j;

        if (line[0] == '\0' || line[0] == ';' || line[0] == '[')
            continue;
        colon = strchr (line, ':');
        if (colon == NULL)
            continue;
        *colon = '\0';
        pats = g_strsplit_set (colon + 1, " \t", -1);
        for (j = 0; pats[j] != NULL; j++)
        {
            char *pat;

            if (pats[j][0] == '\0')
                continue;
            if (pats[j][0] == '@')
            {
                if (head != NULL && match_signature (pats[j], head, head_len))
                {
                    result = g_strdup (g_strstrip (line));
                    break;
                }
                continue;
            }
            pat = g_ascii_strdown (pats[j], -1);
            if (g_pattern_match_simple (pat, lower_base))
            {
                result = g_strdup (g_strstrip (line));
                g_free (pat);
                break;
            }
            g_free (pat);
        }
        g_strfreev (pats);
    }
    g_strfreev (lines);
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* one printf conversion for a double: %[flags][width][.precision](f|F|e|E|g|G|a|A) */
gboolean
slv_float_format_valid (const char *fmt)
{
    const char *p;

    if (fmt == NULL || fmt[0] != '%')
        return FALSE;
    p = fmt + 1;
    while (*p != '\0' && strchr ("-+ #0", *p) != NULL)
        p++;
    while (g_ascii_isdigit (*p))
        p++;
    if (*p == '.')
    {
        p++;
        while (g_ascii_isdigit (*p))
            p++;
    }
    if (*p == '\0' || strchr ("fFeEgGaA", *p) == NULL)
        return FALSE;
    return p[1] == '\0';
}

/* --------------------------------------------------------------------------------------------- */

void
slv_load_set_search_dirs (const char *const *dirs)
{
    g_strfreev (override_dirs);
    override_dirs = dirs != NULL ? g_strdupv ((char **) dirs) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

char **
slv_load_search_dirs (void)
{
    GPtrArray *dirs;

    if (override_dirs != NULL)
        return g_strdupv (override_dirs);

    dirs = g_ptr_array_new ();
    g_ptr_array_add (dirs, g_build_filename (mc_config_get_path (), "mcstruct", (char *) NULL));
    if (mc_global.sysconfig_dir != NULL)
        g_ptr_array_add (dirs,
                         g_build_filename (mc_global.sysconfig_dir, "mcstruct", (char *) NULL));
    g_ptr_array_add (dirs, g_build_filename (MC_PLUGIN_DIR, "data", (char *) NULL));
    g_ptr_array_add (dirs, NULL);
    return (char **) g_ptr_array_free (dirs, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
slv_load_find_def (const char *file_name, const char *hint)
{
    char **dirs = slv_load_search_dirs ();
    char *result = NULL;
    char *base, *lower_base;
    unsigned char head[4096];
    gsize head_len = 0;
    int i;

    if (hint != NULL && hint[0] != '\0')
    {
        const char *h = hint;
        char *name;

        if (g_str_has_prefix (h, "mcstruct-") || g_str_has_prefix (h, "mcstruct_"))
            h += 9;
        name = with_stl_ext (h);
        result = find_in_dirs (dirs, name);
        g_free (name);
        if (result == NULL && g_path_is_absolute (hint)
            && g_file_test (hint, G_FILE_TEST_IS_REGULAR))
            result = g_strdup (hint);
        if (result != NULL)
            goto done;
    }

    if (file_name == NULL)
        goto done;

    base = g_path_get_basename (file_name);
    lower_base = g_ascii_strdown (base, -1);
    g_free (base);

    /* the first bytes for signature patterns */
    {
        FILE *f = fopen (file_name, "rb");

        if (f != NULL)
        {
            head_len = fread (head, 1, sizeof (head), f);
            fclose (f);
        }
    }

    /* aliases and signatures */
    for (i = 0; dirs[i] != NULL && result == NULL; i++)
    {
        char *als = g_build_filename (dirs[i], ALIAS_FILE, (char *) NULL);
        char *target;

        target = match_alias_file (als, lower_base, head_len > 0 ? head : NULL, head_len);
        g_free (als);
        if (target != NULL)
        {
            result = find_in_dirs (dirs, target);
            g_free (target);
        }
    }

    /* <ext>.stl */
    if (result == NULL)
    {
        const char *dot = strrchr (lower_base, '.');

        if (dot != NULL && dot[1] != '\0')
        {
            char *name = g_strconcat (dot + 1, ".stl", (char *) NULL);

            result = find_in_dirs (dirs, name);
            g_free (name);
        }
    }

    if (result == NULL)
        result = find_in_dirs (dirs, "stl.def");

    g_free (lower_base);

done:
    g_strfreev (dirs);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

GPtrArray *
slv_load_list_defs (void)
{
    char **dirs = slv_load_search_dirs ();
    GPtrArray *result = g_ptr_array_new_with_free_func (g_free);
    GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    int i;

    for (i = 0; dirs[i] != NULL; i++)
    {
        GDir *d = g_dir_open (dirs[i], 0, NULL);
        const char *name;
        GPtrArray *here;
        guint j;

        if (d == NULL)
            continue;
        here = g_ptr_array_new_with_free_func (g_free);
        while ((name = g_dir_read_name (d)) != NULL)
        {
            gsize len = strlen (name);
            char *lower;

            if (len <= 4 || g_ascii_strcasecmp (name + len - 4, ".stl") != 0)
                continue;
            lower = g_ascii_strdown (name, -1);
            if (g_hash_table_contains (seen, lower))
            {
                g_free (lower);
                continue;
            }
            g_hash_table_add (seen, lower);
            g_ptr_array_add (here, g_build_filename (dirs[i], name, (char *) NULL));
        }
        g_dir_close (d);
        g_ptr_array_sort_values (here, (GCompareFunc) g_ascii_strcasecmp);
        for (j = 0; j < here->len; j++)
            g_ptr_array_add (result, g_strdup (g_ptr_array_index (here, j)));
        g_ptr_array_free (here, TRUE);
    }

    g_hash_table_destroy (seen);
    g_strfreev (dirs);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
slv_settings_load (slv_settings_t *s)
{
    char **dirs = slv_load_search_dirs ();
    char *path;

    s->tree_lines = 14;
    s->def_layout = 0;
    s->def_width = 45;
    s->hex_lines = 4;
    s->def_lines = 0;
    s->name_width = 16;
    s->offset_column = 2;
    s->grid_rows = 2;
    s->show_hidden = FALSE;
    s->lazy_rows = 64;
    s->fragment_days = 7;
    s->float_format = g_strdup ("%g");

    path = find_in_dirs (dirs, INI_FILE);
    if (path != NULL)
    {
        mc_config_t *cfg = mc_config_init (path, TRUE);

        if (cfg != NULL)
        {
            char *fmt, *oc;

            s->tree_lines = mc_config_get_int (cfg, INI_GROUP, "TreeLines", s->tree_lines);
            s->def_width = mc_config_get_int (cfg, INI_GROUP, "DefWidth", s->def_width);
            oc = mc_config_get_string_raw (cfg, INI_GROUP, "DefLayout", "auto");
            if (g_ascii_strcasecmp (oc, "right") == 0)
                s->def_layout = 1;
            else if (g_ascii_strcasecmp (oc, "bottom") == 0)
                s->def_layout = 2;
            else
                s->def_layout = 0;
            g_free (oc);
            s->hex_lines = mc_config_get_int (cfg, INI_GROUP, "HexLines", s->hex_lines);
            s->def_lines = mc_config_get_int (cfg, INI_GROUP, "DefLines", s->def_lines);
            s->name_width = mc_config_get_int (cfg, INI_GROUP, "NameWidth", s->name_width);
            s->show_hidden = mc_config_get_bool (cfg, INI_GROUP, "ShowHiddenStructures", FALSE);
            s->lazy_rows = mc_config_get_int (cfg, INI_GROUP, "LazyRows", (int) s->lazy_rows);
            s->fragment_days = mc_config_get_int (cfg, INI_GROUP, "FragmentDays", s->fragment_days);
            oc = mc_config_get_string_raw (cfg, INI_GROUP, "OffsetColumn", "global");
            if (g_ascii_strcasecmp (oc, "none") == 0)
                s->offset_column = 0;
            else if (g_ascii_strcasecmp (oc, "local") == 0)
                s->offset_column = 1;
            else
                s->offset_column = 2;
            g_free (oc);
            oc = mc_config_get_string_raw (cfg, INI_GROUP, "GridRowColumn", "offset");
            if (g_ascii_strcasecmp (oc, "none") == 0)
                s->grid_rows = 0;
            else if (g_ascii_strcasecmp (oc, "number") == 0)
                s->grid_rows = 1;
            else
                s->grid_rows = 2;
            g_free (oc);
            fmt = mc_config_get_string_raw (cfg, INI_GROUP, "FloatingPointFormat", "%g");
            if (slv_float_format_valid (fmt))
            {
                g_free (s->float_format);
                s->float_format = fmt;
            }
            else
                g_free (fmt);
            mc_config_deinit (cfg);
        }
        g_free (path);
    }

    if (s->tree_lines < 0)
        s->tree_lines = 0;
    s->def_width = CLAMP (s->def_width, 20, 80);
    if (s->hex_lines < 0)
        s->hex_lines = 0;
    if (s->def_lines < 0)
        s->def_lines = 0;
    if (s->name_width < 4)
        s->name_width = 4;
    if (s->name_width > 40)
        s->name_width = 40;
    if (s->lazy_rows < 1)
        s->lazy_rows = 1;
    if (s->fragment_days < 0)
        s->fragment_days = 0;
    g_strfreev (dirs);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
slv_settings_save (const slv_settings_t *s)
{
    char **dirs = slv_load_search_dirs ();
    char *path;
    mc_config_t *cfg;
    gboolean ok;
    static const char *const offset_names[] = { "none", "local", "global" };

    if (dirs[0] == NULL)
    {
        g_strfreev (dirs);
        return FALSE;
    }
    if (g_mkdir_with_parents (dirs[0], 0700) != 0)
    {
        g_strfreev (dirs);
        return FALSE;
    }
    path = g_build_filename (dirs[0], INI_FILE, (char *) NULL);
    g_strfreev (dirs);

    cfg = mc_config_init (path, FALSE);
    if (cfg == NULL)
    {
        g_free (path);
        return FALSE;
    }
    mc_config_set_int (cfg, INI_GROUP, "TreeLines", s->tree_lines);
    mc_config_set_string_raw (cfg, INI_GROUP, "DefLayout",
                              s->def_layout == 1       ? "right"
                                  : s->def_layout == 2 ? "bottom"
                                                       : "auto");
    mc_config_set_int (cfg, INI_GROUP, "DefWidth", s->def_width);
    mc_config_set_int (cfg, INI_GROUP, "HexLines", s->hex_lines);
    mc_config_set_int (cfg, INI_GROUP, "DefLines", s->def_lines);
    mc_config_set_int (cfg, INI_GROUP, "NameWidth", s->name_width);
    mc_config_set_string_raw (cfg, INI_GROUP, "OffsetColumn",
                              offset_names[CLAMP (s->offset_column, 0, 2)]);
    mc_config_set_string_raw (cfg, INI_GROUP, "GridRowColumn",
                              s->grid_rows == 0       ? "none"
                                  : s->grid_rows == 1 ? "number"
                                                      : "offset");
    mc_config_set_bool (cfg, INI_GROUP, "ShowHiddenStructures", s->show_hidden);
    mc_config_set_int (cfg, INI_GROUP, "LazyRows", (int) s->lazy_rows);
    mc_config_set_int (cfg, INI_GROUP, "FragmentDays", s->fragment_days);
    mc_config_set_string_raw (cfg, INI_GROUP, "FloatingPointFormat",
                              s->float_format != NULL ? s->float_format : "%g");
    ok = mc_config_save_file (cfg, NULL);
    mc_config_deinit (cfg);
    g_free (path);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

void
slv_settings_free (slv_settings_t *s)
{
    g_free (s->float_format);
    s->float_format = NULL;
}

/* --------------------------------------------------------------------------------------------- */
