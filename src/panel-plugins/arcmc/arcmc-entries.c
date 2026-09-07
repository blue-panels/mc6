/*
   arcmc panel plugin - the entries of an archive: lookup, links, release

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

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

#include <string.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "arcmc-types.h"
#include "archive-io.h"

/* --------------------------------------------------------------------------------------------- */

void
arcmc_entry_free (gpointer p)
{
    arcmc_entry_t *e = (arcmc_entry_t *) p;

    g_free (e->full_path);
    g_free (e->name);
    g_free (e->linkname);
    g_free (e->link_path);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

const arcmc_entry_t *
arcmc_find_entry (GPtrArray *entries, const char *full_path)
{
    guint i;

    if (entries == NULL)
        return NULL;

    for (i = 0; i < entries->len; i++)
    {
        const arcmc_entry_t *e = (const arcmc_entry_t *) g_ptr_array_index (entries, i);

        if (strcmp (e->full_path, full_path) == 0)
            return e;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* The path inside the archive, without a leading slash, with "." and ".." folded;
   "" is the root. */
static char *
arcmc_path_normalize (const char *path)
{
    char **parts;
    GPtrArray *kept;
    GString *out;
    guint i;

    parts = g_strsplit (path, "/", -1);
    kept = g_ptr_array_new ();

    for (i = 0; parts[i] != NULL; i++)
    {
        if (parts[i][0] == '\0' || strcmp (parts[i], ".") == 0)
            continue;
        if (strcmp (parts[i], "..") == 0)
        {
            if (kept->len > 0)
                g_ptr_array_remove_index (kept, kept->len - 1);
            continue;
        }
        g_ptr_array_add (kept, parts[i]);
    }

    out = g_string_new ("");
    for (i = 0; i < kept->len; i++)
    {
        if (i > 0)
            g_string_append_c (out, '/');
        g_string_append (out, (const char *) g_ptr_array_index (kept, i));
    }

    g_ptr_array_free (kept, TRUE);
    g_strfreev (parts);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Every symbolic link among @entries learns what it leads to inside the archive:
   a link to a directory, a link to a file (link_path names it), or nothing. */
void
arcmc_resolve_links (GPtrArray *entries)
{
    GHashTable *by_path;
    guint i;

    if (entries == NULL)
        return;

    by_path = g_hash_table_new (g_str_hash, g_str_equal);
    for (i = 0; i < entries->len; i++)
    {
        arcmc_entry_t *e = (arcmc_entry_t *) g_ptr_array_index (entries, i);

        g_hash_table_insert (by_path, e->full_path, e);
    }

    for (i = 0; i < entries->len; i++)
    {
        arcmc_entry_t *e = (arcmc_entry_t *) g_ptr_array_index (entries, i);
        const arcmc_entry_t *cur = e;
        char *path = NULL;
        int hops;

        if (!S_ISLNK (e->mode))
            continue;

        g_free (e->link_path);
        e->link_path = NULL;
        e->link_to_dir = FALSE;
        e->stale_link = FALSE;

        for (hops = 0; hops < 32 && cur != NULL && S_ISLNK (cur->mode); hops++)
        {
            const char *target = cur->linkname;
            char *joined;

            if (target == NULL || target[0] == '\0')
            {
                cur = NULL;
                break;
            }

            if (target[0] == '/')
                joined = g_strdup (target);
            else
            {
                const char *slash = strrchr (cur->full_path, '/');

                if (slash == NULL)
                    joined = g_strdup (target);
                else
                    joined = g_strdup_printf ("%.*s/%s", (int) (slash - cur->full_path),
                                              cur->full_path, target);
            }

            g_free (path);
            path = arcmc_path_normalize (joined);
            g_free (joined);

            cur = path[0] == '\0' ? NULL
                                  : (const arcmc_entry_t *) g_hash_table_lookup (by_path, path);
        }

        if (cur == NULL || S_ISLNK (cur->mode))
        {
            e->stale_link = TRUE;
            g_free (path);
            continue;
        }

        e->link_to_dir = S_ISDIR (cur->mode);
        e->link_path = path;
    }

    g_hash_table_destroy (by_path);
}

/* --------------------------------------------------------------------------------------------- */
