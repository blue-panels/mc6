/*
   src/panel-plugins/shell-link - tests for the listing parser

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

#define TEST_SUITE_NAME "/src/panel-plugins/shell-link"

#include "tests/mctest.h"

#include <string.h>

#include "src/panel-plugins/shell-link/shfs.h"

void shfs_parse_ls (char *buffer, shfs_entry_t *ent);

/* --------------------------------------------------------------------------------------------- */

static shfs_entry_t ent;

static void
setup (void)
{
    memset (&ent, 0, sizeof (ent));
}

static void
teardown (void)
{
    g_free (ent.name);
    g_free (ent.linkname);
}

/* Feed the lines of one record the way shfs_list_dir() does: one call per line. */
static void
feed (const char *first, ...)
{
    va_list ap;
    const char *line;

    va_start (ap, first);
    for (line = first; line != NULL; line = va_arg (ap, const char *))
    {
        char *copy = g_strdup (line);

        shfs_parse_ls (copy, &ent);
        g_free (copy);
    }
    va_end (ap);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_link_to_directory)
{
    feed ("R777 120000 1000.1001", "S7", "Td", ":\"bin\" -> \"usr/bin\"", NULL);

    ck_assert (S_ISLNK (ent.st.st_mode));
    ck_assert_int_eq (ent.st.st_uid, 1000);
    ck_assert_int_eq (ent.st.st_gid, 1001);
    ck_assert_int_eq (ent.st.st_size, 7);
    ck_assert (ent.link_to_dir);
    ck_assert (!ent.stale_link);
    ck_assert_str_eq (ent.name, "bin");
    ck_assert_str_eq (ent.linkname, "usr/bin");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_link_to_file)
{
    feed ("R777 120000 0.0", "T-", ":\"pw\" -> \"/etc/passwd\"", NULL);

    ck_assert (S_ISLNK (ent.st.st_mode));
    ck_assert (!ent.link_to_dir);
    ck_assert (!ent.stale_link);
    ck_assert_str_eq (ent.linkname, "/etc/passwd");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_dangling_link)
{
    feed ("Plrwxrwxrwx 0.0", "T!", ":\"dangling\" -> \"nowhere\"", NULL);

    ck_assert (S_ISLNK (ent.st.st_mode));
    ck_assert (!ent.link_to_dir);
    ck_assert (ent.stale_link);
    ck_assert_str_eq (ent.name, "dangling");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* A helper of revision 1 sends no T line: the link is a link to a file. */
START_TEST (test_link_without_t_line)
{
    feed ("Plrwxrwxrwx 0.0", "S7", ":\"bin\" -> \"usr/bin\"", NULL);

    ck_assert (S_ISLNK (ent.st.st_mode));
    ck_assert (!ent.link_to_dir);
    ck_assert (!ent.stale_link);
    ck_assert_str_eq (ent.linkname, "usr/bin");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_directory_and_file)
{
    feed ("Pdrwxr-xr-x 0.0", "S4096", ":\"etc\"", NULL);

    ck_assert (S_ISDIR (ent.st.st_mode));
    ck_assert (!ent.link_to_dir);
    ck_assert (!ent.stale_link);
    ck_assert_str_eq (ent.name, "etc");
    ck_assert (ent.linkname == NULL);

    g_free (ent.name);
    memset (&ent, 0, sizeof (ent));

    feed ("P-rw-r--r-- 1000.1000", "S3526", ":\".bashrc\"", NULL);

    ck_assert (S_ISREG (ent.st.st_mode));
    ck_assert_int_eq (ent.st.st_uid, 1000);
    ck_assert_str_eq (ent.name, ".bashrc");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* The poor_ls branch sends the names unquoted; "." and ".." are skipped there too. */
START_TEST (test_dot_names_are_skipped)
{
    feed ("Pdrwxr-xr-x 0.0", ":.", NULL);
    ck_assert (ent.name == NULL);

    feed ("Pdrwxr-xr-x 0.0", ":..", NULL);
    ck_assert (ent.name == NULL);

    feed ("Pdrwxr-xr-x 0.0", ":\".\"", NULL);
    ck_assert (ent.name == NULL);

    feed ("Pdrwxr-xr-x 0.0", ":.x", NULL);
    ck_assert_str_eq (ent.name, ".x");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_link_to_directory);
    tcase_add_test (tc_core, test_link_to_file);
    tcase_add_test (tc_core, test_dangling_link);
    tcase_add_test (tc_core, test_link_without_t_line);
    tcase_add_test (tc_core, test_directory_and_file);
    tcase_add_test (tc_core, test_dot_names_are_skipped);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
