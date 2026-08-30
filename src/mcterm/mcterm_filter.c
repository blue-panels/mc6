/*
   Midnight Commander - mcterm line filter.

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

#include "lib/global.h"
#include "lib/search.h"
#include "lib/util.h"  // MC_PTR_FREE

#include "mcterm_filter.h"
#include "mcterm_select.h"

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_filter_active (const mcterm_filter_t *f)
{
    return (f != NULL && f->pattern != NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_filter_clear (mcterm_filter_t *f)
{
    if (f == NULL)
        return;

    MC_PTR_FREE (f->pattern);
    g_clear_pointer (&f->rows, g_array_unref);
    f->top = 0;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_filter_apply (mcterm_filter_t *f, mcview_vterm_t *vt, int cols, gint64 newest,
                     const char *pattern)
{
    mc_search_t *search;
    GArray *rows;
    gint64 row;
    gint64 oldest;

    if (f == NULL || vt == NULL || cols <= 0 || pattern == NULL || *pattern == '\0')
        return FALSE;

    search = mc_search_new (pattern, NULL);
    if (search == NULL)
        return FALSE;

    /* What a log is filtered by is looked for the way the viewer looks for it:
       a plain string, of either case. */
    search->search_type = MC_SEARCH_T_NORMAL;
    search->is_case_sensitive = FALSE;

    rows = g_array_new (FALSE, FALSE, sizeof (gint64));
    oldest = mcview_vterm_scrolled_rows (vt) - mcview_vterm_history_len (vt);

    for (row = oldest; row <= newest; row++)
    {
        char *text;

        text = mcterm_filter_row_text (vt, row, cols);
        if (text == NULL)
            continue;

        if (*text != '\0' && mc_search_run (search, text, 0, strlen (text), NULL))
            g_array_append_val (rows, row);

        g_free (text);
    }

    mc_search_free (search);

    if (rows->len == 0)
    {
        g_array_unref (rows);
        return FALSE;
    }

    mcterm_filter_clear (f);
    f->pattern = g_strdup (pattern);
    f->rows = rows;
    f->top = 0;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

int
mcterm_filter_len (const mcterm_filter_t *f)
{
    if (f == NULL || f->rows == NULL)
        return 0;

    return (int) f->rows->len;
}

/* --------------------------------------------------------------------------------------------- */

gint64
mcterm_filter_row (const mcterm_filter_t *f, int index)
{
    if (index < 0 || index >= mcterm_filter_len (f))
        return -1;

    return g_array_index (f->rows, gint64, index);
}

/* --------------------------------------------------------------------------------------------- */

int
mcterm_filter_index (const mcterm_filter_t *f, gint64 row)
{
    int lo = 0;
    int hi = mcterm_filter_len (f) - 1;

    if (hi < 0)
        return 0;

    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;

        if (g_array_index (f->rows, gint64, mid) < row)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo;
}

/* --------------------------------------------------------------------------------------------- */

char *
mcterm_filter_row_text (mcview_vterm_t *vt, gint64 row, int cols)
{
    GString *text;
    gsize last_word = 0;
    int col;

    if (vt == NULL || cols <= 0)
        return NULL;

    text = g_string_sized_new (cols);

    for (col = 0; col < cols; col++)
    {
        const mcview_vterm_cell_t *cell = mcterm_sel_cell_at (vt, row, col);
        const gunichar ch = (cell == NULL || cell->ch == 0) ? ' ' : cell->ch;

        g_string_append_unichar (text, ch);
        if (ch != ' ')
            last_word = text->len;
    }

    // A terminal row is padded with blanks up to its width; they are not text.
    g_string_truncate (text, last_word);

    return g_string_free (text, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
