/*
   src/panel-plugins/skineditor - character and string entries

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

#include "test_common.h"

/* --------------------------------------------------------------------------------------------- */

static const char *skin_text = "[skin]\n"
                               "    description = Test skin\n"
                               "\n"
                               "[lines]\n"
                               "    horiz = =\n"
                               "    vert = |\n"
                               "\n"
                               "[Lines]\n"
                               "    cross = #\n"
                               "    horiz = %\n"
                               "\n"
                               "[widget-panel]\n"
                               "    sort-up-char = ^\n"
                               "    extra-char = ~\n"
                               "\n"
                               "[core]\n"
                               "    _default_ = lightgray;blue\n"
                               "    spinner_sequence = abc\n";

static skinedit_model_t *model = NULL;

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    char *path;
    GError *error = NULL;

    test_vfs_init ();
    path = test_write_skin ("test.ini", skin_text);
    model = skinedit_model_open_file (path, NULL, &error);
    ck_assert_msg (model != NULL, "%s", error != NULL ? error->message : "?");
    g_free (path);
}

static void
teardown (void)
{
    skinedit_model_free (model);
    model = NULL;
    test_vfs_deinit ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_read)
{
    const skinedit_entry_t *e;

    e = skinedit_model_find (model, "lines", "horiz");
    ck_assert_int_eq (e->kind, SKINEDIT_ENTRY_CHAR);
    ck_assert_str_eq (e->raw[0], "=");
    ck_assert_str_eq (e->builtin, "\xe2\x94\x80");

    e = skinedit_model_find (model, "lines", "lefttop");
    ck_assert_ptr_null (e->raw[0]);
    ck_assert_str_eq (e->builtin, "\xe2\x94\x8c");

    e = skinedit_model_find (model, "widget-panel", "extra-char");
    ck_assert_ptr_nonnull (e);
    ck_assert (!e->known);
    ck_assert_int_eq (e->kind, SKINEDIT_ENTRY_CHAR);
    ck_assert_str_eq (e->raw[0], "~");

    e = skinedit_model_find (model, "core", "spinner_sequence");
    ck_assert_int_eq (e->kind, SKINEDIT_ENTRY_STRING);
    ck_assert_str_eq (e->raw[0], "abc");
    ck_assert_str_eq (e->builtin, "|/-\\");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* [Lines] of old skins is read when [lines] lacks the key, and left behind on write */
START_TEST (test_legacy_lines)
{
    skinedit_entry_t *e;

    e = skinedit_model_find (model, "lines", "cross");
    ck_assert_str_eq (e->raw[0], "#");
    e = skinedit_model_find (model, "lines", "horiz");
    ck_assert_str_eq (e->raw[0], "=");

    e = skinedit_model_find (model, "lines", "cross");
    skinedit_model_set_text (model, e, "+");
    ck_assert (mc_config_has_param (model->config, "lines", "cross"));
    ck_assert (!mc_config_has_param (model->config, "Lines", "cross"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_set_text)
{
    skinedit_entry_t *e;

    e = skinedit_model_find (model, "lines", "lefttop");
    skinedit_model_set_text (model, e, "+");
    ck_assert_str_eq (e->raw[0], "+");
    ck_assert (skinedit_entry_changed (e));
    ck_assert (mc_config_has_param (model->config, "lines", "lefttop"));

    /* the built-in value is not written */
    e = skinedit_model_find (model, "lines", "horiz");
    skinedit_model_set_text (model, e, "\xe2\x94\x80");
    ck_assert_ptr_null (e->raw[0]);
    ck_assert (!mc_config_has_param (model->config, "lines", "horiz"));

    e = skinedit_model_find (model, "lines", "vert");
    skinedit_model_set_text (model, e, NULL);
    ck_assert_ptr_null (e->raw[0]);
    ck_assert (!mc_config_has_param (model->config, "lines", "vert"));

    e = skinedit_model_find (model, "core", "spinner_sequence");
    skinedit_model_set_text (model, e, "|/-\\");
    ck_assert_ptr_null (e->raw[0]);
    ck_assert (!mc_config_has_param (model->config, "core", "spinner_sequence"));

    skinedit_model_reset (model, e);
    ck_assert_str_eq (e->raw[0], "abc");
    ck_assert_str_eq (e->shown, "abc");

    /* the raw setter takes the file's UTF-8 as is */
    skinedit_model_set_text_raw (model, e, "\xe2\x95\xac");
    ck_assert_str_eq (e->raw[0], "\xe2\x95\xac");
    ck_assert_str_eq (e->shown, "\xe2\x95\xac");
    skinedit_model_set_text_raw (model, e, "|/-\\");
    ck_assert_ptr_null (e->raw[0]);
    ck_assert_str_eq (e->shown, "|/-\\");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_round_trip)
{
    skinedit_entry_t *e;
    skinedit_model_t *m;
    char *path;
    GError *error = NULL;

    skinedit_model_set_text (model, skinedit_model_find (model, "lines", "cross"), "\xe2\x95\xac");
    skinedit_model_set_text (model, skinedit_model_find (model, "core", "spinner_sequence"),
                             "xy\\z;w");
    skinedit_model_set_text (model, skinedit_model_find (model, "widget-panel", "sort-up-char"),
                             "\xe2\x96\xb2");

    path = g_build_filename (test_dir, "chars.ini", (char *) NULL);
    ck_assert_msg (skinedit_model_save_to (model, path, NULL, NULL, &error), "%s",
                   error != NULL ? error->message : "?");

    m = skinedit_model_open_file (path, NULL, NULL);
    ck_assert_ptr_nonnull (m);
    e = skinedit_model_find (m, "lines", "cross");
    ck_assert_str_eq (e->raw[0], "\xe2\x95\xac");
    e = skinedit_model_find (m, "core", "spinner_sequence");
    ck_assert_str_eq (e->raw[0], "xy\\z;w");
    e = skinedit_model_find (m, "widget-panel", "sort-up-char");
    ck_assert_str_eq (e->raw[0], "\xe2\x96\xb2");
    e = skinedit_model_find (m, "lines", "horiz");
    ck_assert_str_eq (e->raw[0], "=");
    skinedit_model_free (m);
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    test_env_init ();

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_read);
    tcase_add_test (tc_core, test_legacy_lines);
    tcase_add_test (tc_core, test_set_text);
    tcase_add_test (tc_core, test_round_trip);

    {
        int rc;

        rc = mctest_run_all (tc_core);
        test_env_deinit ();
        return rc;
    }
}

/* --------------------------------------------------------------------------------------------- */
