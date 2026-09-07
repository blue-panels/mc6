/*
   lib - tests for mc_pp_add_entry_st()

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

#define TEST_SUITE_NAME "/src/panel-plugins/pp_common"

#include "tests/mctest.h"

#include <string.h>
#include <unistd.h>

#include "lib/panel-plugin.h"
#include "src/filemanager/dir.h"

/* --------------------------------------------------------------------------------------------- */

static dir_list list;

static void
setup (void)
{
    memset (&list, 0, sizeof (list));
}

static void
teardown (void)
{
    int i;

    for (i = 0; i < list.len; i++)
        g_string_free (list.list[i].fname, TRUE);
    g_free (list.list);
}

/* --------------------------------------------------------------------------------------------- */

static struct stat
make_stat (mode_t mode)
{
    struct stat st;

    memset (&st, 0, sizeof (st));
    st.st_mode = mode;
    st.st_uid = 1234;
    st.st_gid = 5678;
    st.st_nlink = 3;
    st.st_size = 42;
    st.st_mtime = 1000;
    return st;
}

/* --------------------------------------------------------------------------------------------- */

/* The whole struct stat reaches the entry, the owner included. */
START_TEST (test_add_entry_st_keeps_stat)
{
    struct stat st = make_stat (S_IFREG | 0644);
    const file_entry_t *fe;

    mc_pp_add_entry_st (&list, "file", &st, MC_PP_ENTRY_NONE);

    ck_assert_int_eq (list.len, 1);
    fe = &list.list[0];
    ck_assert_str_eq (fe->fname->str, "file");
    ck_assert_int_eq (fe->st.st_mode, S_IFREG | 0644);
    ck_assert_int_eq (fe->st.st_uid, 1234);
    ck_assert_int_eq (fe->st.st_gid, 5678);
    ck_assert_int_eq (fe->st.st_nlink, 3);
    ck_assert_int_eq (fe->st.st_size, 42);
    ck_assert_int_eq (fe->st.st_mtime, 1000);
    ck_assert_int_eq (fe->f.link_to_dir, 0);
    ck_assert_int_eq (fe->f.stale_link, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A link to a directory stays a link in the mode and gets the flag. */
START_TEST (test_add_entry_st_link_to_dir)
{
    struct stat st = make_stat (S_IFLNK | 0777);
    const file_entry_t *fe;

    mc_pp_add_entry_st (&list, "bin", &st, MC_PP_ENTRY_LINK_TO_DIR);

    fe = &list.list[0];
    ck_assert (S_ISLNK (fe->st.st_mode));
    ck_assert_int_eq (fe->f.link_to_dir, 1);
    ck_assert_int_eq (fe->f.stale_link, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_add_entry_st_stale_link)
{
    struct stat st = make_stat (S_IFLNK | 0777);
    const file_entry_t *fe;

    mc_pp_add_entry_st (&list, "dangling", &st, MC_PP_ENTRY_STALE_LINK);

    fe = &list.list[0];
    ck_assert_int_eq (fe->f.link_to_dir, 0);
    ck_assert_int_eq (fe->f.stale_link, 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The link flags are about a link: a directory gets none. */
START_TEST (test_add_entry_st_directory)
{
    struct stat st = make_stat (S_IFDIR | 0755);

    mc_pp_add_entry_st (&list, "etc", &st, MC_PP_ENTRY_NONE);

    ck_assert (S_ISDIR (list.list[0].st.st_mode));
    ck_assert_int_eq (list.list[0].f.link_to_dir, 0);
    ck_assert_int_eq (list.list[0].f.stale_link, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The short form makes the entry the local user's. */
START_TEST (test_add_entry_owner_is_local)
{
    const file_entry_t *fe;

    mc_pp_add_entry (&list, "file", S_IFREG | 0644, 7, 1000);

    fe = &list.list[0];
    ck_assert_int_eq (fe->st.st_uid, getuid ());
    ck_assert_int_eq (fe->st.st_gid, getgid ());
    ck_assert_int_eq (fe->st.st_nlink, 1);
    ck_assert_int_eq (fe->st.st_size, 7);
    ck_assert_int_eq (fe->f.link_to_dir, 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_add_entry_grows_the_list)
{
    struct stat st = make_stat (S_IFREG | 0644);
    int i;

    for (i = 0; i < 300; i++)
    {
        char name[16];

        g_snprintf (name, sizeof (name), "f%d", i);
        mc_pp_add_entry_st (&list, name, &st, MC_PP_ENTRY_NONE);
    }

    ck_assert_int_eq (list.len, 300);
    ck_assert_str_eq (list.list[299].fname->str, "f299");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_add_entry_st_keeps_stat);
    tcase_add_test (tc_core, test_add_entry_st_link_to_dir);
    tcase_add_test (tc_core, test_add_entry_st_stale_link);
    tcase_add_test (tc_core, test_add_entry_st_directory);
    tcase_add_test (tc_core, test_add_entry_owner_is_local);
    tcase_add_test (tc_core, test_add_entry_grows_the_list);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
