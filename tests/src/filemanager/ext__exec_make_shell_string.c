/*
   src/filemanager - exec_make_shell_string() function testing

   Copyright (C) 2026
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include "src/vfs/local/local.h"

#include "src/filemanager/ext.c"

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
}

static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_cd_path_longer_than_buffer)
{
    GString *path, *shell_string, *cd_path = NULL;
    char *component;
    vfs_path_t *filename_vpath;
    int i;

    component = g_strnfill (200, 'x');
    path = g_string_new ("/tmp");
    for (i = 0; i < 6; i++)
    {
        g_string_append_c (path, PATH_SEP);
        g_string_append (path, component);
    }
    g_string_append (path, "/trigger.5115");

    filename_vpath = vfs_path_from_str (path->str);
    shell_string = exec_make_shell_string ("%cd %f", filename_vpath, &cd_path);

    ck_assert_ptr_nonnull (cd_path);
    ck_assert_uint_gt (cd_path->len, BUF_1K);
    mctest_assert_str_eq (shell_string->str, "");
    mctest_assert_str_eq (cd_path->str, path->str);

    g_string_free (cd_path, TRUE);
    g_string_free (shell_string, TRUE);
    vfs_path_free (filename_vpath, TRUE);
    g_string_free (path, TRUE);
    g_free (component);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_cd_path_longer_than_buffer);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
