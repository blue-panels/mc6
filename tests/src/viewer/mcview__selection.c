/*
   src/viewer - unit tests for text selection

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

#define TEST_SUITE_NAME "/src/viewer/selection"

#include "tests/mctest.h"

#include "lib/strutil.h"

#include "src/setup.h"  // option_tab_spacing
#include "src/viewer/internal.h"

/*** fixtures ************************************************************************************/

static WView view;
static int saved_tab_spacing;

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    memset (&view, 0, sizeof (view));
    view.converter = str_cnv_from_term;
    view.force_max = -1;
    view.utf8 = TRUE;
    view.data_area.lines = 3;
    view.data_area.cols = 12;
    mcview_selection_init (&view);

    saved_tab_spacing = option_tab_spacing;
    option_tab_spacing = 8;
}

/* @After */
static void
teardown (void)
{
    option_tab_spacing = saved_tab_spacing;
    mcview_selection_done (&view);
    mcview_close_datasource (&view);
    if (view.filter_offsets != NULL)
        g_array_free (view.filter_offsets, TRUE);
    view.filter_offsets = NULL;
    view.filter_active = FALSE;
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

static void
load (const char *text)
{
    mcview_close_datasource (&view);
    mcview_set_datasource_string (&view, text);
    mcview_selection_clear (&view);
    mcview_selection_render_begin (&view);
}

/* --------------------------------------------------------------------------------------------- */

static void
record (int row, int col, int width, off_t from, off_t to, off_t column, int ch)
{
    mcview_state_machine_t before;

    mcview_state_machine_init (&before, from);
    before.unwrapped_column = column;
    mcview_selection_record (&view, row, col, width, &before, to, ch);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mouse_event (mouse_msg_t msg, int row, int col, int count, int buttons)
{
    mouse_event_t event;

    memset (&event, 0, sizeof (event));
    event.y = row;
    event.x = col;
    event.buttons = buttons;
    event.count = count;
    return mcview_selection_mouse (&view, msg, &event);
}

/* --------------------------------------------------------------------------------------------- */

static void
mouse (mouse_msg_t msg, int row, int col, int count)
{
    ck_assert (mouse_event (msg, row, col, count, GPM_B_LEFT));
}

/* --------------------------------------------------------------------------------------------- */

static void
record_line (int row, const char *line, off_t from, off_t column)
{
    int col;

    for (col = 0; line[col] != '\0'; col++)
        record (row, col, 1, from + col, from + col + 1, column + col, line[col]);
}

/*** tests ***************************************************************************************/

START_TEST (test_mouse_selection_across_lines)
{
    char *text;

    load ("one\ntwo\n");
    record (0, 0, 1, 0, 1, 0, 'o');
    record (0, 1, 1, 1, 2, 1, 'n');
    record (0, 2, 1, 2, 3, 2, 'e');
    record (0, 3, 1, 3, 4, 3, '\n');
    record (1, 0, 1, 4, 5, 0, 't');
    record (1, 1, 1, 5, 6, 1, 'w');
    record (1, 2, 1, 6, 7, 2, 'o');
    record (1, 3, 1, 7, 8, 3, '\n');

    mouse (MSG_MOUSE_DOWN, 0, 1, GPM_SINGLE);
    mouse (MSG_MOUSE_DRAG, 1, 1, GPM_SINGLE);

    text = mcview_selection_text (&view);
    ck_assert_ptr_nonnull (text);
    ck_assert_str_eq (text, "ne\ntw");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_double_click_word_and_triple_click_line)
{
    char *text;

    load ("one two\n");
    record (0, 0, 1, 0, 1, 0, 'o');
    record (0, 1, 1, 1, 2, 1, 'n');
    record (0, 2, 1, 2, 3, 2, 'e');
    record (0, 3, 1, 3, 4, 3, ' ');
    record (0, 4, 1, 4, 5, 4, 't');
    record (0, 5, 1, 5, 6, 5, 'w');
    record (0, 6, 1, 6, 7, 6, 'o');
    record (0, 7, 1, 7, 8, 7, '\n');

    mouse (MSG_MOUSE_DOWN, 0, 1, GPM_DOUBLE);
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "one");
    g_free (text);

    mouse (MSG_MOUSE_DOWN, 0, 4, GPM_TRIPLE);
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "one two");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyboard_extends_from_mouse_cursor)
{
    char *text;

    load ("one\n");
    record (0, 0, 1, 0, 1, 0, 'o');
    record (0, 1, 1, 1, 2, 1, 'n');
    record (0, 2, 1, 2, 3, 2, 'e');
    record (0, 3, 1, 3, 4, 3, '\n');

    mouse (MSG_MOUSE_DOWN, 0, 0, GPM_SINGLE);
    ck_assert (mcview_selection_command (&view, CK_MarkRight));

    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "on");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_tab_is_copied_as_displayed_spaces)
{
    char *text;

    load ("ab\tc");
    record (0, 0, 1, 0, 1, 0, 'a');
    record (0, 1, 1, 1, 2, 1, 'b');
    record (0, 2, 6, 2, 3, 2, '\t');
    record (0, 8, 1, 3, 4, 8, 'c');

    mouse (MSG_MOUSE_DOWN, 0, 3, GPM_SINGLE);
    mouse (MSG_MOUSE_DRAG, 0, 8, GPM_SINGLE);

    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "      c");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_release_ends_drag_and_click_is_consumed_once)
{
    char *text;

    load ("one\ntwo\n");
    record_line (0, "one\n", 0, 0);
    record_line (1, "two\n", 4, 0);

    mouse (MSG_MOUSE_DOWN, 0, 1, GPM_SINGLE);
    mouse (MSG_MOUSE_DRAG, 0, 2, GPM_SINGLE);
    ck_assert (!mouse_event (MSG_MOUSE_UP, 0, 2, GPM_SINGLE, GPM_B_LEFT));

    /* No button is held: a later motion does not extend the selection. */
    ck_assert (!mouse_event (MSG_MOUSE_DRAG, 1, 2, GPM_SINGLE, GPM_B_RIGHT));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "ne");
    g_free (text);

    /* The click after a consumed press is consumed; the next one is not. */
    mouse (MSG_MOUSE_DOWN, 1, 0, GPM_SINGLE);
    ck_assert (!mouse_event (MSG_MOUSE_UP, 1, 0, GPM_SINGLE, GPM_B_LEFT));
    ck_assert (mouse_event (MSG_MOUSE_CLICK, 1, 0, GPM_SINGLE, GPM_B_LEFT));
    ck_assert (!mouse_event (MSG_MOUSE_CLICK, 1, 0, GPM_SINGLE, GPM_B_LEFT));
    ck_assert (!mcview_selection_active (&view));

    /* The right button is left to the scrolling code. */
    ck_assert (!mouse_event (MSG_MOUSE_DOWN, 1, 0, GPM_SINGLE, GPM_B_RIGHT));
    ck_assert (!mouse_event (MSG_MOUSE_CLICK, 1, 0, GPM_SINGLE, GPM_B_RIGHT));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyboard_extends_from_the_click_after_release)
{
    char *text;

    load ("one\n");
    record_line (0, "one\n", 0, 0);

    mouse (MSG_MOUSE_DOWN, 0, 1, GPM_SINGLE);
    ck_assert (!mouse_event (MSG_MOUSE_UP, 0, 1, GPM_SINGLE, GPM_B_LEFT));
    ck_assert (mouse_event (MSG_MOUSE_CLICK, 0, 1, GPM_SINGLE, GPM_B_LEFT));

    ck_assert (mcview_selection_command (&view, CK_MarkRight));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "ne");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_unused_commands_are_not_consumed)
{
    load ("");

    /* Nothing on screen: the keys stay free for the command line under a quick view. */
    ck_assert (!mcview_selection_command (&view, CK_MarkRight));
    ck_assert (!mcview_selection_command (&view, CK_MarkToEnd));
    ck_assert (!mcview_selection_command (&view, CK_Unmark));
    ck_assert (!mcview_selection_command (&view, CK_MarkAll));

    load ("one\n");
    record_line (0, "one\n", 0, 0);
    mouse (MSG_MOUSE_DOWN, 0, 0, GPM_SINGLE);
    /* Already at the start of the row: nothing to extend, nothing selected yet. */
    ck_assert (!mcview_selection_command (&view, CK_MarkToHome));
    ck_assert (!mcview_selection_active (&view));

    ck_assert (mcview_selection_command (&view, CK_MarkToEnd));
    ck_assert (mcview_selection_active (&view));
    ck_assert (mcview_selection_command (&view, CK_Unmark));
    ck_assert (!mcview_selection_active (&view));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_line_scrolled_out_to_the_left_keeps_its_row)
{
    char *text;

    /* Unwrap mode scrolled right by 4 columns: line 2 is shorter than the offset. */
    load ("abcdefgh\nxy\nijklmnop\n");
    view.dpy_text_column = 4;
    record_line (0, "efgh\n", 4, 4);
    record (1, -2, 1, 11, 12, 2, '\n');
    record_line (2, "mnop\n", 16, 4);

    mouse (MSG_MOUSE_DOWN, 0, 0, GPM_SINGLE);
    mouse (MSG_MOUSE_DRAG, 1, 5, GPM_SINGLE);
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "efgh\nxy\n");
    g_free (text);

    mouse (MSG_MOUSE_DOWN, 1, 3, GPM_SINGLE);
    ck_assert (mcview_selection_command (&view, CK_MarkDown));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "\nijklm");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_filter_mode_copies_visible_lines_only)
{
    char *text;
    off_t offset;

    load ("match1\nskip\nskip\nmatch2\n");
    view.filter_active = TRUE;
    view.filter_offsets = g_array_new (FALSE, FALSE, sizeof (off_t));
    offset = 0;
    g_array_append_val (view.filter_offsets, offset);
    offset = 17;
    g_array_append_val (view.filter_offsets, offset);

    record_line (0, "match1\n", 0, 0);
    record_line (1, "match2\n", 17, 0);

    mouse (MSG_MOUSE_DOWN, 0, 0, GPM_SINGLE);
    mouse (MSG_MOUSE_DRAG, 1, 5, GPM_SINGLE);
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "match1\nmatch2");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cursor_moves_drop_the_selection)
{
    char *text;

    load ("abcdefghijk\nxyzw\n");
    record_line (0, "abcdefghijk\n", 0, 0);
    record_line (1, "xyzw\n", 12, 0);

    mouse (MSG_MOUSE_DOWN, 0, 0, GPM_SINGLE);
    ck_assert (mcview_selection_command (&view, CK_MarkRight));
    ck_assert (mcview_selection_active (&view));

    /* Right drops the selection and steps on from the cursor ('b' -> 'c'). */
    ck_assert (mcview_selection_command (&view, CK_Right));
    ck_assert (!mcview_selection_active (&view));
    ck_assert (mcview_selection_command (&view, CK_MarkRight));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "cd");
    g_free (text);

    /* Down keeps the column on the next row, Up goes back; both drop the selection. */
    ck_assert (mcview_selection_command (&view, CK_Down));
    ck_assert (!mcview_selection_active (&view));
    ck_assert (mcview_selection_command (&view, CK_MarkRight));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "w\n");
    g_free (text);
    ck_assert (mcview_selection_command (&view, CK_Up));
    ck_assert (mcview_selection_command (&view, CK_MarkRight));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "ef");
    g_free (text);

    /* Ctrl-Left steps eight characters back, stopping at the start of the screen. */
    ck_assert (mcview_selection_command (&view, CK_LeftQuick));
    ck_assert (mcview_selection_command (&view, CK_MarkRight));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "ab");
    g_free (text);

    /* Ctrl-Right from 'b' lands on 'j'; Right past the newline goes on to 'x'. */
    ck_assert (mcview_selection_command (&view, CK_RightQuick));
    ck_assert (mcview_selection_command (&view, CK_Right));
    ck_assert (mcview_selection_command (&view, CK_Right));
    ck_assert (mcview_selection_command (&view, CK_Right));
    ck_assert (mcview_selection_command (&view, CK_MarkRight));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "xy");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cursor_keeps_its_screen_cell_after_a_scroll)
{
    char *text;

    load ("one\ntwo\nthree\nfour\n");
    record_line (0, "one\n", 0, 0);
    record_line (1, "two\n", 4, 0);
    mouse (MSG_MOUSE_DOWN, 1, 1, GPM_SINGLE);

    /* The view scrolled by two lines and was redrawn: the cursor is on row 1 col 1 again. */
    mcview_selection_render_begin (&view);
    record_line (0, "three\n", 8, 0);
    record_line (1, "four\n", 14, 0);
    mcview_selection_render_end (&view);

    ck_assert (mcview_selection_command (&view, CK_MarkRight));
    text = mcview_selection_text (&view);
    ck_assert_str_eq (text, "ou");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_copy_text_is_ansi_and_nroff_processed)
{
    char *text;

    view.mode_flags.ansi = TRUE;
    load ("\033[31mA\033[0m\tC\r\n");

    ck_assert (mcview_selection_command (&view, CK_MarkAll));
    text = mcview_selection_text (&view);
    ck_assert_ptr_nonnull (text);
    ck_assert_str_eq (text, "A       C\n");
    g_free (text);

    view.mode_flags.ansi = FALSE;
    view.mode_flags.nroff = TRUE;
    load ("A _\bB\tC\r\n");
    ck_assert (mcview_selection_command (&view, CK_MarkAll));
    text = mcview_selection_text (&view);
    ck_assert_ptr_nonnull (text);
    ck_assert_str_eq (text, "A B     C\n");
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_mouse_selection_across_lines);
    tcase_add_test (tc_core, test_double_click_word_and_triple_click_line);
    tcase_add_test (tc_core, test_keyboard_extends_from_mouse_cursor);
    tcase_add_test (tc_core, test_tab_is_copied_as_displayed_spaces);
    tcase_add_test (tc_core, test_release_ends_drag_and_click_is_consumed_once);
    tcase_add_test (tc_core, test_keyboard_extends_from_the_click_after_release);
    tcase_add_test (tc_core, test_unused_commands_are_not_consumed);
    tcase_add_test (tc_core, test_line_scrolled_out_to_the_left_keeps_its_row);
    tcase_add_test (tc_core, test_filter_mode_copies_visible_lines_only);
    tcase_add_test (tc_core, test_cursor_moves_drop_the_selection);
    tcase_add_test (tc_core, test_cursor_keeps_its_screen_cell_after_a_scroll);
    tcase_add_test (tc_core, test_copy_text_is_ansi_and_nroff_processed);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
