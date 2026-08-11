/*
   lib/tty - key sequence trie testing

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

#define TEST_SUITE_NAME "/lib/tty"

#include "tests/mctest.h"

#include <stdlib.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    setenv ("TERM", "xterm", 1);
    mc_global.tty.disable_x11 = TRUE;
    init_key ();
}

static void
teardown (void)
{
    done_key ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_whole_sequence_is_a_key)
{
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[1;5C", 6), KEY_M_CTRL | KEY_RIGHT);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[1;2C", 6), KEY_M_SHIFT | KEY_RIGHT);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[1;3C", 6), KEY_M_ALT | KEY_RIGHT);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[11~", 5), KEY_F (1));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The head of a sequence names no key. Reporting one there makes mc act on a
   key the user did not press as soon as the rest of the bytes are late. */
START_TEST (test_head_of_sequence_is_no_key)
{
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[", 2), 0);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[1", 3), 0);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[1;", 4), 0);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[1;5", 5), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* ESC is a key of its own and the head of every other sequence at once. */
START_TEST (test_esc_is_a_key_and_a_head)
{
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR, 1), ESC_CHAR);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR ESC_STR, 2), ESC_CHAR);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The markers a terminal puts around a paste are whole sequences like any
   other, and only their last byte names them. */
START_TEST (test_bracketed_paste_markers)
{
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[200~", 6), MCKEY_BRACKETED_PASTING_START);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[201~", 6), MCKEY_BRACKETED_PASTING_END);

    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[2", 3), 0);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[20", 4), 0);
    ck_assert_int_eq (tty_match_seq_to_keycode (ESC_STR "[200", 5), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_whole_sequence_is_a_key);
    tcase_add_test (tc_core, test_head_of_sequence_is_no_key);
    tcase_add_test (tc_core, test_esc_is_a_key_and_a_head);
    tcase_add_test (tc_core, test_bracketed_paste_markers);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
