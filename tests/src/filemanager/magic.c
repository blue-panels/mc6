/*
   src/filemanager - tests for magic.ini parser

   Copyright (C) 2026
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the Free
   Software Foundation, either version 3 of the License, or (at your option) any
   later version.
 */

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include "src/filemanager/magic.c"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_plugin_operation)
{
    mc_magic_action_t action = { NULL, NULL };

    ck_assert_int_eq (magic_parse_action ("  %plugin{arcmc:open}  ", &action),
                      MC_MAGIC_ACTION_FOUND);
    ck_assert_str_eq (action.plugin_name, "arcmc");
    ck_assert_str_eq (action.operation_name, "open");

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

    ck_assert_int_eq (magic_parse_action ("%plugin{arcmc}", &action), MC_MAGIC_ACTION_ERROR);
    ck_assert_int_eq (magic_parse_action ("%plugin{arcmc:open:again}", &action),
                      MC_MAGIC_ACTION_ERROR);
    ck_assert_int_eq (magic_parse_action ("%plugin{arcmc:open} extra", &action),
                      MC_MAGIC_ACTION_ERROR);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_test (tc_core, test_parse_plugin_operation);
    tcase_add_test (tc_core, test_reject_non_plugin_action);
    tcase_add_test (tc_core, test_reject_malformed_plugin_operation);

    return mctest_run_all (tc_core);
}
