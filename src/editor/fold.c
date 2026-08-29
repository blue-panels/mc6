/*
   Editor code folding

   Copyright (C) 2025-2026
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

/** \file
 *  \brief Source: editor code folding
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/search.h"
#include "lib/widget.h"  // message(), status_msg

#include "edit-impl.h"
#include "editwidget.h"
#include "editsearch.h"

/* --------------------------------------------------------------------------------------------- */
/*** global variables ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** file scope macro definitions ****************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** file scope type declarations ****************************************************************/

typedef struct
{
    WEdit *edit;
    off_t bol;
    off_t eol;
} edit_filter_search_data_t;

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** forward declarations (file scope functions) *************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** file scope variables ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/

static mc_search_cbret_t
edit_filter_search_callback (const void *user_data, off_t char_offset, int *current_char)
{
    const edit_filter_search_data_t *data = (const edit_filter_search_data_t *) user_data;

    if (char_offset < 0 || char_offset >= data->eol - data->bol)
        return MC_SEARCH_CB_NOTFOUND;

    *current_char = edit_buffer_get_byte (&data->edit->buffer, data->bol + char_offset);
    return MC_SEARCH_CB_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Find the fold that contains the given line.
 *
 * @param edit editor object
 * @param line line number to check
 * @return pointer to fold if line is the start line or within fold range, NULL otherwise
 */
edit_fold_t *
edit_fold_find (WEdit *edit, long line)
{
    edit_fold_t *p;

    if (edit->folds == NULL)
        return NULL;

    for (p = edit->folds; p != NULL; p = p->next)
    {
        if (line >= p->line_start && line <= p->line_start + p->line_count)
            return p;
        if (p->line_start > line)
            break;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Check if a line is hidden inside a fold (not the fold start line itself).
 *
 * @param edit editor object
 * @param line line number to check
 * @return TRUE if line is hidden
 */
gboolean
edit_fold_is_hidden (WEdit *edit, long line)
{
    edit_fold_t *f;

    f = edit_fold_find (edit, line);
    if (f == NULL)
        return FALSE;

    return (line > f->line_start && line <= f->line_start + f->line_count);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Create a new fold region.  The first visible line is line_start,
 * and line_count lines below it become hidden.
 *
 * If the new fold overlaps an existing fold, the existing fold is removed first.
 *
 * @param edit editor object
 * @param line_start first line of the fold
 * @param line_count number of lines to hide
 */
void
edit_fold_make (WEdit *edit, long line_start, long line_count)
{
    edit_fold_t *p, *q, *new_fold;

    if (line_count <= 0)
        return;

    /* remove any folds that overlap with the new region */
    p = edit->folds;
    while (p != NULL)
    {
        q = p->next;
        /* overlap: fold [p->line_start, p->line_start + p->line_count]
           intersects [line_start, line_start + line_count] */
        if (p->line_start + p->line_count >= line_start && p->line_start <= line_start + line_count)
        {
            /* remove p */
            if (p->prev != NULL)
                p->prev->next = p->next;
            else
                edit->folds = p->next;
            if (p->next != NULL)
                p->next->prev = p->prev;
            g_free (p);
        }
        p = q;
    }

    /* create and insert new fold in sorted order */
    new_fold = g_new0 (edit_fold_t, 1);
    new_fold->line_start = line_start;
    new_fold->line_count = line_count;

    if (edit->folds == NULL || edit->folds->line_start > line_start)
    {
        /* insert at head */
        new_fold->next = edit->folds;
        new_fold->prev = NULL;
        if (edit->folds != NULL)
            edit->folds->prev = new_fold;
        edit->folds = new_fold;
    }
    else
    {
        /* find insertion point */
        for (p = edit->folds; p->next != NULL && p->next->line_start <= line_start; p = p->next)
            ;
        new_fold->next = p->next;
        new_fold->prev = p;
        if (p->next != NULL)
            p->next->prev = new_fold;
        p->next = new_fold;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Remove the fold that contains the given line.
 *
 * @param edit editor object
 * @param line line number
 * @return TRUE if a fold was removed
 */
gboolean
edit_fold_remove (WEdit *edit, long line)
{
    edit_fold_t *f;

    f = edit_fold_find (edit, line);
    if (f == NULL)
        return FALSE;

    if (f->prev != NULL)
        f->prev->next = f->next;
    else
        edit->folds = f->next;

    if (f->next != NULL)
        f->next->prev = f->prev;

    g_free (f);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Get the next visible line number after the given line.
 * If the line is a fold start, skip over its hidden lines.
 *
 * @param edit editor object
 * @param line current line number
 * @return next visible line number
 */
long
edit_fold_next_visible (WEdit *edit, long line)
{
    edit_fold_t *f;

    f = edit_fold_find (edit, line);
    if (f != NULL && line == f->line_start)
        return f->line_start + f->line_count + 1;

    /* if inside a fold (shouldn't normally happen for cursor), jump past it */
    if (f != NULL && line > f->line_start)
        return f->line_start + f->line_count + 1;

    return line + 1;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Get the previous visible line number before the given line.
 *
 * @param edit editor object
 * @param line current line number
 * @return previous visible line number
 */
long
edit_fold_prev_visible (WEdit *edit, long line)
{
    edit_fold_t *f;

    if (line <= 0)
        return 0;

    f = edit_fold_find (edit, line - 1);
    if (f != NULL && (line - 1) > f->line_start)
        return f->line_start < 0 ? line : f->line_start;

    return line - 1;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Remove all folds.
 *
 * @param edit editor object
 */
void
edit_fold_flush (WEdit *edit)
{
    edit_fold_t *p, *q;

    for (p = edit->folds; p != NULL; p = q)
    {
        q = p->next;
        g_free (p);
    }
    edit->folds = NULL;
    edit->filter_active = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Shift fold line numbers down by 1 for all folds after the given line.
 * Called when a new line is inserted.
 *
 * @param edit editor object
 * @param line line where insertion happened
 */
void
edit_fold_inc (WEdit *edit, long line)
{
    edit_fold_t *p;

    for (p = edit->folds; p != NULL; p = p->next)
    {
        if (p->line_start > line)
            p->line_start++;
        else if (edit->filter_active && line == p->line_start)
            /* a line split at a filter boundary stays visible: the hidden run moves down */
            p->line_start++;
        else if (line > p->line_start && line <= p->line_start + p->line_count)
            p->line_count++;
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Shift fold line numbers up by 1 for all folds after the given line.
 * Called when two lines are joined: the newline at the end of @line was deleted,
 * so line + 1 merged into it.
 *
 * @param edit editor object
 * @param line upper line of the join
 */
void
edit_fold_dec (WEdit *edit, long line)
{
    edit_fold_t *p, *q;

    for (p = edit->folds; p != NULL; p = q)
    {
        q = p->next;
        if (p->line_start > line)
            p->line_start--;
        else if (line <= p->line_start + p->line_count)
        {
            /* one of the joined lines was hidden: the run is one line shorter */
            p->line_count--;
            if (p->line_count <= 0)
            {
                /* fold collapsed - remove it */
                if (p->prev != NULL)
                    p->prev->next = p->next;
                else
                    edit->folds = p->next;
                if (p->next != NULL)
                    p->next->prev = p->prev;
                g_free (p);
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Calculate the visual width of the fold indicator text "...} (N lines)".
 *
 * Uses str_term_width1 for correct i18n/UTF-8 handling.
 *
 * @param edit editor object
 * @param fold fold structure
 * @return visual column width of the fold indicator text
 */
int
edit_fold_indicator_width (const WEdit *edit, const struct edit_fold_t *fold)
{
    (void) fold;
    /* filter folds draw nothing in the text; a code fold shows "...}" - always 4 columns */
    return edit->filter_active ? 0 : 4;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Toggle fold at the current cursor line.
 *
 * If the cursor is on a fold start, unfold it.
 * If a selection is active, fold the selected lines.
 * Otherwise, find an opening bracket on the line, match it, and fold that range.
 *
 * @param edit editor object
 */
void
edit_fold_toggle (WEdit *edit)
{
    long line;
    edit_fold_t *fold;

    line = edit->buffer.curs_line;
    fold = edit_fold_find (edit, line);

    if (fold != NULL && line == fold->line_start)
    {
        /* existing fold - unfold */
        edit_fold_remove (edit, fold->line_start);
    }
    else
    {
        off_t start_mark, end_mark;

        if (eval_marks (edit, &start_mark, &end_mark))
        {
            /* selection active - fold selected lines */
            long line1, line2;

            line1 = edit_buffer_count_lines (&edit->buffer, 0, start_mark);
            line2 = edit_buffer_count_lines (&edit->buffer, 0, end_mark);
            if (line2 > line1)
            {
                edit_fold_make (edit, line1, line2 - line1);
                edit_mark_cmd (edit, TRUE);
            }
        }
        else
        {
            /* no selection - find { on this line, match } */
            off_t bol, eol, pos;

            bol = edit_buffer_get_current_bol (&edit->buffer);
            eol = edit_buffer_get_current_eol (&edit->buffer);

            for (pos = bol; pos < eol; pos++)
            {
                if (strchr ("{[(", edit_buffer_get_byte (&edit->buffer, pos)) != NULL)
                {
                    off_t match;

                    edit_cursor_move (edit, pos - edit->buffer.curs1);
                    match = edit_get_bracket (edit, 0, 0);
                    if (match >= 0)
                    {
                        long line2;

                        line2 = edit_buffer_count_lines (&edit->buffer, 0, match);
                        if (line2 > line)
                        {
                            edit_fold_make (edit, line, line2 - line);
                            break;
                        }
                    }
                }
            }
        }
    }

    edit->mark1 = edit->mark2 = edit->buffer.curs1;
    edit->force |= REDRAW_PAGE;
}

/* --------------------------------------------------------------------------------------------- */

static int
edit_filter_status_update_cb (status_msg_t *sm)
{
    simple_status_msg_t *ssm = SIMPLE_STATUS_MSG (sm);
    edit_search_status_msg_t *esm = (edit_search_status_msg_t *) sm;

    label_set_textv (ssm->label, _ ("Filtering %s: %3d%%"), esm->edit->last_search_string,
                     edit_buffer_calc_percent (&esm->edit->buffer, esm->offset));

    return status_msg_common_update (sm);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Hide every line that does not match the search.  The set of hidden lines is fixed at
 * this moment: later edits only shift the hidden runs around, they never re-match.
 *
 * @param edit editor object
 * @param search prepared search object; ownership stays with the caller
 * @return TRUE if the filter was applied, FALSE if nothing matched, the search failed
 *         or the user cancelled
 */
gboolean
edit_filter_apply (WEdit *edit, mc_search_t *search)
{
    edit_filter_search_data_t data;
    off_t bol = 0;
    long line;
    long run_start = -1;  // first line of the hidden run being collected, -1 if none
    long shown = 0;
    long curs_target = -1;
    GSList *runs = NULL, *r;
    mc_search_fn saved_search_fn;
    mc_update_fn saved_update_fn;
    edit_search_status_msg_t esm;
    gboolean ok = TRUE;
    char *error_str = NULL;

    /* each line is handed to the engine on its own; progress is reported per line, not per
       byte, so the editor's update callback is not used */
    saved_search_fn = search->search_fn;
    saved_update_fn = search->update_fn;
    search->search_fn = edit_filter_search_callback;
    search->update_fn = NULL;

    memset (&esm, 0, sizeof (esm));
    esm.first = TRUE;
    esm.edit = edit;
    status_msg_init (STATUS_MSG (&esm), _ ("Filter"), 1.0, simple_status_msg_init_cb,
                     edit_filter_status_update_cb, NULL);

    data.edit = edit;

    for (line = 0; line <= edit->buffer.lines; line++)
    {
        off_t eol;
        gsize found_len;
        gboolean match;

        if ((line & 0xff) == 0)
        {
            esm.offset = bol;
            if (status_msg_common_update (STATUS_MSG (&esm)) == B_CANCEL)
            {
                ok = FALSE;
                break;
            }
        }

        eol = edit_buffer_get_eol (&edit->buffer, bol);
        data.bol = bol;
        data.eol = eol;

        match = mc_search_run (search, &data, 0, eol - bol, &found_len);
        if (!match && search->error != MC_SEARCH_E_OK && search->error != MC_SEARCH_E_NOTFOUND)
        {
            error_str = g_strdup (search->error_str);
            ok = FALSE;
            break;
        }

        if (!match)
        {
            if (run_start < 0)
                run_start = line;
        }
        else
        {
            shown++;
            if (curs_target < 0 && line >= edit->buffer.curs_line)
                curs_target = line;
            if (run_start >= 0)
            {
                long *run = g_new (long, 2);

                run[0] = run_start - 1;
                run[1] = line - run_start;
                runs = g_slist_prepend (runs, run);
                run_start = -1;
            }
        }

        bol = eol + 1;
    }

    status_msg_deinit (STATUS_MSG (&esm));
    search->search_fn = saved_search_fn;
    search->update_fn = saved_update_fn;

    if (!ok)
    {
        g_slist_free_full (runs, g_free);
        if (error_str != NULL)
        {
            message (D_ERROR, MSG_ERROR, "%s", error_str);
            g_free (error_str);
        }
        return FALSE;
    }

    if (shown == 0)
    {
        g_slist_free_full (runs, g_free);
        message (D_NORMAL, _ ("Filter"), _ ("No lines match"));
        return FALSE;
    }

    if (run_start >= 0)
    {
        long *run = g_new (long, 2);

        run[0] = run_start - 1;
        run[1] = edit->buffer.lines - run_start + 1;
        runs = g_slist_prepend (runs, run);
    }

    edit_fold_flush (edit);
    for (r = runs; r != NULL; r = r->next)
    {
        long *run = r->data;

        edit_fold_make (edit, run[0], run[1]);
    }
    g_slist_free_full (runs, g_free);
    edit->filter_active = TRUE;

    /* a cursor left on a hidden line lands on the nearest shown line below, else above */
    if (edit_fold_is_hidden (edit, edit->buffer.curs_line))
    {
        if (curs_target < 0)
            curs_target = edit_fold_find (edit, edit->buffer.curs_line)->line_start;
        edit_move_to_line (edit, curs_target);
        edit_cursor_move (edit, edit_buffer_get_current_bol (&edit->buffer) - edit->buffer.curs1);
        edit->over_col = 0;
        edit->prev_col = 0;
    }
    if (edit_fold_is_hidden (edit, edit->start_line))
        edit_scroll_downward (edit, 0);

    edit->force |= REDRAW_PAGE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Lift the filter if one is on; otherwise put the last search back on as a filter,
 * or ask for one when there has been no search yet.
 */
void
edit_filter_toggle (WEdit *edit)
{
    if (edit->filter_active)
    {
        edit_fold_flush (edit);
        edit->force |= REDRAW_PAGE;
    }
    else if (edit->search != NULL)
        (void) edit_filter_apply (edit, edit->search);
    else
        edit_search_cmd (edit, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
