/*
   src/panel-plugins/skineditor - saving a skin

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

static const char *skin_text = "# top comment\n"
                               "[skin]\n"
                               "    description = Test skin\n"
                               "\n"
                               "[core]\n"
                               "    # keep me\n"
                               "    _default_ = lightgray;blue\n"
                               "    selected = black;cyan\n"
                               "    weird = magenta\n"
                               "\n"
                               "[dialog]\n"
                               "    _default_ = black;lightgray\n"
                               "    dfocus = black;cyan\n";

static skinedit_model_t *model = NULL;
static char *out_path = NULL;

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
    out_path = g_build_filename (test_dir, "out", "saved.ini", (char *) NULL);
}

static void
teardown (void)
{
    skinedit_model_free (model);
    model = NULL;
    test_vfs_deinit ();
    g_free (out_path);
    out_path = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
save (const char *description)
{
    GError *error = NULL;
    gboolean ret;

    ret = skinedit_model_save_to (model, out_path, "saved", description, &error);
    ck_assert_msg (ret, "%s", error != NULL ? error->message : "?");
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_save_keeps_file)
{
    char *text;
    skinedit_model_t *m;
    const skinedit_entry_t *e;

    skinedit_model_set (model, skinedit_model_find (model, "core", "selected"), SKINEDIT_PART_BG,
                        "color123");
    ck_assert (skinedit_model_dirty (model));

    save ("New description");
    ck_assert (!skinedit_model_dirty (model));
    ck_assert_str_eq (model->name, "saved");
    ck_assert_str_eq (model->path, out_path);
    ck_assert_str_eq (model->description, "New description");

    text = test_read_file (out_path);
    ck_assert_ptr_nonnull (strstr (text, "# top comment"));
    ck_assert_ptr_nonnull (strstr (text, "[core]"));
    ck_assert_ptr_nonnull (strstr (text, "# keep me"));
    ck_assert_ptr_nonnull (strstr (text, "weird=magenta"));
    ck_assert_ptr_nonnull (strstr (text, "selected=black;color123"));
    ck_assert_ptr_nonnull (strstr (text, "description=New description"));
    g_free (text);

    m = skinedit_model_open_file (out_path, NULL, NULL);
    ck_assert_ptr_nonnull (m);
    e = skinedit_model_find (m, "core", "selected");
    ck_assert_str_eq (e->raw[SKINEDIT_PART_BG], "color123");
    e = skinedit_model_find (m, "core", "weird");
    ck_assert_str_eq (e->raw[SKINEDIT_PART_FG], "magenta");
    ck_assert_str_eq (m->description, "New description");
    skinedit_model_free (m);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_inherit_removes_key)
{
    skinedit_entry_t *e;
    char *text;

    e = skinedit_model_find (model, "dialog", "dfocus");
    skinedit_model_set (model, e, SKINEDIT_PART_FG, NULL);
    skinedit_model_set (model, e, SKINEDIT_PART_BG, NULL);
    ck_assert (!mc_config_has_param (model->config, "dialog", "dfocus"));

    /* only the fg left: the value keeps its shape */
    e = skinedit_model_find (model, "dialog", "_default_");
    skinedit_model_set (model, e, SKINEDIT_PART_BG, NULL);

    save (NULL);
    text = test_read_file (out_path);
    ck_assert_ptr_null (strstr (text, "dfocus"));
    ck_assert_ptr_nonnull (strstr (text, "_default_=black\n"));
    ck_assert_ptr_nonnull (strstr (text, "description=Test skin"));
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_flags)
{
    skinedit_entry_t *e;
    char *text;

    e = skinedit_model_find (model, "core", "selected");

    save (NULL);
    text = test_read_file (out_path);
    ck_assert_ptr_null (strstr (text, "256colors"));
    ck_assert_ptr_null (strstr (text, "truecolors"));
    g_free (text);

    skinedit_model_set (model, e, SKINEDIT_PART_FG, "gray5");
    save (NULL);
    text = test_read_file (out_path);
    ck_assert_ptr_nonnull (strstr (text, "256colors=true"));
    ck_assert_ptr_null (strstr (text, "truecolors"));
    g_free (text);

    skinedit_model_set (model, e, SKINEDIT_PART_BG, "#abcdef");
    save (NULL);
    text = test_read_file (out_path);
    ck_assert_ptr_null (strstr (text, "256colors"));
    ck_assert_ptr_nonnull (strstr (text, "truecolors=true"));
    g_free (text);

    /* through an alias as well; the class is lowered by hand, the colors raise it back */
    skinedit_model_set (model, e, SKINEDIT_PART_FG, "white");
    skinedit_model_set (model, e, SKINEDIT_PART_BG, "Deep");
    mc_config_set_string_raw (model->config, "aliases", "Deep", "color17");
    model->colors = SKINEDIT_COLOR_BASIC;
    save (NULL);
    text = test_read_file (out_path);
    ck_assert_ptr_nonnull (strstr (text, "256colors=true"));
    ck_assert_ptr_null (strstr (text, "truecolors"));
    g_free (text);

    /* the declared class stays even when no color needs it */
    skinedit_model_set (model, e, SKINEDIT_PART_BG, "cyan");
    save (NULL);
    text = test_read_file (out_path);
    ck_assert_ptr_nonnull (strstr (text, "256colors=true"));
    g_free (text);
    model->colors = SKINEDIT_COLOR_BASIC;
    save (NULL);
    text = test_read_file (out_path);
    ck_assert_ptr_null (strstr (text, "256colors"));
    g_free (text);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the class the file declares; a save raises it to what the colors need */
START_TEST (test_color_class)
{
    skinedit_entry_t *e = skinedit_model_find (model, "core", "selected");
    skinedit_part_t part = SKINEDIT_PART_FG;

    ck_assert_int_eq (model->colors, SKINEDIT_COLOR_BASIC);
    ck_assert_ptr_null (skinedit_model_over_class (model, SKINEDIT_COLOR_BASIC, &part));

    skinedit_model_set (model, e, SKINEDIT_PART_BG, "gray5");
    ck_assert_ptr_eq (skinedit_model_over_class (model, SKINEDIT_COLOR_BASIC, &part), e);
    ck_assert_int_eq (part, SKINEDIT_PART_BG);
    ck_assert_ptr_null (skinedit_model_over_class (model, SKINEDIT_COLOR_256, &part));
    ck_assert_int_eq (model->colors, SKINEDIT_COLOR_BASIC);

    save (NULL);
    ck_assert_int_eq (model->colors, SKINEDIT_COLOR_256);
    ck_assert_ptr_null (skinedit_model_over_class (model, model->colors, &part));

    /* color0..color15 are the 16 colors under another name */
    skinedit_model_set (model, e, SKINEDIT_PART_BG, "color12");
    ck_assert_ptr_null (skinedit_model_over_class (model, SKINEDIT_COLOR_BASIC, &part));

    /* the entry of the highest class wins, not the first one over */
    {
        skinedit_entry_t *d = skinedit_model_find (model, "dialog", "dfocus");

        skinedit_model_set (model, e, SKINEDIT_PART_BG, "gray5");
        skinedit_model_set (model, d, SKINEDIT_PART_FG, "#abcdef");
        ck_assert_ptr_eq (skinedit_model_over_class (model, SKINEDIT_COLOR_BASIC, &part), d);
        ck_assert_int_eq (part, SKINEDIT_PART_FG);
        ck_assert_int_eq (skinedit_model_class (model), SKINEDIT_COLOR_TRUECOLOR);
        skinedit_model_set (model, d, SKINEDIT_PART_FG, "black");
        ck_assert_int_eq (skinedit_model_class (model), SKINEDIT_COLOR_256);
        skinedit_model_set (model, e, SKINEDIT_PART_BG, "cyan");
    }

    /* a class change alone is a change; reset takes it back */
    save (NULL);
    ck_assert (!skinedit_model_dirty (model));
    model->colors = SKINEDIT_COLOR_TRUECOLOR;
    ck_assert (skinedit_model_dirty (model));
    skinedit_model_reset_all (model);
    ck_assert_int_eq (model->colors, SKINEDIT_COLOR_256);
    ck_assert (!skinedit_model_dirty (model));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bad_name)
{
    GError *error = NULL;

    ck_assert (!skinedit_model_save (model, "../victim", NULL, &error));
    ck_assert_ptr_nonnull (error);
    g_clear_error (&error);
    ck_assert (!skinedit_model_save (model, "", NULL, &error));
    g_clear_error (&error);
    ck_assert (!skinedit_model_name_ok (".ini"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a save that fails leaves the model as it was */
START_TEST (test_failed_save)
{
    GError *error = NULL;
    char *bad;

    bad = g_build_filename (test_dir, "test.ini", "under-a-file", "x.ini", (char *) NULL);
    ck_assert (!skinedit_model_save_to (model, bad, "x", "Changed", &error));
    ck_assert_ptr_nonnull (error);
    g_clear_error (&error);
    ck_assert_str_eq (model->description, "Test skin");
    ck_assert_str_eq (model->name, "test");
    {
        char *v = mc_config_get_string_raw (model->config, "skin", "description", NULL);

        ck_assert_str_eq (v, "Test skin");
        g_free (v);
    }
    g_free (bad);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_config_copy)
{
    mc_config_t *copy;
    char *v;

    /* the file's flag does not match its colors; the copy carries the right one */
    mc_config_set_bool (model->config, "skin", "256colors", TRUE);

    skinedit_model_set (model, skinedit_model_find (model, "core", "selected"), SKINEDIT_PART_FG,
                        "white");
    copy = skinedit_model_config_copy (model);
    ck_assert_ptr_nonnull (copy);
    v = mc_config_get_string_raw (copy, "core", "selected", NULL);
    ck_assert_str_eq (v, "white;cyan");
    g_free (v);
    v = mc_config_get_string_raw (copy, "skin", "description", NULL);
    ck_assert_str_eq (v, "Test skin");
    g_free (v);
    ck_assert (!mc_config_has_param (copy, "skin", "256colors"));

    /* the copy is independent */
    mc_config_set_string_raw (copy, "core", "selected", "red");
    v = mc_config_get_string_raw (model->config, "core", "selected", NULL);
    ck_assert_str_eq (v, "white;cyan");
    g_free (v);
    mc_config_deinit (copy);
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

    tcase_add_test (tc_core, test_save_keeps_file);
    tcase_add_test (tc_core, test_inherit_removes_key);
    tcase_add_test (tc_core, test_flags);
    tcase_add_test (tc_core, test_color_class);
    tcase_add_test (tc_core, test_bad_name);
    tcase_add_test (tc_core, test_failed_save);
    tcase_add_test (tc_core, test_config_copy);

    {
        int rc;

        rc = mctest_run_all (tc_core);
        test_env_deinit ();
        return rc;
    }
}

/* --------------------------------------------------------------------------------------------- */
