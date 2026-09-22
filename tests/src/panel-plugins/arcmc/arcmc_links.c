/*
   src/panel-plugins/arcmc - tests for the entries of an archive

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

#define TEST_SUITE_NAME "/src/panel-plugins/arcmc"

#include "tests/mctest.h"

#include <string.h>

#include "src/panel-plugins/arcmc/arcmc-types.h"
#include "src/panel-plugins/arcmc/archive-io.h"

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *entries;

static void
setup (void)
{
    entries = g_ptr_array_new_with_free_func (arcmc_entry_free);
}

static void
teardown (void)
{
    g_ptr_array_free (entries, TRUE);
}

static arcmc_entry_t *
add (const char *full_path, mode_t mode, const char *linkname)
{
    arcmc_entry_t *e = g_new0 (arcmc_entry_t, 1);
    const char *slash = strrchr (full_path, '/');

    e->full_path = g_strdup (full_path);
    e->name = g_strdup (slash != NULL ? slash + 1 : full_path);
    e->mode = mode;
    e->linkname = g_strdup (linkname);
    g_ptr_array_add (entries, e);
    return e;
}

static const arcmc_entry_t *
get (const char *full_path)
{
    return arcmc_find_entry (entries, full_path);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_links_are_resolved)
{
    add ("usr", S_IFDIR | 0755, NULL);
    add ("usr/bin", S_IFDIR | 0755, NULL);
    add ("usr/bin/sh", S_IFREG | 0755, NULL);
    add ("bin", S_IFLNK | 0777, "usr/bin");
    add ("sbin", S_IFLNK | 0777, "/usr/bin");
    add ("x", S_IFDIR | 0755, NULL);
    add ("x/y", S_IFDIR | 0755, NULL);
    add ("x/y/up", S_IFLNK | 0777, "../../usr/bin/sh");
    add ("x/y/dot", S_IFLNK | 0777, "./../../usr/./bin");
    add ("tolink", S_IFLNK | 0777, "bin");
    add ("dangling", S_IFLNK | 0777, "nowhere");
    add ("loop1", S_IFLNK | 0777, "loop2");
    add ("loop2", S_IFLNK | 0777, "loop1");
    add ("root", S_IFLNK | 0777, "/");
    add ("empty", S_IFLNK | 0777, "");

    arcmc_resolve_links (entries);

    ck_assert (get ("bin")->link_to_dir);
    ck_assert_str_eq (get ("bin")->link_path, "usr/bin");
    ck_assert (!get ("bin")->stale_link);

    ck_assert (get ("sbin")->link_to_dir);
    ck_assert_str_eq (get ("sbin")->link_path, "usr/bin");

    ck_assert (!get ("x/y/up")->link_to_dir);
    ck_assert (!get ("x/y/up")->stale_link);
    ck_assert_str_eq (get ("x/y/up")->link_path, "usr/bin/sh");

    ck_assert (get ("x/y/dot")->link_to_dir);
    ck_assert_str_eq (get ("x/y/dot")->link_path, "usr/bin");

    ck_assert (get ("tolink")->link_to_dir);
    ck_assert_str_eq (get ("tolink")->link_path, "usr/bin");

    ck_assert (get ("dangling")->stale_link);
    ck_assert (get ("dangling")->link_path == NULL);
    ck_assert (get ("loop1")->stale_link);
    ck_assert (get ("loop2")->stale_link);
    ck_assert (get ("root")->stale_link);
    ck_assert (get ("empty")->stale_link);

    // the others are left alone
    ck_assert (!get ("usr/bin")->link_to_dir);
    ck_assert (!get ("usr/bin/sh")->stale_link);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_compute_dir_size)
{
    arcmc_entry_t *dir;

    dir = add ("etc", S_IFDIR | 0755, NULL);
    add ("etc/sub", S_IFDIR | 0755, NULL);
    add ("etc/config", S_IFREG | 0644, NULL)->size = 12;
    add ("etc/sub/data", S_IFREG | 0644, NULL)->size = 30;
    add ("other", S_IFREG | 0644, NULL)->size = 50;
    // a name that starts with "etc" is not inside it
    add ("etc-old", S_IFDIR | 0755, NULL);
    add ("etc-old/data", S_IFREG | 0644, NULL)->size = 100;

    ck_assert (arcmc_compute_dir_size (entries, "etc"));
    ck_assert (dir->dir_size_computed);
    ck_assert_int_eq (dir->dir_size, 42);

    // a file and a name of nothing are not directories
    ck_assert (!arcmc_compute_dir_size (entries, "other"));
    ck_assert (!arcmc_compute_dir_size (entries, "nowhere"));
    ck_assert (!arcmc_compute_dir_size (entries, ""));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Every directory of one level, in a single pass. */
START_TEST (test_compute_level_dir_sizes)
{
    add ("etc", S_IFDIR | 0755, NULL);
    add ("etc/sub", S_IFDIR | 0755, NULL);
    add ("etc/config", S_IFREG | 0644, NULL)->size = 12;
    add ("etc/sub/data", S_IFREG | 0644, NULL)->size = 30;
    add ("var", S_IFDIR | 0755, NULL);
    add ("var/log", S_IFREG | 0644, NULL)->size = 7;
    add ("empty", S_IFDIR | 0755, NULL);
    add ("loose", S_IFREG | 0644, NULL)->size = 99;

    arcmc_compute_level_dir_sizes (entries, "");

    ck_assert (get ("etc")->dir_size_computed);
    ck_assert_int_eq (get ("etc")->dir_size, 42);
    ck_assert_int_eq (get ("var")->dir_size, 7);
    ck_assert (get ("empty")->dir_size_computed);
    ck_assert_int_eq (get ("empty")->dir_size, 0);
    // a file of this level is not a directory of it
    ck_assert (!get ("loose")->dir_size_computed);
    // a directory of a level below is left alone
    ck_assert (!get ("etc/sub")->dir_size_computed);

    arcmc_compute_level_dir_sizes (entries, "etc");

    ck_assert (get ("etc/sub")->dir_size_computed);
    ck_assert_int_eq (get ("etc/sub")->dir_size, 30);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A second pass starts over, as after a reload. */
START_TEST (test_resolve_twice)
{
    add ("d", S_IFDIR | 0755, NULL);
    add ("l", S_IFLNK | 0777, "d");

    arcmc_resolve_links (entries);
    arcmc_resolve_links (entries);

    ck_assert (get ("l")->link_to_dir);
    ck_assert_str_eq (get ("l")->link_path, "d");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_links_are_resolved);
    tcase_add_test (tc_core, test_compute_dir_size);
    tcase_add_test (tc_core, test_compute_level_dir_sizes);
    tcase_add_test (tc_core, test_resolve_twice);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
