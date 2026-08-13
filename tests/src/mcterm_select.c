/*
   tests/src/mcterm_select.c -- unit tests for the mcterm selection

   Copyright (C) 2026
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/src/mcterm_select"

#include "tests/mctest.h"

#include "src/mcterm/mcterm_select.h"

#define TERM_ROWS 4
#define TERM_COLS 20

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static void
feed (mcview_vterm_t *vt, const char *data)
{
    const char *p;

    for (p = data; *p != '\0'; p++)
    {
        vterm_event_t ev = mcview_vterm_feed (vt, (unsigned char) *p);

        mcview_vterm_apply_event (vt, &ev);
    }
}

/* --------------------------------------------------------------------------------------------- */

static mcview_vterm_t *
term_new (void)
{
    mcview_vterm_t *vt;

    vt = mcview_vterm_new ();
    mcview_vterm_set_size (vt, TERM_ROWS, TERM_COLS);
    mcview_vterm_set_keep_history (vt, TRUE);

    return vt;
}

/* --------------------------------------------------------------------------------------------- */

static void
mark (mcterm_sel_t *sel, gint64 row1, int col1, gint64 row2, int col2)
{
    mcterm_sel_start (sel, row1, col1);
    mcterm_sel_extend (sel, row2, col2);
}

/* --------------------------------------------------------------------------------------------- */
/*** tests ***************************************************************************************/
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_one_row_without_the_padding)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    char *text;

    vt = term_new ();
    feed (vt, "hello");

    // The whole first row, which the terminal padded with blanks.
    mcterm_sel_clear (&sel);
    mark (&sel, 0, 0, 0, TERM_COLS - 1);
    text = mcterm_sel_text (&sel, vt, TERM_COLS);

    ck_assert_ptr_nonnull (text);
    ck_assert_str_eq (text, "hello");

    g_free (text);
    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_part_of_a_row_keeps_its_blanks)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    char *text;

    vt = term_new ();
    feed (vt, "ab cd");

    mcterm_sel_clear (&sel);
    mark (&sel, 0, 1, 0, 3);
    text = mcterm_sel_text (&sel, vt, TERM_COLS);

    // The point is on a cell, and that cell is part of the region.
    ck_assert_str_eq (text, "b c");

    g_free (text);
    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rows_are_joined_by_a_newline)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    char *text;

    vt = term_new ();
    feed (vt, "one\r\ntwo\r\nthree");

    mcterm_sel_clear (&sel);
    mark (&sel, 0, 0, 2, TERM_COLS - 1);
    text = mcterm_sel_text (&sel, vt, TERM_COLS);

    ck_assert_str_eq (text, "one\ntwo\nthree");

    g_free (text);
    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_ends_are_partial_rows)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    char *text;

    vt = term_new ();
    feed (vt, "one\r\ntwo\r\nthree");

    // From the middle of the first row to the middle of the last one.
    mcterm_sel_clear (&sel);
    mark (&sel, 0, 1, 2, 2);
    text = mcterm_sel_text (&sel, vt, TERM_COLS);

    ck_assert_str_eq (text, "ne\ntwo\nthr");

    g_free (text);
    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_region_is_read_either_way_round)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    char *text;

    vt = term_new ();
    feed (vt, "one\r\ntwo");

    // Marked from the end backwards, which is what dragging upwards does.
    mcterm_sel_clear (&sel);
    mark (&sel, 1, 1, 0, 1);
    text = mcterm_sel_text (&sel, vt, TERM_COLS);

    ck_assert_str_eq (text, "ne\ntw");

    g_free (text);
    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* What the terminal has scrolled off is still named by the same number. */
