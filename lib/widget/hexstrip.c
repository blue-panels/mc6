/*
   Widgets for the Midnight Commander

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

/** \file hexstrip.c
 *  \brief Source: WHexStrip, a few rows of hex dump over a byte source
 */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/widget.h"

#include "hexstrip.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef enum
{
    MARK_NORMAL,
    MARK_SELECTED,
    MARK_BLOCK,
    MARK_CHANGED,
    MARK_CURSOR
} mark_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static const char hex_char[] = "0123456789ABCDEF";

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static off_t
source_size (const WHexStrip *h)
{
    if (h->source.get_size == NULL)
        return 0;
    return h->source.get_size (h->source.ctx);
}

/* --------------------------------------------------------------------------------------------- */

static off_t
row_align (const WHexStrip *h, off_t offset)
{
    return offset - offset % h->bytes_per_line;
}

/* --------------------------------------------------------------------------------------------- */

static void
clamp_top (WHexStrip *h)
{
    off_t size = source_size (h);
    off_t last_row;

    if (h->top < 0)
        h->top = 0;
    last_row = size > 0 ? row_align (h, size - 1) : 0;
    if (h->top > last_row)
        h->top = last_row;
}

/* --------------------------------------------------------------------------------------------- */

static void
ensure_cursor_visible (WHexStrip *h)
{
    int lines = WIDGET (h)->rect.lines;
    off_t page = (off_t) lines * h->bytes_per_line;

    if (h->cursor < h->top)
        h->top = row_align (h, h->cursor);
    else if (h->cursor >= h->top + page)
        h->top = row_align (h, h->cursor) - (off_t) (lines - 1) * h->bytes_per_line;
    clamp_top (h);
}

/* --------------------------------------------------------------------------------------------- */

static void
redraw (WHexStrip *h)
{
    Widget *w = WIDGET (h);

    if (w->owner != NULL)
        widget_draw (w);
}

/* --------------------------------------------------------------------------------------------- */

static void
cursor_moved (WHexStrip *h)
{
    ensure_cursor_visible (h);
    redraw (h);
    if (h->on_cursor != NULL)
        h->on_cursor (h, h->data);
}

/* --------------------------------------------------------------------------------------------- */

static mark_t
byte_mark (const WHexStrip *h, off_t offset)
{
    if (offset == h->cursor)
        return MARK_CURSOR;
    if (h->source.is_changed != NULL && h->source.is_changed (h->source.ctx, offset))
        return MARK_CHANGED;
    if (h->block_len > 0 && offset >= h->block_start && offset < h->block_start + h->block_len)
        return MARK_BLOCK;
    if (h->mark_len > 0 && offset >= h->mark_start && offset < h->mark_start + h->mark_len)
        return MARK_SELECTED;
    return MARK_NORMAL;
}

/* --------------------------------------------------------------------------------------------- */

