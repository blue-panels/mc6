/*
   lib/tty - color pair numbering testing

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

#define TEST_SUITE_NAME "/lib/tty"

#include "tests/mctest.h"

#include <stdio.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/tty/color.h"

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    setenv ("TERM", "xterm", 1);
    tty_init_colors (TRUE, FALSE);
}

static void
teardown (void)
{
    tty_colors_done ();
}

/* --------------------------------------------------------------------------------------------- */

static int
test_pair (int fg, int bg, gboolean is_temp)
{
    char fg_name[16], bg_name[16];
    tty_color_pair_t color = { .fg = fg_name, .bg = bg_name, .attrs = NULL, .pair_index = 0 };

    // colors 0-15 go by their names, "red" and the like
    g_snprintf (fg_name, sizeof (fg_name), "color%d", fg + 16);
    g_snprintf (bg_name, sizeof (bg_name), "color%d", bg + 16);

    return tty_try_alloc_color_pair (&color, is_temp);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_numbers_in_order_and_shared)
{
    ck_assert_int_eq (test_pair (1, 2, FALSE), 0);
    ck_assert_int_eq (test_pair (3, 4, FALSE), 1);
    ck_assert_int_eq (test_pair (5, 6, TRUE), 2);

    // the same colors are the same pair
    ck_assert_int_eq (test_pair (3, 4, FALSE), 1);
    ck_assert_int_eq (test_pair (5, 6, TRUE), 2);

    ck_assert_str_eq (tty_color_pair_foreground (1), "color19");
    ck_assert_str_eq (tty_color_pair_background (1), "color20");
    ck_assert_ptr_null (tty_color_pair_foreground (3));
    ck_assert_ptr_null (tty_color_pair_foreground (-1));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A released number is the first one given out again. */
START_TEST (test_released_number_is_reused)
{
    ck_assert_int_eq (test_pair (1, 2, TRUE), 0);
    ck_assert_int_eq (test_pair (3, 4, TRUE), 1);
    ck_assert_int_eq (test_pair (5, 6, TRUE), 2);
    ck_assert_int_eq (test_pair (7, 8, TRUE), 3);

    tty_color_release_temp (2);
    tty_color_release_temp (1);
    ck_assert_ptr_null (tty_color_pair_foreground (1));
    ck_assert_ptr_null (tty_color_pair_foreground (2));
    ck_assert_str_eq (tty_color_pair_foreground (3), "color23");

    ck_assert_int_eq (test_pair (9, 10, TRUE), 1);
    ck_assert_int_eq (test_pair (11, 12, TRUE), 2);
    ck_assert_int_eq (test_pair (13, 14, TRUE), 4);

    // the old colors are gone with the old numbers
    ck_assert_int_eq (test_pair (3, 4, TRUE), 5);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A pair taken twice goes away with its last owner only. */
START_TEST (test_shared_temp_pair_needs_every_release)
{
    ck_assert_int_eq (test_pair (1, 2, TRUE), 0);
    ck_assert_int_eq (test_pair (1, 2, TRUE), 0);

    tty_color_release_temp (0);
    ck_assert_str_eq (tty_color_pair_foreground (0), "color17");

    tty_color_release_temp (0);
    ck_assert_ptr_null (tty_color_pair_foreground (0));

    // a permanent pair is not released
    ck_assert_int_eq (test_pair (3, 4, FALSE), 0);
    tty_color_release_temp (0);
    ck_assert_str_eq (tty_color_pair_foreground (0), "color19");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_free_temp_keeps_permanent)
{
    ck_assert_int_eq (test_pair (1, 2, TRUE), 0);
    ck_assert_int_eq (test_pair (3, 4, FALSE), 1);
    ck_assert_int_eq (test_pair (5, 6, TRUE), 2);

    tty_color_free_temp ();
    ck_assert_ptr_null (tty_color_pair_foreground (0));
    ck_assert_str_eq (tty_color_pair_foreground (1), "color19");
    ck_assert_ptr_null (tty_color_pair_foreground (2));

    ck_assert_int_eq (test_pair (7, 8, TRUE), 0);
    ck_assert_int_eq (test_pair (9, 10, TRUE), 2);
    ck_assert_int_eq (test_pair (11, 12, TRUE), 3);

    tty_color_free_all ();
    ck_assert_ptr_null (tty_color_pair_foreground (1));
    ck_assert_int_eq (test_pair (3, 4, FALSE), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_many_pairs)
{
    int fg, bg, n = 0;

    for (fg = 0; fg < 240; fg++)
        for (bg = 0; bg < 16; bg++)
            ck_assert_int_eq (test_pair (fg, bg, TRUE), n++);

    for (fg = 0; fg < 240; fg += 17)
    {
        char name[16];

        g_snprintf (name, sizeof (name), "color%d", fg + 16);
        ck_assert_str_eq (tty_color_pair_foreground (fg * 16 + 5), name);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_numbers_in_order_and_shared);
    tcase_add_test (tc_core, test_released_number_is_reused);
    tcase_add_test (tc_core, test_shared_temp_pair_needs_every_release);
    tcase_add_test (tc_core, test_free_temp_keeps_permanent);
    tcase_add_test (tc_core, test_many_pairs);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
