/*
   lib/widget - tests for WHexStrip

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

#define TEST_SUITE_NAME "lib/widget/hexstrip"

#include "tests/mctest.h"

#include <string.h>

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

static unsigned char mem[100];
static off_t last_edit_offset = -1;
static unsigned char last_edit_value;
static int cursor_calls;

static off_t
mem_size (void *ctx)
{
    (void) ctx;
    return sizeof (mem);
}

static gssize
mem_read (void *ctx, off_t offset, void *buf, gsize len)
{
    (void) ctx;
    if (offset >= (off_t) sizeof (mem))
        return 0;
    if (len > sizeof (mem) - offset)
        len = sizeof (mem) - offset;
    memcpy (buf, mem + offset, len);
    return (gssize) len;
}

static gboolean
on_edit (WHexStrip *h, off_t offset, unsigned char value, void *data)
{
    (void) h;
    (void) data;
    last_edit_offset = offset;
    last_edit_value = value;
    mem[offset] = value;
    return TRUE;
}

static void
on_cursor (WHexStrip *h, void *data)
{
    (void) h;
    (void) data;
    cursor_calls++;
}

static WHexStrip *
make_strip (int lines, int cols)
{
    WHexStrip *h;
    hexstrip_source_t src = { mem_size, mem_read, NULL, NULL };

    h = hexstrip_new (0, 0, lines, cols);
    hexstrip_set_source (h, &src);
    hexstrip_set_handlers (h, on_cursor, on_edit, NULL);
    return h;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_layout)
{
    int bpl, text;

    hexstrip_layout (80, &bpl, &text);
    ck_assert_int_eq (bpl, 16);
    ck_assert_int_eq (text, 63);
    ck_assert_int_eq (hexstrip_byte_col (80, bpl, 0), 9);
    ck_assert_int_eq (hexstrip_byte_col (80, bpl, 3), 18);
    ck_assert_int_eq (hexstrip_byte_col (80, bpl, 4), 23);
    ck_assert_int_eq (hexstrip_byte_col (80, bpl, 15), 60);

    hexstrip_layout (132, &bpl, &text);
    ck_assert_int_eq (bpl, 24);
    ck_assert_int_eq (text, 8 + 13 * 6 + 6);

    hexstrip_layout (40, &bpl, &text);
    ck_assert_int_eq (bpl, 4);
    ck_assert_int_eq (text, 21);
    ck_assert_int_eq (hexstrip_byte_col (40, bpl, 3), 18);

    hexstrip_layout (10, &bpl, &text);
    ck_assert_int_eq (bpl, 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cursor_and_scroll)
{
    WHexStrip *h = make_strip (4, 80);

    ck_assert_int_eq (h->bytes_per_line, 16);
    ck_assert_int_eq (h->top, 0);
    ck_assert_int_eq (hexstrip_get_cursor (h), 0);

    cursor_calls = 0;
    hexstrip_move (h, 40);
    ck_assert_int_eq (hexstrip_get_cursor (h), 40);
    ck_assert_int_eq (h->top, 0); /* rows 0..63 visible */
    ck_assert_int_eq (cursor_calls, 1);

    hexstrip_move (h, 30);
    ck_assert_int_eq (hexstrip_get_cursor (h), 70);
    ck_assert_int_eq (h->top, 16); /* 64 is on the last row */

    hexstrip_move (h, 1000);
    ck_assert_int_eq (hexstrip_get_cursor (h), 99);
    ck_assert_int_eq (h->top, 48);

    hexstrip_move (h, -1000);
    ck_assert_int_eq (hexstrip_get_cursor (h), 0);
    ck_assert_int_eq (h->top, 0);

    hexstrip_scroll (h, 100);
    ck_assert_int_eq (h->top, 96); /* last row */
    hexstrip_scroll (h, -100);
    ck_assert_int_eq (h->top, 0);

    widget_destroy (WIDGET (h));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_mark)
{
    WHexStrip *h = make_strip (4, 80);

    hexstrip_set_mark (h, 70, 4);
    ck_assert_int_eq (h->mark_start, 70);
    ck_assert_int_eq (h->mark_len, 4);
    ck_assert_int_eq (hexstrip_get_cursor (h), 70);
    /* a block does not move the cursor */
    hexstrip_set_block (h, 10, 5);
    ck_assert_int_eq (h->block_start, 10);
    ck_assert_int_eq (h->block_len, 5);
    ck_assert_int_eq (hexstrip_get_cursor (h), 70);
    hexstrip_set_block (h, 0, 0);
    ck_assert_int_eq (h->block_len, 0);
    ck_assert_int_eq (h->top, 64 - 2 * 16);

    /* a range already on screen does not scroll */
    hexstrip_set_mark (h, 40, 2);
    ck_assert_int_eq (h->top, 32);

    hexstrip_set_mark (h, 0, 1);
    ck_assert_int_eq (h->top, 0);

    hexstrip_set_mark (h, 500, 1);
    ck_assert_int_eq (hexstrip_get_cursor (h), 99);

    widget_destroy (WIDGET (h));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keys_and_edit)
{
    WHexStrip *h = make_strip (2, 80);
    Widget *w = WIDGET (h);

    memset (mem, 0, sizeof (mem));
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, KEY_DOWN, NULL), MSG_HANDLED);
    ck_assert_int_eq (hexstrip_get_cursor (h), 16);
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, KEY_RIGHT, NULL), MSG_HANDLED);
    ck_assert_int_eq (hexstrip_get_cursor (h), 17);
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, KEY_NPAGE, NULL), MSG_HANDLED);
    ck_assert_int_eq (hexstrip_get_cursor (h), 49);
    ck_assert_int_eq (h->top, 32);
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, KEY_HOME, NULL), MSG_HANDLED);
    ck_assert_int_eq (hexstrip_get_cursor (h), 0);
    ck_assert_int_eq (send_message (w, NULL, MSG_ACTION, CK_Bottom, NULL), MSG_HANDLED);
    ck_assert_int_eq (hexstrip_get_cursor (h), 99);
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, KEY_HOME, NULL), MSG_HANDLED);

    /* hex nibbles */
    last_edit_offset = -1;
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, 'a', NULL), MSG_HANDLED);
    ck_assert_int_eq (last_edit_offset, 0);
    ck_assert_int_eq (last_edit_value, 0xA0);
    ck_assert (h->low_nibble);
    ck_assert_int_eq (hexstrip_get_cursor (h), 0);
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, '5', NULL), MSG_HANDLED);
    ck_assert_int_eq (last_edit_value, 0xA5);
    ck_assert (!h->low_nibble);
    ck_assert_int_eq (hexstrip_get_cursor (h), 1);
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, 'z', NULL), MSG_NOT_HANDLED);

    /* text column */
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, '\t', NULL), MSG_HANDLED);
    ck_assert (h->in_text);
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, 'z', NULL), MSG_HANDLED);
    ck_assert_int_eq (last_edit_offset, 1);
    ck_assert_int_eq (last_edit_value, 'z');
    ck_assert_int_eq (hexstrip_get_cursor (h), 2);

    /* no handler, no edit */
    hexstrip_set_handlers (h, NULL, NULL, NULL);
    last_edit_offset = -1;
    ck_assert_int_eq (send_message (w, NULL, MSG_KEY, 'z', NULL), MSG_NOT_HANDLED);
    ck_assert_int_eq (last_edit_offset, -1);

    widget_destroy (w);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_layout);
    tcase_add_test (tc_core, test_cursor_and_scroll);
    tcase_add_test (tc_core, test_mark);
    tcase_add_test (tc_core, test_keys_and_edit);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
