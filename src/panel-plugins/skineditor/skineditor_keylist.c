/*
   Skin editor plugin - the list of skin keys.

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
#include "lib/strutil.h"
#include "lib/tty/key.h"
#include "lib/tty/color.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "skineditor_keylist.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* the character of a CHAR entry and a piece of a STRING entry go at the right edge */
#define VALUE_COLS 8

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static const skinkeylist_row_t *
row_at (const WSkinKeyList *l, int i)
{
    if (i < 0 || i >= (int) l->rows->len)
        return NULL;
    return &g_array_index (l->rows, skinkeylist_row_t, i);
}

/* --------------------------------------------------------------------------------------------- */

static void
build_rows (WSkinKeyList *l)
{
    guint si, ei;

    g_array_set_size (l->rows, 0);
    for (si = 0; si < l->model->sections->len; si++)
    {
        skinedit_section_t *s = g_ptr_array_index (l->model->sections, si);
        skinkeylist_row_t r = { s, NULL };

        g_array_append_val (l->rows, r);
        for (ei = 0; ei < s->entries->len; ei++)
        {
            r.entry = g_ptr_array_index (s->entries, ei);
            g_array_append_val (l->rows, r);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
keep_current_visible (WSkinKeyList *l)
{
    int lines = WIDGET (l)->rect.lines;

    if (l->current < 0)
        return;
    if (l->current < l->top)
        l->top = l->current;
    if (l->current >= l->top + lines)
        l->top = l->current - lines + 1;
    /* show the heading of the section when the cursor sits right under it */
    if (l->current > 0 && l->current - 1 == l->top - 1 && row_at (l, l->current - 1)->entry == NULL)
        l->top = l->current - 1;
    if (l->top < 0)
        l->top = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
set_current (WSkinKeyList *l, int i)
{
    if (i == l->current)
        return;
    l->current = i;
    keep_current_visible (l);
    widget_draw (WIDGET (l));
    if (l->on_change != NULL)
        l->on_change (l, l->data);
}

/* --------------------------------------------------------------------------------------------- */

/* the next entry row from @from in direction @dir, headings skipped; -1 if none */

static int
next_entry (const WSkinKeyList *l, int from, int dir)
{
    int i;

    for (i = from + dir; i >= 0 && i < (int) l->rows->len; i += dir)
        if (row_at (l, i)->entry != NULL)
            return i;
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static void
move_by (WSkinKeyList *l, int delta)
{
    int i = l->current;
    int dir = delta < 0 ? -1 : 1;
    int n;

    for (n = 0; n < (delta < 0 ? -delta : delta); n++)
    {
        int j = next_entry (l, i, dir);

        if (j < 0)
            break;
        i = j;
    }
    if (i >= 0)
        set_current (l, i);
}

/* --------------------------------------------------------------------------------------------- */

/* the heading row of the section the row @i belongs to */

static int
heading_of (const WSkinKeyList *l, int i)
{
    while (i > 0 && row_at (l, i)->entry != NULL)
        i--;
    return i;
}

/* --------------------------------------------------------------------------------------------- */

static void
move_section (WSkinKeyList *l, int dir)
{
    int heading, target = -1;

    if (l->current < 0)
        return;
    heading = heading_of (l, l->current);

    if (dir > 0)
    {
        int i;

        for (i = l->current + 1; i < (int) l->rows->len; i++)
            if (row_at (l, i)->entry == NULL)
            {
                target = next_entry (l, i, 1);
                break;
            }
    }
    else if (next_entry (l, heading, 1) != l->current)
        // not at the first entry yet: go there
        target = next_entry (l, heading, 1);
    else if (heading > 0)
        target = next_entry (l, heading_of (l, heading - 1), 1);

    if (target >= 0)
    {
        set_current (l, target);
        // the heading of the section at the top, the section below it
        l->top = heading_of (l, target);
        keep_current_visible (l);
        widget_draw (WIDGET (l));
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
draw_row (WSkinKeyList *l, int y, const skinkeylist_row_t *r, gboolean focused)
{
    Widget *w = WIDGET (l);
    const int *colors = widget_get_colors (w);
    int cols = w->rect.cols;
    gboolean is_current = (r != NULL && r->entry != NULL && row_at (l, l->current) == r);

    if (r == NULL)
    {
        tty_setcolor (colors[DLG_COLOR_NORMAL]);
        tty_draw_hline (w->rect.y + y, w->rect.x, ' ', cols);
        return;
    }

    if (r->entry == NULL)
    {
        char *text;

        tty_setcolor (colors[DLG_COLOR_HOT_NORMAL]);
        tty_draw_hline (w->rect.y + y, w->rect.x, ' ', cols);
        widget_gotoyx (w, y, 0);
        text = g_strdup_printf ("%s%s", r->section->label,
                                skinedit_section_changed (r->section) ? " *" : "");
        tty_print_string (str_fit_to_term (text, cols, J_LEFT_FIT));
        g_free (text);
        return;
    }

    // like a listbox: the current row in the selected color, the rest in dialog colors
    tty_setcolor (is_current
                      ? colors[focused ? DLG_COLOR_SELECTED_FOCUS : DLG_COLOR_SELECTED_NORMAL]
                      : colors[DLG_COLOR_NORMAL]);
    tty_draw_hline (w->rect.y + y, w->rect.x, ' ', cols);
    widget_gotoyx (w, y, 0);
    tty_print_string (is_current ? ">" : " ");
    // a changed key, until Save
    tty_print_string (skinedit_entry_changed (r->entry) ? "*" : " ");

    if (r->entry->kind == SKINEDIT_ENTRY_COLOR)
    {
        widget_gotoyx (w, y, 2);
        tty_print_string (str_fit_to_term (r->entry->label, cols - 2, J_LEFT_FIT));
    }
    else
    {
        const char *value;
        int label_cols = cols - 2 - VALUE_COLS;

        widget_gotoyx (w, y, 2);
        tty_print_string (str_fit_to_term (r->entry->label, label_cols, J_LEFT_FIT));
        value = r->entry->shown != NULL ? r->entry->shown : "";
        widget_gotoyx (w, y, 2 + label_cols);
        tty_print_string (str_fit_to_term (value, VALUE_COLS, J_RIGHT_FIT));
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
keylist_draw (WSkinKeyList *l, gboolean focused)
{
    Widget *w = WIDGET (l);
    int y;

    for (y = 0; y < w->rect.lines; y++)
        draw_row (l, y, row_at (l, l->top + y), focused);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
keylist_key (WSkinKeyList *l, int key)
{
    int lines = WIDGET (l)->rect.lines;

    switch (key)
    {
    case KEY_UP:
        move_by (l, -1);
        return MSG_HANDLED;
    case KEY_DOWN:
        move_by (l, 1);
        return MSG_HANDLED;
    case KEY_PPAGE:
        move_by (l, -(lines - 1));
        return MSG_HANDLED;
    case KEY_NPAGE:
        move_by (l, lines - 1);
        return MSG_HANDLED;
    case KEY_HOME:
    case ALT ('<'):
        set_current (l, next_entry (l, -1, 1));
        return MSG_HANDLED;
    case KEY_END:
    case ALT ('>'):
        set_current (l, next_entry (l, (int) l->rows->len, -1));
        return MSG_HANDLED;
    case KEY_M_CTRL | KEY_PPAGE:
        move_section (l, -1);
        return MSG_HANDLED;
    case KEY_M_CTRL | KEY_NPAGE:
        move_section (l, 1);
        return MSG_HANDLED;
    case '\n':
    case KEY_ENTER:
    case KEY_F (4):
        if (l->on_edit != NULL && l->current >= 0)
            l->on_edit (l, l->data);
        return MSG_HANDLED;
    case ' ':
        if (l->on_show != NULL && l->current >= 0)
            l->on_show (l, l->data);
        return MSG_HANDLED;
    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
keylist_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WSkinKeyList *l = SKINKEYLIST (w);

    switch (msg)
    {
    case MSG_KEY:
        return keylist_key (l, parm);

    case MSG_CURSOR:
        if (l->current >= 0)
            widget_gotoyx (w, l->current - l->top, 0);
        return MSG_HANDLED;

    case MSG_DRAW:
        keylist_draw (l, widget_get_state (w, WST_FOCUSED));
        return MSG_HANDLED;

    case MSG_RESIZE:
        widget_default_callback (w, sender, msg, parm, data);
        keep_current_visible (l);
        return MSG_HANDLED;

    case MSG_DESTROY:
        g_array_free (l->rows, TRUE);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
keylist_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WSkinKeyList *l = SKINKEYLIST (w);

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
    {
        const skinkeylist_row_t *r;

        widget_select (w);
        r = row_at (l, l->top + event->y);
        if (r != NULL && r->entry != NULL)
            set_current (l, l->top + event->y);
        break;
    }

    case MSG_MOUSE_CLICK:
        if (event->count == GPM_DOUBLE && l->on_edit != NULL && l->current >= 0
            && l->top + event->y == l->current)
            l->on_edit (l, l->data);
        break;

    case MSG_MOUSE_SCROLL_UP:
        move_by (l, -3);
        break;

    case MSG_MOUSE_SCROLL_DOWN:
        move_by (l, 3);
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WSkinKeyList *
skinkeylist_new (int y, int x, int lines, int cols, skinedit_model_t *model)
{
    WSkinKeyList *l;
    Widget *w;
    WRect r = { y, x, lines, cols };

    l = g_new0 (WSkinKeyList, 1);
    w = WIDGET (l);
    widget_init (w, &r, keylist_callback, keylist_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_WANT_CURSOR;

    l->model = model;
    l->rows = g_array_new (FALSE, FALSE, sizeof (skinkeylist_row_t));
    build_rows (l);
    l->current = next_entry (l, -1, 1);
    l->top = 0;
    return l;
}

/* --------------------------------------------------------------------------------------------- */

void
skinkeylist_set_model (WSkinKeyList *l, skinedit_model_t *model)
{
    l->model = model;
    build_rows (l);
    l->current = next_entry (l, -1, 1);
    l->top = 0;
    widget_draw (WIDGET (l));
    if (l->on_change != NULL)
        l->on_change (l, l->data);
}

/* --------------------------------------------------------------------------------------------- */

skinedit_entry_t *
skinkeylist_current (const WSkinKeyList *l)
{
    const skinkeylist_row_t *r = row_at (l, l->current);

    return r != NULL ? r->entry : NULL;
}

/* --------------------------------------------------------------------------------------------- */

skinedit_section_t *
skinkeylist_current_section (const WSkinKeyList *l)
{
    const skinkeylist_row_t *r = row_at (l, l->current);

    return r != NULL ? r->section : NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
skinkeylist_goto_entry (WSkinKeyList *l, const skinedit_entry_t *e)
{
    int i;

    for (i = 0; i < (int) l->rows->len; i++)
        if (row_at (l, i)->entry == e)
        {
            set_current (l, i);
            return;
        }
}

/* --------------------------------------------------------------------------------------------- */
