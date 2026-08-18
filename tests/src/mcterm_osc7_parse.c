/*
   tests/src/mcterm_osc7_parse.c -- unit tests for OSC 7 URI path extraction

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

#define TEST_SUITE_NAME "/src/mcterm_osc7_parse"

#include "tests/mctest.h"

#include "lib/strutil.h"
#include "src/mcterm/mcterm_cwd.h"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_empty_host_hash_in_path)
{
    char *path;

    path = mcterm_osc7_uri_to_path ("7;file:///tmp/a#b", NULL);
    ck_assert_ptr_nonnull (path);
    ck_assert_str_eq (path, "/tmp/a#b");
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_empty_host_percent_encoded_space)
{
    char *path;

    path = mcterm_osc7_uri_to_path ("7;file:///tmp/a%20b", NULL);
    ck_assert_ptr_nonnull (path);
    ck_assert_str_eq (path, "/tmp/a b");
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_localhost_accepted)
{
    char *path;

    path = mcterm_osc7_uri_to_path ("7;file://localhost/tmp", NULL);
    ck_assert_ptr_nonnull (path);
    ck_assert_str_eq (path, "/tmp");
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_remote_host_rejected)
{
    char *path;

    path = mcterm_osc7_uri_to_path ("7;file://remote-host/tmp", NULL);
    ck_assert_ptr_null (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_missing_prefix_rejected)
{
    ck_assert_ptr_null (mcterm_osc7_uri_to_path (NULL, NULL));
    ck_assert_ptr_null (mcterm_osc7_uri_to_path ("", NULL));
    ck_assert_ptr_null (mcterm_osc7_uri_to_path ("file:///tmp", NULL));
    ck_assert_ptr_null (mcterm_osc7_uri_to_path ("7;ftp://localhost/tmp", NULL));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_token_accepted_and_stripped)
{
    char *path;

    path = mcterm_osc7_uri_to_path ("7;file:///tmp/a?mc=deadbeef", "deadbeef");
    ck_assert_ptr_nonnull (path);
    ck_assert_str_eq (path, "/tmp/a");
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_foreign_token_rejected)
{
    // the shell on the far side of ssh, or the output of a command
    ck_assert_ptr_null (mcterm_osc7_uri_to_path ("7;file:///tmp/a?mc=0badc0de", "deadbeef"));
    ck_assert_ptr_null (mcterm_osc7_uri_to_path ("7;file:///tmp/a", "deadbeef"));
    ck_assert_ptr_null (mcterm_osc7_uri_to_path ("7;file:///tmp/a?mc=", "deadbeef"));
    ck_assert_ptr_null (mcterm_osc7_uri_to_path ("7;file:///tmp/a?deadbeef", "deadbeef"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_token_with_encoded_question_mark_in_path)
{
    char *path;

    // '?' is not in the safe set, so a real path carries it encoded: the token stays the only one
    path = mcterm_osc7_uri_to_path ("7;file:///tmp/a%3Fb?mc=deadbeef", "deadbeef");
    ck_assert_ptr_nonnull (path);
    ck_assert_str_eq (path, "/tmp/a?b");
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    str_init_strings ("UTF-8");

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_empty_host_hash_in_path);
    tcase_add_test (tc_core, test_empty_host_percent_encoded_space);
    tcase_add_test (tc_core, test_localhost_accepted);
    tcase_add_test (tc_core, test_remote_host_rejected);
    tcase_add_test (tc_core, test_missing_prefix_rejected);
    tcase_add_test (tc_core, test_token_accepted_and_stripped);
    tcase_add_test (tc_core, test_foreign_token_rejected);
    tcase_add_test (tc_core, test_token_with_encoded_question_mark_in_path);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
