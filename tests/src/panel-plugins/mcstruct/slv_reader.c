/*
   src/panel-plugins/mcstruct - tests for the file reader and the def-file loader

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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

#define TEST_SUITE_NAME "/src/panel-plugins/mcstruct"

#include "tests/mctest.h"

#include <string.h>
#include <unistd.h>

#include "lib/strutil.h"
#include "lib/vfs/vfs.h"
#include "src/vfs/local/local.c"

#include "src/panel-plugins/mcstruct/slv_reader.h"
#include "src/panel-plugins/mcstruct/slv_load.h"

/* --------------------------------------------------------------------------------------------- */

static char *tmpdir;

static void
setup (void)
{
    str_init_strings (NULL);
    vfs_init ();
    vfs_init_localfs ();
    tmpdir = g_dir_make_tmp ("mcstruct-XXXXXX", NULL);
    ck_assert_ptr_nonnull (tmpdir);
}

static void
teardown (void)
{
    char *cmd = g_strdup_printf ("rm -rf '%s'", tmpdir);

    ck_assert_int_eq (system (cmd), 0);
    g_free (cmd);
    g_free (tmpdir);
    slv_load_set_search_dirs (NULL);
    vfs_shut ();
    str_uninit_strings ();
}

static char *
write_file (const char *name, const char *content, gsize len)
{
    char *path = g_build_filename (tmpdir, name, (char *) NULL);

    ck_assert (g_file_set_contents (path, content, (gssize) len, NULL));
    return path;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_reader_overlay_and_save)
{
    char *path = write_file ("data.bin", "0123456789", 10);
    slv_file_reader_t *fr;
    GError *error = NULL;
    unsigned char buf[10];
    char *saved = NULL;

    fr = slv_file_reader_open (path, &error);
    ck_assert_msg (fr != NULL, "%s", error != NULL ? error->message : "?");
    ck_assert_int_eq (fr->size, 10);
    ck_assert_int_eq (fr->reader.size (fr->reader.ctx), 10);

    ck_assert_int_eq (fr->reader.read (fr->reader.ctx, 8, buf, 10), 2);
    ck_assert_int_eq (buf[0], '8');
    ck_assert_int_eq (fr->reader.read (fr->reader.ctx, 10, buf, 1), 0);

    slv_file_reader_set_byte (fr, 3, 'x');
    slv_file_reader_set_byte (fr, 1, 'y');
    slv_file_reader_set_byte (fr, 3, 'z'); /* replaces the pending value */
    ck_assert_int_eq (slv_file_reader_change_count (fr), 2);
    ck_assert (slv_file_reader_is_changed (fr, 1));
    ck_assert (slv_file_reader_is_changed (fr, 3));
    ck_assert (!slv_file_reader_is_changed (fr, 2));

    ck_assert_int_eq (fr->reader.read (fr->reader.ctx, 0, buf, 10), 10);
    ck_assert (memcmp (buf, "0y2z456789", 10) == 0);

    /* writing the original byte back drops the edit */
    slv_file_reader_set_byte (fr, 1, '1');
    ck_assert_int_eq (slv_file_reader_change_count (fr), 1);

    ck_assert (g_file_get_contents (path, &saved, NULL, NULL));
    ck_assert (memcmp (saved, "0123456789", 10) == 0); /* nothing on disk yet */
    g_free (saved);

    ck_assert (slv_file_reader_save (fr, &error));
    ck_assert_int_eq (slv_file_reader_change_count (fr), 0);
    ck_assert (g_file_get_contents (path, &saved, NULL, NULL));
    ck_assert (memcmp (saved, "012z456789", 10) == 0);
    g_free (saved);

    slv_file_reader_set_byte (fr, 0, 'q');
    slv_file_reader_discard (fr);
    ck_assert_int_eq (slv_file_reader_change_count (fr), 0);
    ck_assert_int_eq (fr->reader.read (fr->reader.ctx, 0, buf, 1), 1);
    ck_assert_int_eq (buf[0], '0');

    slv_file_reader_free (fr);
    g_free (path);

    fr = slv_file_reader_open ("/nonexistent/file", &error);
    ck_assert_ptr_null (fr);
    ck_assert_ptr_nonnull (error);
    g_error_free (error);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_find_def)
{
    char *user_dir = g_build_filename (tmpdir, "user", (char *) NULL);
    char *sys_dir = g_build_filename (tmpdir, "sys", (char *) NULL);
    const char *dirs[3];
    char *p, *found;
    GPtrArray *list;

    ck_assert_int_eq (g_mkdir_with_parents (user_dir, 0700), 0);
    ck_assert_int_eq (g_mkdir_with_parents (sys_dir, 0700), 0);
    dirs[0] = user_dir;
    dirs[1] = sys_dir;
    dirs[2] = NULL;
    slv_load_set_search_dirs (dirs);

    p = g_build_filename (sys_dir, "zip.stl", (char *) NULL);
    ck_assert (g_file_set_contents (p, "STL 4.00\n/Zip\n w 1 x\n", -1, NULL));
    g_free (p);
    p = g_build_filename (sys_dir, "exe.stl", (char *) NULL);
    ck_assert (g_file_set_contents (p, "STL 4.00\n/Exe\n w 1 x\n", -1, NULL));
    g_free (p);
    p = g_build_filename (user_dir, "exe.stl", (char *) NULL);
    ck_assert (g_file_set_contents (p, "STL 4.00\n/UserExe\n w 1 x\n", -1, NULL));
    g_free (p);
    p = g_build_filename (user_dir, "stl.als", (char *) NULL);
    ck_assert (
        g_file_set_contents (p, "; aliases\nexe.stl: *.dll  *.drv\nzip.stl: *.jar\n", -1, NULL));
    g_free (p);

    /* hint: magic group name */
    found = slv_load_find_def ("whatever.bin", "mcstruct-zip");
    ck_assert_ptr_nonnull (found);
    ck_assert (g_str_has_suffix (found, "/sys/zip.stl"));
    g_free (found);

    /* hint: def name, user file shadows the system one */
    found = slv_load_find_def (NULL, "exe");
    ck_assert_ptr_nonnull (found);
    ck_assert (g_str_has_suffix (found, "/user/exe.stl"));
    g_free (found);

    /* alias */
    found = slv_load_find_def ("/some/where/LIB.DLL", NULL);
    ck_assert_ptr_nonnull (found);
    ck_assert (g_str_has_suffix (found, "/user/exe.stl"));
    g_free (found);
    found = slv_load_find_def ("app.jar", NULL);
    ck_assert_ptr_nonnull (found);
    ck_assert (g_str_has_suffix (found, "/sys/zip.stl"));
    g_free (found);

    /* signature beats the extension, the file has to exist for it */
    p = g_build_filename (user_dir, "stl.als", (char *) NULL);
    ck_assert (
        g_file_set_contents (p, "exe.stl: *.dll @0:4D5A\nzip.stl: *.jar @2:504B\n", -1, NULL));
    g_free (p);
    p = g_build_filename (tmpdir, "firmware.bin", (char *) NULL);
    ck_assert (g_file_set_contents (p, "MZ\x00\x01", 4, NULL));
    found = slv_load_find_def (p, NULL);
    ck_assert_ptr_nonnull (found);
    ck_assert (g_str_has_suffix (found, "/user/exe.stl"));
    g_free (found);
    ck_assert (g_file_set_contents (p, "xxPK", 4, NULL));
    found = slv_load_find_def (p, NULL);
    ck_assert_ptr_nonnull (found);
    ck_assert (g_str_has_suffix (found, "/sys/zip.stl"));
    g_free (found);
    ck_assert (g_file_set_contents (p, "xx", 2, NULL));
    found = slv_load_find_def (p, NULL);
    ck_assert_ptr_null (found);
    g_free (p);

    /* extension */
    found = slv_load_find_def ("archive.ZIP", NULL);
    ck_assert_ptr_nonnull (found);
    ck_assert (g_str_has_suffix (found, "/sys/zip.stl"));
    g_free (found);

    /* nothing */
    found = slv_load_find_def ("notes.txt", NULL);
    ck_assert_ptr_null (found);
    found = slv_load_find_def ("notes.txt", "nosuch");
    ck_assert_ptr_null (found);

    /* listing: user first, no duplicates */
    list = slv_load_list_defs ();
    ck_assert_int_eq (list->len, 2);
    ck_assert (g_str_has_suffix (g_ptr_array_index (list, 0), "/user/exe.stl"));
    ck_assert (g_str_has_suffix (g_ptr_array_index (list, 1), "/sys/zip.stl"));
    g_ptr_array_free (list, TRUE);

    g_free (user_dir);
    g_free (sys_dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_settings)
{
    char *dir = g_build_filename (tmpdir, "cfg", (char *) NULL);
    const char *dirs[2];
    char *p;
    slv_settings_t s;

    ck_assert_int_eq (g_mkdir_with_parents (dir, 0700), 0);
    dirs[0] = dir;
    dirs[1] = NULL;
    slv_load_set_search_dirs (dirs);

    slv_settings_load (&s);
    ck_assert_int_eq (s.tree_lines, 14);
    ck_assert_int_eq (s.hex_lines, 4);
    ck_assert_int_eq (s.def_lines, 0);
    ck_assert_int_eq (s.offset_column, 2);
    ck_assert_str_eq (s.float_format, "%g");
    slv_settings_free (&s);

    p = g_build_filename (dir, "mcstruct.ini", (char *) NULL);
    ck_assert (g_file_set_contents (p,
                                    "[mcstruct]\nHexLines=7\nDefLines=0\nOffsetColumn=local\n"
                                    "NameWidth=99\nFloatingPointFormat=%.3f\nLazyRows=0\n",
                                    -1, NULL));
    g_free (p);
    slv_settings_load (&s);
    ck_assert_int_eq (s.hex_lines, 7);
    ck_assert_int_eq (s.def_lines, 0);
    ck_assert_int_eq (s.offset_column, 1);
    ck_assert_int_eq (s.name_width, 40);
    ck_assert_int_eq (s.lazy_rows, 1);
    ck_assert_str_eq (s.float_format, "%.3f");

    /* save writes the user file on the first search dir */
    s.hex_lines = 2;
    s.offset_column = 0;
    s.show_hidden = TRUE;
    ck_assert (slv_settings_save (&s));
    slv_settings_free (&s);
    slv_settings_load (&s);
    ck_assert_int_eq (s.hex_lines, 2);
    ck_assert_int_eq (s.def_lines, 0);
    ck_assert_int_eq (s.offset_column, 0);
    ck_assert (s.show_hidden);
    ck_assert_str_eq (s.float_format, "%.3f");
    slv_settings_free (&s);

    g_free (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_float_format_valid)
{
    ck_assert (slv_float_format_valid ("%g"));
    ck_assert (slv_float_format_valid ("%.3f"));
    ck_assert (slv_float_format_valid ("%-12.4E"));
    ck_assert (!slv_float_format_valid (NULL));
    ck_assert (!slv_float_format_valid (""));
    ck_assert (!slv_float_format_valid ("%s%f"));
    ck_assert (!slv_float_format_valid ("%n"));
    ck_assert (!slv_float_format_valid ("%Lf"));
    ck_assert (!slv_float_format_valid ("%f "));
    ck_assert (!slv_float_format_valid ("x%f"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_reader_overlay_and_save);
    tcase_add_test (tc_core, test_find_def);
    tcase_add_test (tc_core, test_settings);
    tcase_add_test (tc_core, test_float_format_valid);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