static void
hexstrip_draw (WHexStrip *h, gboolean focused)
{
    Widget *w = WIDGET (h);
    const WRect *r = &w->rect;
    off_t size = source_size (h);
    unsigned char *row_buf;
    int row;
    int c_normal = h->colors.normal >= 0 ? h->colors.normal : VIEWER_NORMAL_COLOR;
    int c_offset = h->colors.offset >= 0 ? h->colors.offset : VIEWER_BOLD_COLOR;
    int c_mark = h->colors.mark >= 0 ? h->colors.mark : VIEWER_BOLD_COLOR;
    int c_changed = h->colors.changed >= 0 ? h->colors.changed : VIEWER_UNDERLINED_COLOR;
    int c_cursor = h->colors.cursor >= 0 ? h->colors.cursor : VIEWER_SELECTED_COLOR;
    int c_frame = h->colors.frame >= 0 ? h->colors.frame : VIEWER_FRAME_COLOR;
    int c_block = h->colors.block >= 0 ? h->colors.block : VIEWER_SELECTED_COLOR;

    hexstrip_layout (r->cols, &h->bytes_per_line, &h->text_start);
    clamp_top (h);
    h->cursor_y = -1;
    h->cursor_x = 0;

    row_buf = g_malloc (h->bytes_per_line);

    for (row = 0; row < r->lines; row++)
    {
        off_t row_off = h->top + (off_t) row * h->bytes_per_line;
        gssize got = 0;
        int i;
        char offs[16];

        tty_setcolor (c_normal);
        tty_draw_hline (r->y + row, r->x, ' ', r->cols);

        if (row_off >= size)
            continue;

        got = h->source.read (h->source.ctx, row_off, row_buf, h->bytes_per_line);
        if (got < 0)
            got = 0;
        if (row_off + got > size)
            got = (gssize) (size - row_off);

        widget_gotoyx (h, row, 0);
        tty_setcolor (c_offset);
        g_snprintf (offs, sizeof (offs), "%08llX", (unsigned long long) row_off);
        tty_print_string (offs);

        for (i = 0; i < got; i++)
        {
            int col = hexstrip_byte_col (r->cols, h->bytes_per_line, i);
            unsigned char c = row_buf[i];
            mark_t m = byte_mark (h, row_off + i);
            int color;

            switch (m)
            {
            case MARK_CURSOR:
                color = focused && !h->in_text ? c_cursor : c_changed;
                break;
            case MARK_CHANGED:
                color = c_changed;
                break;
            case MARK_SELECTED:
                color = c_mark;
                break;
            case MARK_BLOCK:
                color = c_block;
                break;
            default:
                color = c_normal;
                break;
            }

            if (col + 1 < r->cols)
            {
                widget_gotoyx (h, row, col);
                tty_setcolor (color);
                tty_print_char (hex_char[c >> 4]);
                tty_print_char (hex_char[c & 15]);
            }
            if (m == MARK_CURSOR && !h->in_text)
            {
                h->cursor_y = row;
                h->cursor_x = col;
            }

            if (i % 4 == 3 && i + 1 < h->bytes_per_line && r->cols >= 80)
            {
                widget_gotoyx (h, row, col + 3);
                tty_setcolor (c_frame);
                tty_print_one_vline (TRUE);
            }

            if (h->text_start + i < r->cols)
            {
                widget_gotoyx (h, row, h->text_start + i);
                switch (m)
                {
                case MARK_CURSOR:
                    tty_setcolor (focused && h->in_text ? c_cursor : c_changed);
                    break;
                case MARK_CHANGED:
                    tty_setcolor (c_changed);
                    break;
                case MARK_SELECTED:
                    tty_setcolor (c_mark);
                    break;
                case MARK_BLOCK:
                    tty_setcolor (c_block);
                    break;
                default:
                    tty_setcolor (c_normal);
                    break;
                }
                tty_print_char (c >= 0x20 && c < 0x7F ? (char) c : '.');
            }
            if (m == MARK_CURSOR && h->in_text)
            {
                h->cursor_y = row;
                h->cursor_x = h->text_start + i;
            }
        }
    }

    g_free (row_buf);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
edit_hex_digit (WHexStrip *h, int key)
{
    unsigned char c, d;
    off_t size = source_size (h);

    if (h->on_edit == NULL || h->cursor >= size)
        return FALSE;
    if (key >= '0' && key <= '9')
        d = key - '0';
    else if (key >= 'a' && key <= 'f')
        d = key - 'a' + 10;
    else if (key >= 'A' && key <= 'F')
        d = key - 'A' + 10;
    else
        return FALSE;

    if (h->source.read (h->source.ctx, h->cursor, &c, 1) != 1)
        return FALSE;
    if (!h->low_nibble)
    {
        c = (c & 0x0F) | (d << 4);
        if (!h->on_edit (h, h->cursor, c, h->data))
            return FALSE;
        h->low_nibble = TRUE;
        redraw (h);
    }
    else
    {
        c = (c & 0xF0) | d;
        if (!h->on_edit (h, h->cursor, c, h->data))
            return FALSE;
        h->low_nibble = FALSE;
        if (h->cursor + 1 < size)
            h->cursor++;
        cursor_moved (h);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
edit_text_char (WHexStrip *h, int key)
{
    off_t size = source_size (h);

    if (h->on_edit == NULL || h->cursor >= size || key < 0x20 || key > 0xFF)
        return FALSE;
    if (!h->on_edit (h, h->cursor, (unsigned char) key, h->data))
        return FALSE;
    if (h->cursor + 1 < size)
        h->cursor++;
    cursor_moved (h);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
hexstrip_key (WHexStrip *h, int key)
{
    int lines = WIDGET (h)->rect.lines;
    off_t size = source_size (h);

    switch (key)
    {
    case KEY_UP:
        hexstrip_move (h, -h->bytes_per_line);
        return MSG_HANDLED;
    case KEY_DOWN:
        hexstrip_move (h, h->bytes_per_line);
        return MSG_HANDLED;
    case KEY_LEFT:
        hexstrip_move (h, -1);
        return MSG_HANDLED;
    case KEY_RIGHT:
        hexstrip_move (h, 1);
        return MSG_HANDLED;
    case KEY_PPAGE:
        hexstrip_move (h, -(off_t) lines * h->bytes_per_line);
        return MSG_HANDLED;
    case KEY_NPAGE:
        hexstrip_move (h, (off_t) lines * h->bytes_per_line);
        return MSG_HANDLED;
    case KEY_HOME:
        hexstrip_set_cursor (h, 0);
        return MSG_HANDLED;
    case KEY_END:
        hexstrip_set_cursor (h, size > 0 ? size - 1 : 0);
        return MSG_HANDLED;
    case '\t':
        h->in_text = !h->in_text;
        h->low_nibble = FALSE;
        redraw (h);
        return MSG_HANDLED;
    default:
        break;
    }

    if (h->in_text ? edit_text_char (h, key) : edit_hex_digit (h, key))
        return MSG_HANDLED;
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
hexstrip_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WHexStrip *h = HEXSTRIP (w);

    switch (msg)
    {
    case MSG_KEY:
        return hexstrip_key (h, parm);

    case MSG_ACTION:
        switch (parm)
        {
        case CK_Up:
            return hexstrip_key (h, KEY_UP);
        case CK_Down:
            return hexstrip_key (h, KEY_DOWN);
        case CK_Left:
            return hexstrip_key (h, KEY_LEFT);
        case CK_Right:
            return hexstrip_key (h, KEY_RIGHT);
        case CK_PageUp:
            return hexstrip_key (h, KEY_PPAGE);
        case CK_PageDown:
            return hexstrip_key (h, KEY_NPAGE);
        case CK_Top:
            return hexstrip_key (h, KEY_HOME);
        case CK_Bottom:
            return hexstrip_key (h, KEY_END);
        default:
            return MSG_NOT_HANDLED;
        }

    case MSG_CURSOR:
        if (h->cursor_y >= 0)
            widget_gotoyx (h, h->cursor_y, h->cursor_x + (h->low_nibble && !h->in_text ? 1 : 0));
        return MSG_HANDLED;

    case MSG_DRAW:
        hexstrip_draw (h, widget_get_state (w, WST_FOCUSED));
        return MSG_HANDLED;

    case MSG_RESIZE:
        widget_default_callback (w, sender, msg, parm, data);
        hexstrip_layout (w->rect.cols, &h->bytes_per_line, &h->text_start);
        h->top = row_align (h, h->top);
        ensure_cursor_visible (h);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static off_t
byte_at_point (const WHexStrip *h, int x, int y)
{
    const WRect *r = &CONST_WIDGET (h)->rect;
    off_t row_off = h->top + (off_t) y * h->bytes_per_line;
    int i;

    if (x >= h->text_start)
        return row_off + MIN (x - h->text_start, h->bytes_per_line - 1);
    for (i = h->bytes_per_line - 1; i > 0; i--)
        if (x >= hexstrip_byte_col (r->cols, h->bytes_per_line, i))
            break;
    return row_off + i;
}

/* --------------------------------------------------------------------------------------------- */

static void
hexstrip_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WHexStrip *h = HEXSTRIP (w);

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        widget_select (w);
        h->in_text = event->x >= h->text_start;
        h->low_nibble = FALSE;
        hexstrip_set_cursor (h, byte_at_point (h, event->x, event->y));
        break;

    case MSG_MOUSE_SCROLL_UP:
        hexstrip_scroll (h, -1);
        break;

    case MSG_MOUSE_SCROLL_DOWN:
        hexstrip_scroll (h, 1);
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
hexstrip_layout (int cols, int *bytes_per_line, int *text_start)
{
    int bpl, ngroups;

    if (cols < 9 + 17)
        bpl = 4;
    else
        bpl = 4 * ((cols - 9) / (cols <= 80 ? 17 : 18));
    ngroups = bpl / 4;
    *bytes_per_line = bpl;
    *text_start = 8 + 13 * ngroups;
    if (cols == 80)
        *text_start += ngroups - 1;
    else if (cols > 80)
        *text_start += ngroups;
}

/* --------------------------------------------------------------------------------------------- */

int
hexstrip_byte_col (int cols, int bytes_per_line, int i)
{
    int col = 9 + i * 3;

    (void) bytes_per_line;
    if (cols >= 80)
        col += (i / 4) * 2;
    return col;
}

/* --------------------------------------------------------------------------------------------- */

WHexStrip *
hexstrip_new (int y, int x, int lines, int cols)
{
    WHexStrip *h;
    Widget *w;
    WRect r = { y, x, lines > 0 ? lines : 1, cols > 0 ? cols : 1 };

    h = g_new0 (WHexStrip, 1);
    w = WIDGET (h);
    widget_init (w, &r, hexstrip_callback, hexstrip_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_WANT_CURSOR;
    h->cursor_y = -1;
    h->colors.normal = h->colors.offset = h->colors.mark = h->colors.changed = -1;
    h->colors.cursor = h->colors.frame = h->colors.block = -1;
    hexstrip_layout (r.cols, &h->bytes_per_line, &h->text_start);
    return h;
}

/* --------------------------------------------------------------------------------------------- */

void
hexstrip_set_source (WHexStrip *h, const hexstrip_source_t *source)
{
    h->source = *source;
    h->top = 0;
    h->cursor = 0;
    h->low_nibble = FALSE;
    h->mark_len = 0;
    redraw (h);
}

/* --------------------------------------------------------------------------------------------- */

void
hexstrip_set_colors (WHexStrip *h, const hexstrip_colors_t *colors)
{
    h->colors = *colors;
    redraw (h);
}

/* --------------------------------------------------------------------------------------------- */

void
hexstrip_set_handlers (WHexStrip *h, void (*on_cursor) (WHexStrip *, void *),
                       gboolean (*on_edit) (WHexStrip *, off_t, unsigned char, void *), void *data)
{
    h->on_cursor = on_cursor;
    h->on_edit = on_edit;
    h->data = data;
}

/* --------------------------------------------------------------------------------------------- */

void
hexstrip_set_block (WHexStrip *h, off_t offset, off_t len)
{
    h->block_start = offset;
    h->block_len = len;
    redraw (h);
}

/* --------------------------------------------------------------------------------------------- */

void
hexstrip_set_mark (WHexStrip *h, off_t offset, off_t len)
{
    int lines = WIDGET (h)->rect.lines;
    off_t page = (off_t) lines * h->bytes_per_line;
    off_t size = source_size (h);

    h->mark_start = offset;
    h->mark_len = len;
    h->low_nibble = FALSE;
    if (size > 0 && offset >= size)
        offset = size - 1;
    if (offset < 0)
        offset = 0;
    h->cursor = offset;

    /* keep the range in view; when it moves out, center its start */
    if (offset < h->top || offset + MAX (len, 1) > h->top + page)
    {
        h->top = row_align (h, offset) - (off_t) (lines / 2) * h->bytes_per_line;
        clamp_top (h);
    }
    redraw (h);
}

/* --------------------------------------------------------------------------------------------- */

void
hexstrip_set_cursor (WHexStrip *h, off_t offset)
{
    off_t size = source_size (h);

    if (size > 0 && offset >= size)
        offset = size - 1;
    if (offset < 0)
        offset = 0;
    h->cursor = offset;
    h->low_nibble = FALSE;
    cursor_moved (h);
}

/* --------------------------------------------------------------------------------------------- */

off_t
hexstrip_get_cursor (const WHexStrip *h)
{
    return h->cursor;
}

/* --------------------------------------------------------------------------------------------- */

void
hexstrip_move (WHexStrip *h, off_t delta)
{
    hexstrip_set_cursor (h, h->cursor + delta);
}

/* --------------------------------------------------------------------------------------------- */

void
hexstrip_scroll (WHexStrip *h, int rows)
{
    h->top += (off_t) rows * h->bytes_per_line;
    clamp_top (h);
    redraw (h);
}

/* --------------------------------------------------------------------------------------------- */
