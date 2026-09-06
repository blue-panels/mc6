/*
   src/panel-plugins/skineditor - color value split, join and classification

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

#define TEST_SUITE_NAME "/src/panel-plugins/skineditor"

#include "tests/mctest.h"

#include <string.h>

#include "src/panel-plugins/skineditor/skinedit_model.h"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_join)
{
    char *p[SKINEDIT_PARTS] = { NULL, NULL, NULL };
    char *s;

    ck_assert_ptr_null (skinedit_color_join (p));

    p[0] = (char *) "white";
    s = skinedit_color_join (p);
    ck_assert_str_eq (s, "white");
    g_free (s);

    p[0] = NULL;
    p[1] = (char *) "black";
    s = skinedit_color_join (p);
    ck_assert_str_eq (s, ";black");
    g_free (s);

    p[0] = (char *) "yellow";
    p[1] = NULL;
    p[2] = (char *) "bold";
    s = skinedit_color_join (p);
    ck_assert_str_eq (s, "yellow;;bold");
    g_free (s);

    p[0] = NULL;
    p[1] = NULL;
    s = skinedit_color_join (p);
    ck_assert_str_eq (s, ";;bold");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_name_ok)
{
    ck_assert (skinedit_model_name_ok ("modarin256"));
    ck_assert (skinedit_model_name_ok ("my skin"));
    ck_assert (!skinedit_model_name_ok (""));
    ck_assert (!skinedit_model_name_ok (NULL));
    ck_assert (!skinedit_model_name_ok ("../victim"));
    ck_assert (!skinedit_model_name_ok ("sub/skin"));
    ck_assert (!skinedit_model_name_ok ("/etc/passwd"));
    ck_assert (!skinedit_model_name_ok ("."));
    ck_assert (!skinedit_model_name_ok (".."));
    ck_assert (!skinedit_model_name_ok ("default.ini"));
    ck_assert_ptr_null (skinedit_model_user_path ("../victim"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_classify)
{
    ck_assert_int_eq (skinedit_color_classify (NULL), SKINEDIT_COLOR_BASIC);
    ck_assert_int_eq (skinedit_color_classify ("black"), SKINEDIT_COLOR_BASIC);
    ck_assert_int_eq (skinedit_color_classify ("brightmagenta"), SKINEDIT_COLOR_BASIC);
    ck_assert_int_eq (skinedit_color_classify ("default"), SKINEDIT_COLOR_BASIC);
    ck_assert_int_eq (skinedit_color_classify ("base"), SKINEDIT_COLOR_BASIC);

    ck_assert_int_eq (skinedit_color_classify ("color0"), SKINEDIT_COLOR_256);
    ck_assert_int_eq (skinedit_color_classify ("color255"), SKINEDIT_COLOR_256);
    ck_assert_int_eq (skinedit_color_classify ("color256"), SKINEDIT_COLOR_UNKNOWN);
    ck_assert_int_eq (skinedit_color_classify ("gray23"), SKINEDIT_COLOR_256);
    ck_assert_int_eq (skinedit_color_classify ("gray24"), SKINEDIT_COLOR_UNKNOWN);
    ck_assert_int_eq (skinedit_color_classify ("rgb505"), SKINEDIT_COLOR_256);
    ck_assert_int_eq (skinedit_color_classify ("rgb506"), SKINEDIT_COLOR_UNKNOWN);
    ck_assert_int_eq (skinedit_color_classify ("rgb50"), SKINEDIT_COLOR_UNKNOWN);

    ck_assert_int_eq (skinedit_color_classify ("#fff"), SKINEDIT_COLOR_TRUECOLOR);
    ck_assert_int_eq (skinedit_color_classify ("#1a2B3c"), SKINEDIT_COLOR_TRUECOLOR);
    ck_assert_int_eq (skinedit_color_classify ("#12345"), SKINEDIT_COLOR_UNKNOWN);
    ck_assert_int_eq (skinedit_color_classify ("#12345g"), SKINEDIT_COLOR_UNKNOWN);

    ck_assert_int_eq (skinedit_color_classify ("MainFg"), SKINEDIT_COLOR_UNKNOWN);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_join);
    tcase_add_test (tc_core, test_name_ok);
    tcase_add_test (tc_core, test_classify);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
