/*
   Internal diff viewer for the M-Commander
   Syntax colors for the two sides

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

/** \file syntax.c
 *  \brief Source: syntax colors for the diff viewer
 *
 *  The diff viewer asks for color once, while it is building its array of lines,
 *  and never during drawing: a line carries the runs covering it, and drawing is
 *  a lookup.  That keeps the scan strictly forward, which is the cheap direction.
 *
 *  A run carries a color of the rule set, not a color of the screen; ask
 *  dview_syntax_color() for something to draw with.  The two are different
 *  things, and a rule set outlives the skin it is being looked at through.
 */

#include <config.h>

#include <fcntl.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/fileloc.h"  // EDIT_SYNTAX_FILE
#include "lib/mcconfig.h"
#include "lib/skin.h"  // EDITOR_NORMAL_COLOR
#include "lib/util.h"

#include "src/syntax/tty-backend.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define READ_CHUNK 65536

/*** file scope type declarations ****************************************************************/

struct dview_syntax_t
{
    char *buf;
    off_t size;

    syntax_scanner_t *sc;
    syntax_palette_t *palette;
    gboolean line_local;
    syntax_line_local_state_t ll;
};

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** Out of range is '\n': the scanner reads one byte back and one line forward. */

static int
dview_syntax_get_byte (void *data, off_t byte_index)
{
    const dview_syntax_t *ds = (const dview_syntax_t *) data;

    return byte_index < 0 || byte_index >= ds->size ? '\n' : (unsigned char) ds->buf[byte_index];
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
dview_syntax_slurp (dview_syntax_t *ds, const char *filename)
{
    int fd;
    GString *s;
    char chunk[READ_CHUNK];
    ssize_t sz;

    fd = open (filename, O_RDONLY);
    if (fd < 0)
        return FALSE;

    s = g_string_new (NULL);
    while ((sz = read (fd, chunk, sizeof (chunk))) > 0)
        g_string_append_len (s, chunk, sz);
    close (fd);

    if (sz < 0)
    {
        g_string_free (s, TRUE);
        return FALSE;
    }

    ds->size = (off_t) s->len;
    ds->buf = g_string_free (s, FALSE);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Prepare to color a file.
 *
 * Every readable file gets a handle: mc's Syntax file ends in a catch-all rule
 * whose unknown.syntax is line-local, so a file of no recognized type still gets
 * its numbers, quotes and punctuation colored.
 *
 * @param filename file to color
 * @return a handle, or NULL if the file cannot be read or has no rules at all
 */

dview_syntax_t *
dview_syntax_open (const char *filename)
{
    dview_syntax_t *ds;
    syntax_rules_t *rules = NULL;
    syntax_select_t sel;
    char *syntax_file;
    char *error_file = NULL;
    char first[256];
    int res;

    if (filename == NULL)
        return NULL;

    ds = g_new0 (dview_syntax_t, 1);
    if (!dview_syntax_slurp (ds, filename))
    {
        g_free (ds);
        return NULL;
    }

    sel.type = NULL;
    sel.filename = filename;
    /* the first line only: the rule set is chosen by matching a regexp against it,
       and handing over the whole file would match somebody else's language */
    {
        const char *nl = memchr (ds->buf, '\n', MIN (ds->size, (off_t) sizeof (first) - 1));

        g_strlcpy (
            first, ds->buf,
            (nl != NULL ? (gsize) (nl - ds->buf) : MIN ((gsize) ds->size, sizeof (first) - 1)) + 1);
    }
    sel.first_line = first;

    syntax_file = mc_config_get_full_path (EDIT_SYNTAX_FILE);
    res = syntax_rules_load (syntax_file, &sel, &rules, &error_file);
    g_free (syntax_file);
    g_free (error_file);  // a broken syntax file is the editor's business to report

    if (res != 0)
    {
        g_free (ds->buf);
        g_free (ds);
        return NULL;
    }

    ds->line_local = syntax_rules_is_line_local (rules);
    ds->sc = syntax_scanner_new (rules, dview_syntax_get_byte, ds, ds->size);
    ds->palette = syntax_palette_new (rules, syntax_tty_alloc_color, syntax_tty_release_color,
                                      (void *) "editor", EDITOR_NORMAL_COLOR);
    syntax_rules_unref (rules);

    return ds;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Drop the copy of the file and the scanner over it, keeping what is needed to
 * turn a run's color into something to draw with.
 *
 * The bytes are only wanted while the runs are being built; the palette outlives
 * them, because drawing happens later and many times.
 */

void
dview_syntax_release_source (dview_syntax_t *ds)
{
    if (ds == NULL)
        return;

    syntax_scanner_free (ds->sc);
    ds->sc = NULL;
    MC_PTR_FREE (ds->buf);
    ds->size = 0;
}

/* --------------------------------------------------------------------------------------------- */

void
dview_syntax_close (dview_syntax_t *ds)
{
    if (ds == NULL)
        return;

    dview_syntax_release_source (ds);
    syntax_palette_free (ds->palette);
    g_free (ds);
}

/* --------------------------------------------------------------------------------------------- */

off_t
dview_syntax_size (const dview_syntax_t *ds)
{
    return ds == NULL ? 0 : ds->size;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Append the runs covering a range of bytes, coalescing equal colors.
 *
 * Meant to be called once per line, in increasing order: that is the direction
 * the scanner is cheap in.  Out of order is correct but costs a rescan from the
 * nearest checkpoint.
 *
 * @param ds handle the runs come from
 * @param from first byte of the range
 * @param to byte past its end
 * @param runs array of syntax_run_t to append to
 */

void
dview_syntax_runs (dview_syntax_t *ds, off_t from, off_t to, GArray *runs)
{
    if (ds == NULL || runs == NULL || from < 0)
        return;

    if (to > ds->size)
        to = ds->size;

    if (!ds->line_local)
    {
        syntax_runs_for_range (ds->sc, from, to, runs);
        return;
    }

    // line-local rules start afresh on every line, and a call is one line
    syntax_line_local_reset (&ds->ll, from);

    {
        off_t i;
        guint cur = 0;
        guint32 len = 0;

        for (i = from; i < to; i++)
        {
            const guint color = syntax_line_local_color (syntax_scanner_rules (ds->sc), &ds->ll,
                                                         dview_syntax_get_byte, ds, i);

            if (color != cur && len != 0)
            {
                syntax_run_t r = { len, cur };

                g_array_append_val (runs, r);
                len = 0;
            }

            cur = color;
            len++;
        }

        if (len != 0)
        {
            syntax_run_t r = { len, cur };

            g_array_append_val (runs, r);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

int
dview_syntax_color (const dview_syntax_t *ds, guint32 run_color)
{
    return ds == NULL ? 0 : syntax_palette_get (ds->palette, run_color);
}

/* --------------------------------------------------------------------------------------------- */
