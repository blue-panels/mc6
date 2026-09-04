/*
   src/editor - tests for the control characters display mode

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

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include "lib/charsets.h"
#include "src/vfs/local/local.c"
#include "src/selcodepage.h"

#include "src/editor/editwidget.h"

static WDialog owner;
static WEdit *test_edit;

// a\r b\f c\t d DEL \n
static const char test_text[] = "a\rb\x0c"
                                "c\td\x7f\n";

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    WRect r;

    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    edit_options.filesize_threshold = (char *) "64M";
    edit_options.show_control_chars = TRUE;

    rect_init (&r, 0, 0, 24, 80);
    test_edit = edit_init (NULL, &r, NULL);
    memset (&owner, 0, sizeof (owner));
    group_add_widget (&owner.group, WIDGET (test_edit));

    mc_global.source_codepage = 0;
    mc_global.display_codepage = 0;
    cp_source = "ASCII";
    cp_display = "ASCII";

    do_set_codepage (0);
    edit_set_codeset (test_edit);

    for (const char *t = test_text; *t != '\0'; t++)
    {
        edit_buffer_insert (&test_edit->buffer, *t);
        if (*t == '\n')
            test_edit->buffer.lines++;
    }
    test_edit->buffer.one_byte_per_column = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    edit_clean (test_edit);
    group_remove_widget (test_edit);
    g_free (test_edit);

    free_codepages_list ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_width_shown)
{
    ck_assert_int_eq (edit_control_char_width (), 2);

    // "a" "^M" "b" "^L" "c" -> 7 columns, tab pads to 8
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 0, 2), 3);
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 0, 5), 7);
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 0, 6), 8);
    // "d" "^?"
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 0, 8), 11);

    // column 3 is "b", column 4 is inside "^L"
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 3, 0), 2);
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 4, 0), 3);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_width_hidden)
{
    edit_options.show_control_chars = FALSE;

    ck_assert_int_eq (edit_control_char_width (), 1);

    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 0, 2), 2);
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 0, 5), 5);
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 0, 6), 8);
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 0, 8), 10);

    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 3, 0), 3);
    ck_assert_int_eq (edit_move_forward3 (test_edit, 0, 4, 0), 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_layout_reset_after_toggle)
{
    // cursor after "^M", before "b"
    edit_cursor_move (test_edit, 2 - test_edit->buffer.curs1);
    ck_assert_int_eq (edit_get_col (test_edit), 3);

    edit_options.show_control_chars = FALSE;
    edit_layout_reset (test_edit);
    ck_assert_int_eq (edit_get_col (test_edit), 2);
    ck_assert_int_eq (test_edit->curs_col, 2);

    edit_options.show_control_chars = TRUE;
    edit_layout_reset (test_edit);
    ck_assert_int_eq (edit_get_col (test_edit), 3);
    ck_assert_int_eq (test_edit->curs_col, 3);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_width_shown);
    tcase_add_test (tc_core, test_width_hidden);
    tcase_add_test (tc_core, test_layout_reset_after_toggle);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