START_TEST (test_a_row_keeps_its_number_after_scrolling)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    char *text;

    vt = term_new ();
    // Six rows through a screen of four: the first two leave the top.
    feed (vt, "r0\r\nr1\r\nr2\r\nr3\r\nr4\r\nr5");

    ck_assert_int_eq ((int) mcview_vterm_scrolled_rows (vt), 2);
    ck_assert_int_eq (mcview_vterm_history_len (vt), 2);

    mcterm_sel_clear (&sel);
    mark (&sel, 0, 0, 1, TERM_COLS - 1);
    text = mcterm_sel_text (&sel, vt, TERM_COLS);

    // Rows 0 and 1 are in the history by now, and read the same as before.
    ck_assert_str_eq (text, "r0\nr1");
    g_free (text);

    // A row of the live screen, by the number it had when it was printed.
    mcterm_sel_clear (&sel);
    mark (&sel, 5, 0, 5, TERM_COLS - 1);
    text = mcterm_sel_text (&sel, vt, TERM_COLS);
    ck_assert_str_eq (text, "r5");
    g_free (text);

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Output that arrives after the mark was set must not move it. */
START_TEST (test_new_output_does_not_move_the_region)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    char *before, *after;

    vt = term_new ();
    feed (vt, "keep me\r\nr1\r\nr2");

    mcterm_sel_clear (&sel);
    mark (&sel, 0, 0, 0, TERM_COLS - 1);
    before = mcterm_sel_text (&sel, vt, TERM_COLS);

    feed (vt, "\r\nr3\r\nr4\r\nr5");
    after = mcterm_sel_text (&sel, vt, TERM_COLS);

    ck_assert_str_eq (before, "keep me");
    ck_assert_str_eq (after, "keep me");

    g_free (before);
    g_free (after);
    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_row_that_fell_out_of_the_history_reads_as_blank)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    char *text;

    vt = term_new ();
    mcview_vterm_set_keep_history (vt, FALSE);
    feed (vt, "gone\r\nr1\r\nr2\r\nr3\r\nr4");

    ck_assert_int_eq ((int) mcview_vterm_scrolled_rows (vt), 1);

    mcterm_sel_clear (&sel);
    mark (&sel, 0, 0, 0, TERM_COLS - 1);
    text = mcterm_sel_text (&sel, vt, TERM_COLS);

    ck_assert_ptr_nonnull (text);
    ck_assert_str_eq (text, "");

    g_free (text);
    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_nothing_is_marked)
{
    mcview_vterm_t *vt;
    mcterm_sel_t sel;
    int from, to;

    vt = term_new ();
    feed (vt, "text");

    mcterm_sel_clear (&sel);
    ck_assert_ptr_null (mcterm_sel_text (&sel, vt, TERM_COLS));
    ck_assert (!mcterm_sel_row_span (&sel, 0, TERM_COLS, &from, &to));

    // A click alone marks nothing: the region starts when the drag does.
    mcterm_sel_start (&sel, 0, 0);
    ck_assert (!mcterm_sel_row_span (&sel, 0, TERM_COLS, &from, &to));
    ck_assert_ptr_null (mcterm_sel_text (&sel, vt, TERM_COLS));

    mcview_vterm_free (vt);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_span_of_a_row)
{
    mcterm_sel_t sel;
    int from, to;

    mcterm_sel_clear (&sel);
    mark (&sel, 1, 3, 3, 5);

    ck_assert (!mcterm_sel_row_span (&sel, 0, TERM_COLS, &from, &to));

    // The first row starts at the anchor and runs to the width.
    ck_assert (mcterm_sel_row_span (&sel, 1, TERM_COLS, &from, &to));
    ck_assert_int_eq (from, 3);
    ck_assert_int_eq (to, TERM_COLS);

    ck_assert (mcterm_sel_row_span (&sel, 2, TERM_COLS, &from, &to));
    ck_assert_int_eq (from, 0);
    ck_assert_int_eq (to, TERM_COLS);

    // The last one ends past the cell the point is on.
    ck_assert (mcterm_sel_row_span (&sel, 3, TERM_COLS, &from, &to));
    ck_assert_int_eq (from, 0);
    ck_assert_int_eq (to, 6);

    ck_assert (!mcterm_sel_row_span (&sel, 4, TERM_COLS, &from, &to));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */
/*** main ****************************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_one_row_without_the_padding);
    tcase_add_test (tc_core, test_part_of_a_row_keeps_its_blanks);
    tcase_add_test (tc_core, test_rows_are_joined_by_a_newline);
    tcase_add_test (tc_core, test_the_ends_are_partial_rows);
    tcase_add_test (tc_core, test_the_region_is_read_either_way_round);
    tcase_add_test (tc_core, test_a_row_keeps_its_number_after_scrolling);
    tcase_add_test (tc_core, test_new_output_does_not_move_the_region);
    tcase_add_test (tc_core, test_a_row_that_fell_out_of_the_history_reads_as_blank);
    tcase_add_test (tc_core, test_nothing_is_marked);
    tcase_add_test (tc_core, test_the_span_of_a_row);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
