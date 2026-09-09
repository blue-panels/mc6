/*
   Internal file viewer for the M-Commander
   Text selection: hit map of the displayed characters, mouse and Shift keys, copy

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
#include "lib/event.h"
#include "src/setup.h"  // option_tab_spacing

#include "internal.h"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    gboolean valid;
    off_t from;    // raw offset before the displayed character
    off_t to;      // raw offset after it (ANSI and nroff bytes included)
    off_t column;  // logical column before it, needed to expand an initial TAB
    int ch;        // displayed base character, before control-character replacement
    int row;
    int col;
} mcview_selection_point_t;

typedef struct
{
    gboolean valid;
    off_t from;
    off_t to;
    off_t column;
    int ch;
} mcview_selection_cell_t;

struct mcview_selection
{
    gboolean anchored;  // anchor is valid
    gboolean active;    // anchor..point is highlighted
    gboolean dragging;  // left button is held since a press inside the data area
    gboolean pressed;   // the press was consumed; the click that follows it is too
    mcview_selection_point_t anchor;
    mcview_selection_point_t point;
    mcview_selection_point_t cursor;

    gboolean cursor_on_screen;
    gboolean wrap;  // the hit map was drawn in wrap mode

    GArray *cells;  // mcview_selection_cell_t, one entry per visible terminal cell
    int rows;
    int cols;
};

/*** file scope macro definitions ****************************************************************/

/* Ctrl-Left and Ctrl-Right step this many characters, the same as MCTERM_JUMP_COLS in mcterm. */
#define MCVIEW_CURSOR_JUMP 8

/*** file scope variables ************************************************************************/

/* Same word delimiters as the editor and mcterm selection. */
static const char mcview_word_break[] = "{}[]()<>=|/\\!?~-+`'\",.;:#$%^&*";

/*** file scope functions ************************************************************************/

static gboolean
mcview_selection_supported (const WView *view)
{
    return view != NULL && !view->mode_flags.hex && !view->mode_flags.terminal
        && !view->mode_flags.structured;
}

/* --------------------------------------------------------------------------------------------- */

