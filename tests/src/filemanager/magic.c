/*
   src/filemanager - tests for magic.ini parser

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

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include "src/filemanager/magic.c"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    mc_magic_flush ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_plugin_operation)
{
    mc_magic_action_t action = { NULL, NULL };

    ck_assert_int_eq (magic_parse_action ("  %plugin{arcmc:open}  ", &action),
                      MC_MAGIC_ACTION_FOUND);
    ck_assert_str_eq (action.plugin_name, "arcmc");
    ck_assert_str_eq (action.operation_name, "open");
    mc_magic_action_clear (&action);

    /* the characters an identifier is allowed to be made of */
    ck_assert_int_eq (magic_parse_action ("%plugin{a-b_1:o-p_2}", &action), MC_MAGIC_ACTION_FOUND);
    ck_assert_str_eq (action.plugin_name, "a-b_1");
    ck_assert_str_eq (action.operation_name, "o-p_2");
    mc_magic_action_clear (&action);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reject_non_plugin_action)
{
    mc_magic_action_t action = { NULL, NULL };

    ck_assert_int_eq (magic_parse_action ("archive.sh view tar", &action), MC_MAGIC_ACTION_ERROR);
    mctest_assert_null (action.plugin_name);
    mctest_assert_null (action.operation_name);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reject_malformed_plugin_operation)
{
    mc_magic_action_t action = { NULL, NULL };

    static const char *const rejected[] = {
        "%plugin{arcmc}",             // no operation
        "%plugin{arcmc:open:again}",  // one separator, no more
        "%plugin{arcmc:open} extra",  // the directive is the whole value
        "%plugin{arcmc:open",         // unterminated
        "%plugin{arcmc:open}}",       // and not over-terminated either
        "%plugin{:open}",             // an empty name is not a name
        "%plugin{arcmc:}",
        "%plugin{}",
        "%plugin{ arcmc : open }",  // whitespace belongs outside the braces
        "%plugin{arcmc:open with space}",
        "%plugin{arc mc:open}",
        "",
        "archive.sh view tar",
    };
    gsize i;

    for (i = 0; i < G_N_ELEMENTS (rejected); i++)
    {
        ck_assert_int_eq (magic_parse_action (rejected[i], &action), MC_MAGIC_ACTION_ERROR);
        mctest_assert_null (action.plugin_name);
        mctest_assert_null (action.operation_name);
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

    tcase_add_test (tc_core, test_parse_plugin_operation);
    tcase_add_test (tc_core, test_reject_non_plugin_action);
    tcase_add_test (tc_core, test_reject_malformed_plugin_operation);

    return mctest_run_all (tc_core);
}
