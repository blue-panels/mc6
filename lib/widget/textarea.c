/*
   A field of several lines.

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

/** \file textarea.c
 *  \brief Source: WTextArea widget
 *
 *  What WInput is to a line, this is to a text: a field of several lines that
 *  types, splits and joins them.  It is a field, not an editor: no marking, no
 *  search, no undo.  Enough for a script somebody writes in a dialog.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/event.h"
#include "lib/tty/key.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
textarea_line (const WTextArea *area, int line)
{
    if (line < 0 || line >= (int) area->lines->len)
        return NULL;

    return g_ptr_array_index (area->lines, line);
}

/* --------------------------------------------------------------------------------------------- */

static char *
textarea_current_line (const WTextArea *area)
{
    return textarea_line (area, area->line);
}

/* --------------------------------------------------------------------------------------------- */

/** The byte the character number stands at. */
static int
textarea_offset (const char *text, int point)
{
    if (point <= 0)
        return 0;

    return str_offset_to_pos (text, (size_t) point);
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_replace_line (WTextArea *area, int line, char *text)
{
    g_free (g_ptr_array_index (area->lines, line));
    g_ptr_array_index (area->lines, line) = text;
}

/* --------------------------------------------------------------------------------------------- */

/** Keep the cursor on the screen and the screen around the cursor. */
static void
textarea_adjust (WTextArea *area)
{
    Widget *w = WIDGET (area);
    const char *line;
    int length;
    int width;

    if (area->line < 0)
        area->line = 0;
    if (area->line >= (int) area->lines->len)
        area->line = (int) area->lines->len - 1;

    line = textarea_current_line (area);
    length = line == NULL ? 0 : str_length (line);

    if (area->point < 0)
        area->point = 0;
    if (area->point > length)
        area->point = length;

    if (area->line < area->top)
        area->top = area->line;
    if (area->line >= area->top + w->rect.lines)
        area->top = area->line - w->rect.lines + 1;
    if (area->top < 0)
        area->top = 0;

    // one column is kept for the cursor sitting after the last character
    width = w->rect.cols - 1;
    if (width < 1)
        width = 1;

    if (area->point < area->left)
        area->left = area->point;
    if (area->point > area->left + width)
        area->left = area->point - width;
    if (area->left < 0)
        area->left = 0;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * A character arrives byte by byte, as the terminal sends it.  The bytes are
 * kept until they make a character, which is what an input line does with them
 * as well: a letter of two or three bytes typed into the field is one letter,
 * not two or three broken ones.
 */
static void
textarea_insert_char (WTextArea *area, int byte)
{
    char *line;
    char *result;
    int offset;
    int valid;

    if (area->charpoint >= MB_LEN_MAX)
        area->charpoint = 0;

    area->charbuf[area->charpoint] = (char) byte;
    area->charpoint++;

    valid = str_is_valid_char (area->charbuf, area->charpoint);
    if (valid < 0)
    {
        // -2 is a character that has not arrived whole yet; anything else broke
        if (valid != -2)
            area->charpoint = 0;
        return;
    }

    line = textarea_current_line (area);
    if (line == NULL)
    {
        area->charpoint = 0;
        return;
    }

    offset = textarea_offset (line, area->point);
    result = g_strdup_printf ("%.*s%.*s%s", offset, line, (int) area->charpoint, area->charbuf,
                              line + offset);
    textarea_replace_line (area, area->line, result);
    area->point++;
    area->charpoint = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_split_line (WTextArea *area)
{
    char *line;
    int offset;

    line = textarea_current_line (area);
    if (line == NULL)
        return;

    offset = textarea_offset (line, area->point);

    g_ptr_array_insert (area->lines, area->line + 1, g_strdup (line + offset));
    textarea_replace_line (area, area->line, g_strndup (line, (gsize) offset));

    area->line++;
    area->point = 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_join_line (WTextArea *area, int line)
{
    char *first, *second, *result;

    first = textarea_line (area, line);
    second = textarea_line (area, line + 1);
    if (first == NULL || second == NULL)
        return;

    result = g_strconcat (first, second, (char *) NULL);
    textarea_replace_line (area, line, result);
    g_ptr_array_remove_index (area->lines, line + 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_delete_char (WTextArea *area)
{
    char *line;
    int offset, next;
    char *result;

    line = textarea_current_line (area);
    if (line == NULL)
        return;

    if (area->point >= str_length (line))
    {
        // at the end of a line the next line comes up to it
        textarea_join_line (area, area->line);
        return;
    }

    offset = textarea_offset (line, area->point);
    next = textarea_offset (line, area->point + 1);
    result = g_strdup_printf ("%.*s%s", offset, line, line + next);
    textarea_replace_line (area, area->line, result);
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_backspace (WTextArea *area)
{
    if (area->point > 0)
    {
        area->point--;
        textarea_delete_char (area);
        return;
    }

    if (area->line > 0)
    {
        const char *previous;

        previous = textarea_line (area, area->line - 1);
        area->line--;
        area->point = previous == NULL ? 0 : str_length (previous);
        textarea_join_line (area, area->line);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * The marked text runs from where the mark was put to where the cursor is, in
 * whichever order.  Both ends come back sorted.
 */
static gboolean
textarea_marked_range (const WTextArea *area, int *from_line, int *from_point, int *to_line,
                       int *to_point)
{
    if (area->mark_line < 0)
        return FALSE;

    if (area->mark_line == area->line && area->mark_point == area->point)
        return FALSE;

    if (area->mark_line < area->line
        || (area->mark_line == area->line && area->mark_point < area->point))
    {
        *from_line = area->mark_line;
        *from_point = area->mark_point;
        *to_line = area->line;
        *to_point = area->point;
    }
    else
    {
        *from_line = area->line;
        *from_point = area->point;
        *to_line = area->mark_line;
        *to_point = area->mark_point;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/** Which characters of one row are marked; FALSE when none of it is. */
static gboolean
textarea_marked_row (const WTextArea *area, int row, int *from, int *to)
{
    int fl, fp, tl, tp;
    const char *line;
    int length;

    if (!textarea_marked_range (area, &fl, &fp, &tl, &tp))
        return FALSE;

    if (row < fl || row > tl)
        return FALSE;

    line = textarea_line (area, row);
    length = line == NULL ? 0 : str_length (line);

    *from = row == fl ? fp : 0;
    *to = row == tl ? tp : length;

    return *to > *from;
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_paste (WTextArea *area, const char *text)
{
    const char *p;

    if (text == NULL)
        return;

    for (p = text; *p != '\0'; p++)
        if (*p == '\n')
            textarea_split_line (area);
        else if (*p != '\r')
            textarea_insert_char (area, (unsigned char) *p);
}

/* --------------------------------------------------------------------------------------------- */

/** Take the marked text through the clipboard file, the way an input line does. */
static void
textarea_copy_to_clip (WTextArea *area)
{
    char *text;

    text = textarea_get_marked (area);
    if (text == NULL)
        return;

    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_to_file", text);
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_to_ext_clip", NULL);
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_paste_from_clip (WTextArea *area)
{
    char *text = NULL;
    ev_clipboard_text_from_file_t event_data = { NULL, FALSE };

    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_file_from_ext_clip", NULL);

    event_data.text = &text;
    mc_event_raise (MCEVENT_GROUP_CORE, "clipboard_text_from_file", &event_data);

    if (event_data.ret && text != NULL)
    {
        textarea_delete_marked (area);
        textarea_paste (area, text);
    }

    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
textarea_key (WTextArea *area, int key)
{
    Widget *w = WIDGET (area);
    const char *line;

    // Shift and a motion mark what the motion goes over; a motion of its own
    // drops the mark, the way an input line does it.
    if ((key & KEY_M_SHIFT) != 0)
    {
        int plain = key & ~KEY_M_SHIFT;

        switch (plain)
        {
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_UP:
        case KEY_DOWN:
        case KEY_HOME:
        case KEY_END:
        case KEY_PPAGE:
        case KEY_NPAGE:
            if (area->mark_line < 0)
                textarea_mark (area, TRUE);
            key = plain;
            break;
        default:
            break;
        }
    }
    else if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN
             || key == KEY_HOME || key == KEY_END || key == KEY_A1 || key == KEY_C1
             || key == KEY_PPAGE || key == KEY_NPAGE || key == XCTRL ('n') || key == XCTRL ('p')
             || key == XCTRL ('a') || key == XCTRL ('e'))
        area->mark_line = -1;

    switch (key)
    {
    case KEY_M_CTRL | KEY_IC:  // Ctrl-Insert: copy
        textarea_copy_to_clip (area);
        // the text is taken; keeping it marked would only ask to be typed over
        area->mark_line = -1;
        return MSG_HANDLED;

    case KEY_M_SHIFT | KEY_IC:  // Shift-Insert: paste
        textarea_paste_from_clip (area);
        return MSG_HANDLED;

    case KEY_M_SHIFT | KEY_DC:  // Shift-Delete: cut
        textarea_copy_to_clip (area);
        textarea_delete_marked (area);
        return MSG_HANDLED;

    default:
        break;
    }

    switch (key)
    {
    case KEY_LEFT:
        if (area->point > 0)
            area->point--;
        else if (area->line > 0)
        {
            area->line--;
            line = textarea_current_line (area);
            area->point = line == NULL ? 0 : str_length (line);
        }
        return MSG_HANDLED;

    case KEY_RIGHT:
        line = textarea_current_line (area);
        if (line != NULL && area->point < str_length (line))
            area->point++;
        else if (area->line + 1 < (int) area->lines->len)
        {
            area->line++;
            area->point = 0;
        }
        return MSG_HANDLED;

    case XCTRL ('p'):
    case KEY_UP:
        // the field keeps the key even with nowhere to go: it is a text, and
        // the cursor of a text does not step out of it into the next widget
        if (area->line > 0)
            area->line--;
        return MSG_HANDLED;

    case XCTRL ('n'):
    case KEY_DOWN:
        if (area->line + 1 < (int) area->lines->len)
            area->line++;
        return MSG_HANDLED;

    case XCTRL ('a'):
    case KEY_HOME:
    case KEY_A1:
        area->point = 0;
        return MSG_HANDLED;

    case XCTRL ('e'):
    case KEY_END:
    case KEY_C1:
        line = textarea_current_line (area);
        area->point = line == NULL ? 0 : str_length (line);
        return MSG_HANDLED;

    case KEY_PPAGE:
        area->line -= w->rect.lines;
        return MSG_HANDLED;

    case KEY_NPAGE:
        area->line += w->rect.lines;
        return MSG_HANDLED;

    case KEY_BACKSPACE:
        if (area->mark_line >= 0)
            textarea_delete_marked (area);
        else
            textarea_backspace (area);
        return MSG_HANDLED;

    case KEY_DC:
        if (area->mark_line >= 0)
            textarea_delete_marked (area);
        else
            textarea_delete_char (area);
        return MSG_HANDLED;

    case '\n':
    case '\r':
    case KEY_ENTER:
        // as with a character typed over it: what is marked goes first, or the
        // mark would point into lines that have moved under it
        textarea_delete_marked (area);
        textarea_split_line (area);
        return MSG_HANDLED;

    default:
        break;
    }

    // Every byte of a character has to come in, and half of them are not
    // printable on their own: what is a character is decided when it is whole.
    if (key < 32 || key > 255)
        return MSG_NOT_HANDLED;

    // what is marked goes away under the first character typed over it
    textarea_delete_marked (area);
    textarea_insert_char (area, key);

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_draw (WTextArea *area)
{
    Widget *w = WIDGET (area);
    gboolean focused;
    int i;

    focused = widget_get_state (w, WST_FOCUSED);
    tty_setcolor (focused ? CORE_INPUT_COLOR : CORE_INPUT_UNCHANGED_COLOR);

    for (i = 0; i < w->rect.lines; i++)
    {
        const char *line;
        const char *from;

        // tty_draw_hline() wants where to draw: with -1 it draws nothing at
        // all, and the row would keep whatever stood there before.
        tty_draw_hline (w->rect.y + i, w->rect.x, ' ', w->rect.cols);
        widget_gotoyx (w, i, 0);

        line = textarea_line (area, area->top + i);
        if (line == NULL)
            continue;

        // the scroll belongs to the field, and a shorter row has nothing there
        if (area->left >= str_length (line))
            continue;

        from = line + textarea_offset (line, area->left);
        tty_print_string (str_fit_to_term (from, w->rect.cols, J_LEFT));

        // what is marked, drawn over what was just drawn
        {
            int mark_from, mark_to;

            if (textarea_marked_row (area, area->top + i, &mark_from, &mark_to))
            {
                int start, end;
                char *part;

                start = MAX (mark_from, area->left);
                end = MIN (mark_to, area->left + w->rect.cols);

                if (end > start)
                {
                    part = g_strndup (
                        line + textarea_offset (line, start),
                        (gsize) (textarea_offset (line, end) - textarea_offset (line, start)));
                    tty_setcolor (input_colors[WINPUTC_MARK]);
                    widget_gotoyx (w, i, start - area->left);
                    tty_print_string (part);
                    tty_setcolor (focused ? CORE_INPUT_COLOR : CORE_INPUT_UNCHANGED_COLOR);
                    g_free (part);
                }
            }
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
textarea_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WTextArea *area = TEXTAREA (w);

    switch (msg)
    {
    case MSG_KEY:
    {
        cb_ret_t ret;

        ret = textarea_key (area, parm);
        if (ret == MSG_HANDLED)
        {
            textarea_adjust (area);
            widget_draw (w);
        }
        return ret;
    }

    case MSG_CURSOR:
        widget_gotoyx (w, area->line - area->top, area->point - area->left);
        return MSG_HANDLED;

    case MSG_DRAW:
        textarea_adjust (area);
        textarea_draw (area);
        return MSG_HANDLED;

    case MSG_RESIZE:
        w->rect = *(WRect *) data;
        textarea_adjust (area);
        return MSG_HANDLED;

    case MSG_DESTROY:
        g_ptr_array_free (area->lines, TRUE);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
textarea_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WTextArea *area = TEXTAREA (w);

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        // the click takes the focus as well, the way an input line does
        widget_select (w);
        area->line = area->top + event->y;
        area->point = area->left + event->x;
        textarea_adjust (area);
        textarea_mark (area, TRUE);  // a drag from here marks
        widget_draw (w);
        break;

    case MSG_MOUSE_DRAG:
        area->line = area->top + event->y;
        area->point = area->left + event->x;
        textarea_adjust (area);
        widget_draw (w);
        break;

    case MSG_MOUSE_SCROLL_UP:
        area->line--;
        textarea_adjust (area);
        widget_draw (w);
        break;

    case MSG_MOUSE_SCROLL_DOWN:
        area->line++;
        textarea_adjust (area);
        widget_draw (w);
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WTextArea *
textarea_new (int y, int x, int lines, int cols, const char *text)
{
    WRect r = { y, x, lines, cols };
    WTextArea *area;
    Widget *w;

    area = g_new0 (WTextArea, 1);
    w = WIDGET (area);
    widget_init (w, &r, textarea_callback, textarea_mouse_callback);
    // WOP_IS_INPUT: a letter typed here is a letter, not the hotkey of a button
    w->options |= WOP_SELECTABLE | WOP_WANT_CURSOR | WOP_IS_INPUT;
    w->keymap = NULL;

    area->lines = g_ptr_array_new_with_free_func (g_free);
    textarea_set_text (area, text);

    return area;
}

/* --------------------------------------------------------------------------------------------- */

void
textarea_mark (WTextArea *area, gboolean on)
{
    area->mark_line = on ? area->line : -1;
    area->mark_point = on ? area->point : 0;
}

/* --------------------------------------------------------------------------------------------- */

char *
textarea_get_marked (const WTextArea *area)
{
    int fl, fp, tl, tp;
    GString *text;
    int i;

    if (!textarea_marked_range (area, &fl, &fp, &tl, &tp))
        return NULL;

    text = g_string_new ("");

    for (i = fl; i <= tl; i++)
    {
        const char *line;
        int from, to;

        line = textarea_line (area, i);
        if (line == NULL)
            break;

        from = i == fl ? textarea_offset (line, fp) : 0;
        to = i == tl ? textarea_offset (line, tp) : (int) strlen (line);

        if (i != fl)
            g_string_append_c (text, '\n');
        g_string_append_len (text, line + from, to - from);
    }

    return g_string_free (text, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
textarea_delete_marked (WTextArea *area)
{
    int fl, fp, tl, tp;
    const char *first, *last;
    char *result;

    if (!textarea_marked_range (area, &fl, &fp, &tl, &tp))
    {
        area->mark_line = -1;
        return;
    }

    first = textarea_line (area, fl);
    last = textarea_line (area, tl);

    if (first != NULL && last != NULL)
    {
        result = g_strdup_printf ("%.*s%s", textarea_offset (first, fp), first,
                                  last + textarea_offset (last, tp));
        textarea_replace_line (area, fl, result);

        while (tl > fl)
        {
            g_ptr_array_remove_index (area->lines, fl + 1);
            tl--;
        }
    }

    area->line = fl;
    area->point = fp;
    area->mark_line = -1;
}

/* --------------------------------------------------------------------------------------------- */

void
textarea_set_text (WTextArea *area, const char *text)
{
    gchar **lines;
    guint i;

    g_ptr_array_set_size (area->lines, 0);

    lines = g_strsplit (text != NULL ? text : "", "\n", -1);
    for (i = 0; lines[i] != NULL; i++)
        g_ptr_array_add (area->lines, g_strdup (lines[i]));
    g_strfreev (lines);

    if (area->lines->len == 0)
        g_ptr_array_add (area->lines, g_strdup (""));

    area->line = 0;
    area->point = 0;
    area->top = 0;
    area->left = 0;
    area->mark_line = -1;
    area->mark_point = 0;
}

/* --------------------------------------------------------------------------------------------- */

char *
textarea_get_text (const WTextArea *area)
{
    GString *text;
    guint i;

    text = g_string_new ("");

    for (i = 0; i < area->lines->len; i++)
    {
        if (i != 0)
            g_string_append_c (text, '\n');
        g_string_append (text, (const char *) g_ptr_array_index (area->lines, i));
    }

    return g_string_free (text, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