static int
mcview_selection_point_compare (const mcview_selection_point_t *a,
                                const mcview_selection_point_t *b)
{
    if (a->from < b->from)
        return -1;
    if (a->from > b->from)
        return 1;
    if (a->to < b->to)
        return -1;
    if (a->to > b->to)
        return 1;
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_selection_bounds (const struct mcview_selection *sel, mcview_selection_point_t *first,
                         mcview_selection_point_t *last)
{
    if (sel == NULL || !sel->active || !sel->anchor.valid || !sel->point.valid)
        return FALSE;

    if (mcview_selection_point_compare (&sel->anchor, &sel->point) <= 0)
    {
        *first = sel->anchor;
        *last = sel->point;
    }
    else
    {
        *first = sel->point;
        *last = sel->anchor;
    }

    return first->from < last->to;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_selection_same_char (const mcview_selection_point_t *a, const mcview_selection_point_t *b)
{
    return a->valid && b->valid && a->from == b->from && a->to == b->to;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_selection_cell_at (const struct mcview_selection *sel, int row, int col,
                          mcview_selection_point_t *point)
{
    const mcview_selection_cell_t *cell;
    guint index;

    if (sel == NULL || sel->cells == NULL || row < 0 || row >= sel->rows || col < 0
        || col >= sel->cols)
        return FALSE;

    index = (guint) row * (guint) sel->cols + (guint) col;
    cell = &g_array_index (sel->cells, mcview_selection_cell_t, index);
    if (!cell->valid)
        return FALSE;

    point->valid = TRUE;
    point->from = cell->from;
    point->to = cell->to;
    point->column = cell->column;
    point->ch = cell->ch;
    point->row = row;
    point->col = col;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The character on @col, or the closest character of that visual row. */
static gboolean
mcview_selection_point_on_row (const struct mcview_selection *sel, int row, int col,
                               mcview_selection_point_t *point)
{
    int distance;

    if (sel == NULL || row < 0 || row >= sel->rows || sel->cols <= 0)
        return FALSE;

    col = CLAMP (col, 0, sel->cols - 1);
    if (mcview_selection_cell_at (sel, row, col, point))
        return TRUE;

    for (distance = 1; distance < sel->cols; distance++)
    {
        if (col - distance >= 0 && mcview_selection_cell_at (sel, row, col - distance, point))
            return TRUE;
        if (col + distance < sel->cols
            && mcview_selection_cell_at (sel, row, col + distance, point))
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_selection_first_on_row (const struct mcview_selection *sel, int row,
                               mcview_selection_point_t *point)
{
    int col;

    for (col = 0; sel != NULL && col < sel->cols; col++)
        if (mcview_selection_cell_at (sel, row, col, point))
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_selection_last_on_row (const struct mcview_selection *sel, int row,
                              mcview_selection_point_t *point)
{
    int col;

    for (col = sel != NULL ? sel->cols - 1 : -1; col >= 0; col--)
        if (mcview_selection_cell_at (sel, row, col, point))
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_selection_first (const struct mcview_selection *sel, mcview_selection_point_t *point)
{
    int row;

    for (row = 0; sel != NULL && row < sel->rows; row++)
        if (mcview_selection_first_on_row (sel, row, point))
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_selection_last (const struct mcview_selection *sel, mcview_selection_point_t *point)
{
    int row;

    for (row = sel != NULL ? sel->rows - 1 : -1; row >= 0; row--)
        if (mcview_selection_last_on_row (sel, row, point))
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* The next displayed character. In unwrap mode a row ends only at its newline: a line cut by
   the viewport edge is scrolled by the caller instead of jumping to the neighbouring row. */
static gboolean
mcview_selection_next (const struct mcview_selection *sel, const mcview_selection_point_t *current,
                       gboolean forward, mcview_selection_point_t *point)
{
    int row = current->row;
    int col = current->col;

    while (sel != NULL)
    {
        col += forward ? 1 : -1;
        if (col >= sel->cols)
        {
            if (!sel->wrap && current->ch != '\n')
                return FALSE;
            row++;
            col = 0;
        }
        else if (col < 0)
        {
            if (!sel->wrap && current->column > 0)
                return FALSE;
            row--;
            col = sel->cols - 1;
        }

        if (row < 0 || row >= sel->rows)
            return FALSE;

        if (mcview_selection_cell_at (sel, row, col, point)
            && !mcview_selection_same_char (current, point))
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcview_selection_is_word (int ch)
{
    if (ch <= 0 || g_unichar_isspace ((gunichar) ch))
        return FALSE;

    return ch >= 0x80 || strchr (mcview_word_break, ch) == NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_selection_word (struct mcview_selection *sel, const mcview_selection_point_t *at)
{
    mcview_selection_point_t first = *at;
    mcview_selection_point_t last = *at;
    mcview_selection_point_t candidate;

    if (mcview_selection_is_word (at->ch))
    {
        while (mcview_selection_next (sel, &first, FALSE, &candidate) && candidate.row == at->row
               && mcview_selection_is_word (candidate.ch))
            first = candidate;
        while (mcview_selection_next (sel, &last, TRUE, &candidate) && candidate.row == at->row
               && mcview_selection_is_word (candidate.ch))
            last = candidate;
    }

    sel->anchored = TRUE;
    sel->active = TRUE;
    sel->anchor = first;
    sel->point = last;
    sel->cursor = last;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcview_selection_line (struct mcview_selection *sel, const mcview_selection_point_t *at)
{
    mcview_selection_point_t first;
    mcview_selection_point_t last;

    if (!mcview_selection_first_on_row (sel, at->row, &first)
        || !mcview_selection_last_on_row (sel, at->row, &last))
        return;

    /* A newline is a boundary, not a visible part of a selected line. */
    while (last.ch == '\n')
    {
        mcview_selection_point_t previous;

        if (!mcview_selection_next (sel, &last, FALSE, &previous) || previous.row != at->row)
            break;
        last = previous;
    }

    sel->anchored = TRUE;
    sel->active = TRUE;
    sel->anchor = first;
    sel->point = last;
    sel->cursor = last;
}

/* --------------------------------------------------------------------------------------------- */

/* The cursor stays on the screen when the text scrolls away from under it: it keeps its screen
   cell, taking the character now shown there. Without a cursor yet, the first visible one. */
static gboolean
mcview_selection_locate_cursor (struct mcview_selection *sel)
{
    mcview_selection_point_t at;

    if (sel->cursor.valid && sel->cursor_on_screen)
        return TRUE;

    if (sel->cursor.valid
        && mcview_selection_point_on_row (sel, sel->cursor.row, sel->cursor.col, &at))
        sel->cursor = at;
    else if (!mcview_selection_first (sel, &sel->cursor))
        return FALSE;

    /* A click's anchor that nothing shows is not carried over to the new text. */
    if (!sel->active)
        sel->anchored = FALSE;

    sel->cursor_on_screen = TRUE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Move the viewport when a Shift movement reaches its edge, redraw the hit map, and find the
   command's destination on the newly visible row. */
static gboolean
mcview_selection_scroll (WView *view, struct mcview_selection *sel, long command,
                         mcview_selection_point_t *target)
{
    const off_t old_start = view->dpy_start;
    const off_t old_skip = view->dpy_paragraph_skip_lines;
    const off_t old_column = view->dpy_text_column;
    const int page = MAX (sel->rows - 1, 1);

    switch (command)
    {
    case CK_MarkLeft:
        if (!view->mode_flags.wrap && sel->cursor.column > 0)
            mcview_move_left (view, 1);
        else
            mcview_move_up (view, 1);
        break;
    case CK_MarkRight:
        if (!view->mode_flags.wrap && sel->cursor.ch != '\n')
            mcview_move_right (view, 1);
        else
            mcview_move_down (view, 1);
        break;
    case CK_MarkUp:
        mcview_move_up (view, 1);
        break;
    case CK_MarkDown:
        mcview_move_down (view, 1);
        break;
    case CK_MarkPageUp:
        mcview_move_up (view, page);
        break;
    case CK_MarkPageDown:
        mcview_move_down (view, page);
        break;
    default:
        return FALSE;
    }

    if (view->dpy_start == old_start && view->dpy_paragraph_skip_lines == old_skip
        && view->dpy_text_column == old_column)
        return FALSE;

    /* mcview_update() runs after the command, but the new cells are needed now to extend the
       selection in this same keystroke. */
    mcview_display (view);

    if (!sel->cursor_on_screen)
    {
        /* A page scroll leaves the cursor behind: continue from the new page's edge. */
        if (command == CK_MarkPageUp)
            return mcview_selection_first (sel, target);
        if (command == CK_MarkPageDown)
            return mcview_selection_last (sel, target);
        return FALSE;
    }

    switch (command)
    {
    case CK_MarkLeft:
        return mcview_selection_next (sel, &sel->cursor, FALSE, target);
    case CK_MarkRight:
        return mcview_selection_next (sel, &sel->cursor, TRUE, target);
    case CK_MarkUp:
        return mcview_selection_point_on_row (sel, sel->cursor.row - 1, sel->cursor.col, target);
    case CK_MarkDown:
        return mcview_selection_point_on_row (sel, sel->cursor.row + 1, sel->cursor.col, target);
    case CK_MarkPageUp:
        return mcview_selection_point_on_row (sel, 0, sel->cursor.col, target);
    case CK_MarkPageDown:
        if (mcview_selection_point_on_row (sel, sel->rows - 1, sel->cursor.col, target))
            return TRUE;
        return mcview_selection_last (sel, target);
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* A plain cursor move, the way mcterm moves its cursor: along the row, on to the neighbouring
   row at its end, by rows for Up and Down, scrolling at the edge of the viewport. It drops the
   selection and keeps the cursor. */
static gboolean
mcview_selection_move_cursor (WView *view, struct mcview_selection *sel, long command)
{
    int steps = (command == CK_LeftQuick || command == CK_RightQuick) ? MCVIEW_CURSOR_JUMP : 1;

    if (!mcview_selection_locate_cursor (sel))
        return FALSE;

    mcview_selection_clear (view);

    for (; steps > 0; steps--)
    {
        mcview_selection_point_t target = { .valid = FALSE };
        long edge;
        gboolean found;

        switch (command)
        {
        case CK_Up:
            edge = CK_MarkUp;
            found =
                mcview_selection_point_on_row (sel, sel->cursor.row - 1, sel->cursor.col, &target);
            break;
        case CK_Down:
            edge = CK_MarkDown;
            found =
                mcview_selection_point_on_row (sel, sel->cursor.row + 1, sel->cursor.col, &target);
            break;
        case CK_Left:
        case CK_LeftQuick:
            edge = CK_MarkLeft;
            found = mcview_selection_next (sel, &sel->cursor, FALSE, &target);
            break;
        default:
            edge = CK_MarkRight;
            found = mcview_selection_next (sel, &sel->cursor, TRUE, &target);
            break;
        }

        if (!found && !mcview_selection_scroll (view, sel, edge, &target))
            break;
        sel->cursor = target;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* In filter mode only the matching lines are displayed: after a newline, continue from the
   next one, as mcview_display_text() does. */
static gboolean
mcview_selection_next_line (WView *view, mcview_state_machine_t *state)
{
    guint idx;
    off_t next_match;

    if (!view->filter_active || view->filter_offsets == NULL || view->filter_offsets->len == 0)
        return TRUE;

    idx = mcview_filter_idx (view, state->offset);
    next_match = mcview_filter_offset (view, idx);
    if (next_match < state->offset)
        next_match = mcview_filter_offset (view, idx + 1);
    if (next_match == (off_t) -1)
        return FALSE;

    if (next_match > state->offset)
        mcview_state_machine_init (state, next_match);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_selection_init (WView *view)
{
    if (view != NULL && view->selection == NULL)
        view->selection = g_new0 (struct mcview_selection, 1);
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_selection_done (WView *view)
{
    if (view == NULL || view->selection == NULL)
        return;

    if (view->selection->cells != NULL)
        g_array_free (view->selection->cells, TRUE);
    g_free (view->selection);
    view->selection = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_selection_clear (WView *view)
{
    struct mcview_selection *sel;

    if (view == NULL || view->selection == NULL)
        return;

    sel = view->selection;
    sel->anchored = FALSE;
    sel->active = FALSE;
    memset (&sel->anchor, 0, sizeof (sel->anchor));
    memset (&sel->point, 0, sizeof (sel->point));
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_selection_active (const WView *view)
{
    return mcview_selection_supported (view) && view->selection != NULL && view->selection->active;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_selection_render_begin (WView *view)
{
    struct mcview_selection *sel;
    guint length;

    if (view == NULL || view->selection == NULL)
        return;

    sel = view->selection;
    sel->cursor_on_screen = FALSE;
    sel->wrap = view->mode_flags.wrap;
    sel->rows = MAX (view->data_area.lines, 0);
    sel->cols = MAX (view->data_area.cols, 0);
    length = (guint) sel->rows * (guint) sel->cols;

    if (sel->cells == NULL)
        sel->cells = g_array_sized_new (FALSE, TRUE, sizeof (mcview_selection_cell_t), length);
    g_array_set_size (sel->cells, length);
    if (length != 0)
        memset (sel->cells->data, 0, length * sizeof (mcview_selection_cell_t));
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_selection_record (WView *view, int row, int col, int width,
                         const mcview_state_machine_t *before, off_t after, int ch)
{
    struct mcview_selection *sel;
    int first, last, x;

    if (view == NULL || view->selection == NULL || before == NULL || width <= 0)
        return;

    /* A line scrolled out to the left still owns its row: its newline is the row's only cell. */
    if (ch == '\n' && col < 0)
        col = 0;

    sel = view->selection;
    if (sel->cells == NULL || row < 0 || row >= sel->rows || col >= sel->cols || col + width <= 0)
        return;

    first = MAX (col, 0);
    last = MIN (col + width, sel->cols);
    for (x = first; x < last; x++)
    {
        mcview_selection_cell_t *cell = &g_array_index (
            sel->cells, mcview_selection_cell_t, (guint) row * (guint) sel->cols + (guint) x);

        cell->valid = TRUE;
        cell->from = before->offset;
        cell->to = after;
        cell->column = before->unwrapped_column;
        cell->ch = ch;

        if (!sel->cursor_on_screen && sel->cursor.valid && sel->cursor.from == cell->from
            && sel->cursor.to == cell->to)
        {
            sel->cursor.row = row;
            sel->cursor.col = x;
            sel->cursor_on_screen = TRUE;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The rows are drawn: a cursor that the text scrolled away from takes its screen cell back. */
void
mcview_selection_render_end (WView *view)
{
    if (view != NULL && view->selection != NULL && view->selection->cursor.valid)
        (void) mcview_selection_locate_cursor (view->selection);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcview_selection_contains (const WView *view, off_t from, off_t to)
{
    mcview_selection_point_t first;
    mcview_selection_point_t last;

    if (view == NULL || !mcview_selection_bounds (view->selection, &first, &last))
        return FALSE;

    return from < last.to && to > first.from;
}

/* --------------------------------------------------------------------------------------------- */

/* The screen cell of the selection cursor, the place the next Shift movement starts from. */
gboolean
mcview_selection_cursor (const WView *view, int *row, int *col)
{
    const struct mcview_selection *sel;

    if (!mcview_selection_supported (view) || view->selection == NULL)
        return FALSE;

    sel = view->selection;
    if (!sel->cursor.valid || !sel->cursor_on_screen)
        return FALSE;

    *row = sel->cursor.row;
    *col = sel->cursor.col;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The left button inside the data area is owned by the selection: a press anchors, a drag
   extends, the release ends the drag, and the click that follows a press is consumed too. */
gboolean
mcview_selection_mouse (WView *view, mouse_msg_t msg, mouse_event_t *event)
{
    struct mcview_selection *sel;
    mcview_selection_point_t at;
    int row, col;

    if (!mcview_selection_supported (view) || view->selection == NULL || event == NULL)
        return FALSE;

    sel = view->selection;

    switch (msg)
    {
    case MSG_MOUSE_UP:
        sel->dragging = FALSE;
        return FALSE;
    case MSG_MOUSE_CLICK:
    {
        const gboolean pressed = sel->pressed;

        sel->pressed = FALSE;
        return pressed;
    }
    case MSG_MOUSE_DOWN:
        sel->pressed = FALSE;
        sel->dragging = FALSE;
        if ((event->buttons & GPM_B_LEFT) == 0)
            return FALSE;
        break;
    case MSG_MOUSE_DRAG:
        if (!sel->dragging)
            return FALSE;
        break;
    default:
        return FALSE;
    }

    row = event->y - view->data_area.y;
    col = event->x - view->data_area.x;
    if (msg == MSG_MOUSE_DOWN
        && (row < 0 || row >= view->data_area.lines || col < 0 || col >= view->data_area.cols))
        return FALSE;

    row = CLAMP (row, 0, MAX (sel->rows - 1, 0));
    col = CLAMP (col, 0, MAX (sel->cols - 1, 0));

    if (!mcview_selection_point_on_row (sel, row, col, &at))
    {
        /* A drag may leave the data at EOF; keep extending to its last visible character. */
        if (msg != MSG_MOUSE_DRAG || !mcview_selection_last (sel, &at))
        {
            if (msg == MSG_MOUSE_DOWN)
            {
                mcview_selection_clear (view);
                sel->pressed = TRUE;
            }
            return TRUE;
        }
    }

    if (msg == MSG_MOUSE_DOWN)
    {
        mcview_selection_clear (view);
        sel->pressed = TRUE;
        sel->dragging = TRUE;
        sel->anchored = TRUE;
        sel->anchor = at;
        sel->point = at;
        sel->cursor = at;
        sel->cursor_on_screen = TRUE;

        if ((event->count & GPM_TRIPLE) != 0)
            mcview_selection_line (sel, &at);
        else if ((event->count & GPM_DOUBLE) != 0)
            mcview_selection_word (sel, &at);
    }
    else
    {
        sel->active = TRUE;
        sel->point = at;
        sel->cursor = at;
        sel->cursor_on_screen = TRUE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Returns TRUE when the command changed the selection or is consumed by an existing one, so an
   unused Shift key still reaches the command line under a quick view panel. */
gboolean
mcview_selection_command (WView *view, long command)
{
    struct mcview_selection *sel;
    mcview_selection_point_t target = { .valid = FALSE };
    gboolean found = FALSE;

    if (!mcview_selection_supported (view) || view->selection == NULL)
        return FALSE;

    sel = view->selection;

    if (command == CK_Unmark)
    {
        const gboolean active = sel->active;

        mcview_selection_clear (view);
        return active;
    }

    if (command == CK_MarkAll)
    {
        off_t size = mcview_get_filesize (view);

        if (view->force_max >= 0)
            size = MIN (size, (off_t) view->force_max);
        if (size <= 0)
            return FALSE;

        memset (&sel->anchor, 0, sizeof (sel->anchor));
        memset (&sel->point, 0, sizeof (sel->point));
        sel->anchor.valid = TRUE;
        sel->point.valid = TRUE;
        sel->point.from = size;
        sel->point.to = size;
        sel->anchored = TRUE;
        sel->active = TRUE;
        // No cell holds EOF: the cursor settles on the last visible character.
        sel->cursor = sel->point;
        sel->cursor.row = sel->rows - 1;
        sel->cursor.col = sel->cols - 1;
        sel->cursor_on_screen = FALSE;
        return TRUE;
    }

    switch (command)
    {
    case CK_MarkLeft:
    case CK_MarkRight:
    case CK_MarkUp:
    case CK_MarkDown:
    case CK_MarkPageUp:
    case CK_MarkPageDown:
    case CK_MarkToHome:
    case CK_MarkToEnd:
        break;
    case CK_Up:
    case CK_Down:
    case CK_Left:
    case CK_Right:
    case CK_LeftQuick:
    case CK_RightQuick:
        return mcview_selection_move_cursor (view, sel, command);
    default:
        return FALSE;
    }

    if (!mcview_selection_locate_cursor (sel))
        return FALSE;

    switch (command)
    {
    case CK_MarkLeft:
        found = mcview_selection_next (sel, &sel->cursor, FALSE, &target);
        break;
    case CK_MarkRight:
        found = mcview_selection_next (sel, &sel->cursor, TRUE, &target);
        break;
    case CK_MarkUp:
        found = mcview_selection_point_on_row (sel, sel->cursor.row - 1, sel->cursor.col, &target);
        break;
    case CK_MarkDown:
        found = mcview_selection_point_on_row (sel, sel->cursor.row + 1, sel->cursor.col, &target);
        break;
    case CK_MarkPageUp:
        found = mcview_selection_point_on_row (sel, 0, sel->cursor.col, &target);
        break;
    case CK_MarkPageDown:
        found = mcview_selection_point_on_row (sel, sel->rows - 1, sel->cursor.col, &target);
        if (!found)
            found = mcview_selection_last (sel, &target);
        break;
    case CK_MarkToHome:
        found = mcview_selection_first_on_row (sel, sel->cursor.row, &target);
        break;
    case CK_MarkToEnd:
        found = mcview_selection_last_on_row (sel, sel->cursor.row, &target);
        break;
    default:
        break;
    }

    if (found && mcview_selection_same_char (&sel->cursor, &target))
        found = FALSE;
    if (!found)
        found = mcview_selection_scroll (view, sel, command, &target);
    if (!found)
        return sel->active;

    if (!sel->anchored)
    {
        sel->anchored = TRUE;
        sel->anchor = sel->cursor;
    }
    sel->cursor = target;
    sel->point = target;
    sel->active = TRUE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The selected text as it is displayed: same character replacement and charset as the screen,
   so the clipfile holds bytes of the terminal charset like the editor and the input line. */
char *
mcview_selection_text (WView *view)
{
    mcview_selection_point_t first;
    mcview_selection_point_t last;
    mcview_state_machine_t state;
    GString *text;

    if (view == NULL || !mcview_selection_bounds (view->selection, &first, &last))
        return NULL;

    mcview_state_machine_init (&state, first.from);
    state.unwrapped_column = first.column;
    text = g_string_new (NULL);

    while (state.offset < last.to)
    {
        mcview_state_machine_t before = state;
        int cs[1 + MAX_COMBINING_CHARS];
        char str[(1 + MAX_COMBINING_CHARS) * MB_LEN_MAX + 1];
        int n;
        int i;
        int charwidth = 0;

        n = mcview_next_combining_char_sequence (view, &state, cs, G_N_ELEMENTS (cs), NULL);
        if (n == 0 || state.offset <= before.offset)
            break;

        if (cs[0] == '\n')
        {
            mcview_ansi_state_t ansi = state.ansi;

            g_string_append_c (text, '\n');
            mcview_state_machine_init (&state, state.offset);
            state.ansi = ansi;
            if (!mcview_selection_next_line (view, &state))
                break;
            continue;
        }

        if (mcview_is_non_spacing_mark (view, cs[0]))
            continue;

        if ((!mcview_isprint (view, cs[0]) || mcview_ismark (view, cs[0])) && cs[0] != '\t')
            cs[0] = '.';

        for (i = 0; i < n; i++)
            charwidth += mcview_wcwidth (view, cs[i]);

        if (cs[0] == '\t')
        {
            charwidth = option_tab_spacing - state.unwrapped_column % option_tab_spacing;
            for (i = 0; i < charwidth; i++)
                g_string_append_c (text, ' ');
            state.print_lonely_combining = TRUE;
        }
        else
        {
            int j = 0;

            for (i = 0; i < n; i++)
                j += mcview_char_display (view, cs[i], str + j);
            g_string_append_len (text, str, j);
            state.print_lonely_combining = FALSE;
        }

        state.unwrapped_column += charwidth;
    }

    return g_string_free (text, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Copy the selection to the clipfile and the external clipboard. The selection is dropped
   either way: one whose text is gone (a truncated file) must not stay on screen. */
gboolean
mcview_selection_store (WView *view)
{
    char *text = mcview_selection_text (view);
    gboolean copied = text != NULL && *text != '\0';

    if (copied)
    {
        mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file", text);
        mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_to_ext_clip", NULL);
    }
    g_free (text);

    mcview_selection_clear (view);

    return copied;
}

/* --------------------------------------------------------------------------------------------- */
