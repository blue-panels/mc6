/*
   src/panel-plugins/skineditor - fallback and alias resolution of the model

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
                               "[aliases]\n"
                               "    Main = #123456\n"
                               "    MainFg = Main\n"
                               "    Blank = #abcdef \n"
                               "    Loop1 = Loop2\n"
                               "    Loop2 = Loop1\n"
                               "\n"
                               "[core]\n"
                               "    _default_ = lightgray;blue\n"
                               "    selected = black;cyan\n"
                               "    marked = yellow\n"
                               "    header = MainFg;;bold\n"
                               "    mykey = red\n"
                               "\n"
                               "[dialog]\n"
                               "    _default_ = black;lightgray\n"
                               "    dfocus = ;cyan\n"
                               "    dhotnormal = base;base\n"
                               "\n"
                               "[error]\n"
                               "    errdfocus = white\n"
                               "    errdtitle = red;blue;bold;underline\n"
                               "\n"
                               "[myplugin]\n"
                               "    thing = green;black\n";

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

START_TEST (test_open)
{
    ck_assert_str_eq (model->name, "test");
    ck_assert_str_eq (model->description, "Test skin");
    ck_assert (model->system);
    ck_assert (!skinedit_model_dirty (model));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_set_parts)
{
    const skinedit_entry_t *e;

    e = skinedit_model_find (model, "core", "_default_");
    ck_assert_ptr_nonnull (e);
    ck_assert (e->known);
    ck_assert_int_eq (e->kind, SKINEDIT_ENTRY_COLOR);
    ck_assert_str_eq (e->raw[SKINEDIT_PART_FG], "lightgray");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "lightgray");
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_SET);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "blue");
    ck_assert_int_eq (e->src[SKINEDIT_PART_BG], SKINEDIT_SRC_SET);
    ck_assert_ptr_null (e->raw[SKINEDIT_PART_ATTRS]);
    ck_assert_ptr_null (e->effective[SKINEDIT_PART_ATTRS]);
    ck_assert_int_eq (e->src[SKINEDIT_PART_ATTRS], SKINEDIT_SRC_TERMINAL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_core_fallback)
{
    const skinedit_entry_t *e;

    e = skinedit_model_find (model, "core", "marked");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "yellow");
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_SET);
    ck_assert_ptr_null (e->raw[SKINEDIT_PART_BG]);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "blue");
    ck_assert_int_eq (e->src[SKINEDIT_PART_BG], SKINEDIT_SRC_CORE_DEFAULT);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_group_fallback)
{
    const skinedit_entry_t *e;

    e = skinedit_model_find (model, "dialog", "dfocus");
    ck_assert_ptr_null (e->raw[SKINEDIT_PART_FG]);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "black");
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_GROUP_DEFAULT);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "cyan");
    ck_assert_int_eq (e->src[SKINEDIT_PART_BG], SKINEDIT_SRC_SET);

    /* a group without _default_ falls through to core */
    e = skinedit_model_find (model, "error", "errdfocus");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "white");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "blue");
    ck_assert_int_eq (e->src[SKINEDIT_PART_BG], SKINEDIT_SRC_CORE_DEFAULT);

    /* the absent _default_ of that group */
    e = skinedit_model_find (model, "error", "_default_");
    ck_assert_ptr_nonnull (e);
    ck_assert_ptr_null (e->raw[SKINEDIT_PART_FG]);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "lightgray");
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_CORE_DEFAULT);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_base)
{
    const skinedit_entry_t *e;

    e = skinedit_model_find (model, "dialog", "dhotnormal");
    ck_assert_str_eq (e->raw[SKINEDIT_PART_FG], "base");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "lightgray");
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_CORE_DEFAULT);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "blue");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_alias)
{
    skinedit_entry_t *e;

    e = skinedit_model_find (model, "core", "header");
    ck_assert_str_eq (e->raw[SKINEDIT_PART_FG], "MainFg");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "#123456");
    ck_assert_str_eq (e->alias[SKINEDIT_PART_FG], "MainFg");
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_SET);
    ck_assert_ptr_null (e->alias[SKINEDIT_PART_BG]);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_ATTRS], "bold");

    /* a loop resolves to the name as written */
    skinedit_model_set (model, e, SKINEDIT_PART_FG, "Loop1");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "Loop1");
    ck_assert_ptr_null (e->alias[SKINEDIT_PART_FG]);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the engine keeps an alias target as written, a trailing blank included */
START_TEST (test_alias_blank)
{
    skinedit_entry_t *e;

    e = skinedit_model_find (model, "core", "selected");
    skinedit_model_set (model, e, SKINEDIT_PART_FG, "Blank");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "#abcdef ");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a fourth list item is dropped, as the engine does */
START_TEST (test_extra_items)
{
    const skinedit_entry_t *e;

    e = skinedit_model_find (model, "error", "errdtitle");
    ck_assert_str_eq (e->raw[SKINEDIT_PART_ATTRS], "bold");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* keys the engine fills in when absent: the model shows the same */
START_TEST (test_engine_fallbacks)
{
    const skinedit_entry_t *e;

    e = skinedit_model_find (model, "core", "permread");
    ck_assert_ptr_null (e->raw[SKINEDIT_PART_FG]);
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_FALLBACK);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "yellow");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "blue");
    ck_assert_ptr_null (e->effective[SKINEDIT_PART_ATTRS]);

    /* [viewer] is absent here, so the viewer default is core's */
    e = skinedit_model_find (model, "mctree", "marker");
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_FALLBACK);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "lightgray");

    /* naming any part of the key switches the fallback off */
    skinedit_model_set (model, skinedit_model_find (model, "core", "permread"), SKINEDIT_PART_BG,
                        "black");
    e = skinedit_model_find (model, "core", "permread");
    ck_assert_int_eq (e->src[SKINEDIT_PART_FG], SKINEDIT_SRC_CORE_DEFAULT);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "lightgray");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_broken_file)
{
    char *path;
    skinedit_model_t *m;
    GError *error = NULL;

    path = test_write_skin ("broken.ini", "[core]\n_default_ = a;b\nno equals sign here\n");
    m = skinedit_model_open_file (path, NULL, &error);
    ck_assert_ptr_null (m);
    ck_assert_ptr_nonnull (error);
    g_clear_error (&error);
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_set_and_inherit)
{
    skinedit_entry_t *e, *d;

    e = skinedit_model_find (model, "core", "selected");
    d = skinedit_model_find (model, "core", "_default_");

    skinedit_model_set (model, e, SKINEDIT_PART_BG, NULL);
    ck_assert_ptr_null (e->raw[SKINEDIT_PART_BG]);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "blue");
    ck_assert_int_eq (e->src[SKINEDIT_PART_BG], SKINEDIT_SRC_CORE_DEFAULT);
    ck_assert (skinedit_entry_changed (e));
    ck_assert (skinedit_model_dirty (model));

    /* changing the default changes what inherits from it */
    skinedit_model_set (model, d, SKINEDIT_PART_BG, " green ");
    ck_assert_str_eq (d->raw[SKINEDIT_PART_BG], "green");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "green");

    skinedit_model_reset (model, e);
    ck_assert_str_eq (e->raw[SKINEDIT_PART_BG], "cyan");
    ck_assert (!skinedit_entry_changed (e));
    ck_assert (skinedit_model_dirty (model));

    skinedit_model_reset_all (model);
    ck_assert (!skinedit_model_dirty (model));
    ck_assert_str_eq (d->raw[SKINEDIT_PART_BG], "blue");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the engine refuses a skin without [core] _default_: inheriting it means the terminal colors */
START_TEST (test_core_default_stays)
{
    skinedit_entry_t *e;
    int i;

    e = skinedit_model_find (model, "core", "_default_");
    for (i = 0; i < SKINEDIT_PARTS; i++)
        skinedit_model_set (model, e, i, NULL);
    ck_assert_str_eq (e->raw[SKINEDIT_PART_FG], "default");
    ck_assert_str_eq (e->raw[SKINEDIT_PART_BG], "default");
    ck_assert_ptr_null (e->raw[SKINEDIT_PART_ATTRS]);
    ck_assert (mc_config_has_param (model->config, "core", "_default_"));

    /* a key that inherits from it now gets the terminal color */
    e = skinedit_model_find (model, "core", "marked");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "default");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_needs)
{
    gboolean n256, ntrue;

    skinedit_model_needs (model, &n256, &ntrue);
    ck_assert (!n256);
    ck_assert (ntrue); /* header goes through MainFg to #123456 */

    skinedit_model_set (model, skinedit_model_find (model, "core", "header"), SKINEDIT_PART_FG,
                        "color200");
    skinedit_model_needs (model, &n256, &ntrue);
    ck_assert (n256);
    ck_assert (!ntrue);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_unknown_keys)
{
    const skinedit_entry_t *e;
    const skinedit_section_t *s;

    e = skinedit_model_find (model, "core", "mykey");
    ck_assert_ptr_nonnull (e);
    ck_assert (!e->known);
    ck_assert_str_eq (e->label, "mykey");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_FG], "red");
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "blue");

    s = skinedit_model_find_section (model, "core");
    ck_assert_ptr_nonnull (s);
    ck_assert_ptr_eq (g_ptr_array_index (s->entries, s->entries->len - 1), e);

    s = skinedit_model_find_section (model, "myplugin");
    ck_assert_ptr_nonnull (s);
    ck_assert_str_eq (s->label, "myplugin");
    ck_assert_int_eq (s->entries->len, 1);
    e = g_ptr_array_index (s->entries, 0);
    ck_assert_str_eq (e->key, "thing");
    ck_assert_int_eq (e->kind, SKINEDIT_ENTRY_COLOR);
    ck_assert_str_eq (e->effective[SKINEDIT_PART_BG], "black");

    /* the sections of the table come first, in table order */
    s = g_ptr_array_index (model->sections, 0);
    ck_assert_str_eq (s->group, "core");
    s = g_ptr_array_index (model->sections, model->sections->len - 1);
    ck_assert_str_eq (s->group, "myplugin");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_open_by_name)
{
    char *dir, *path;
    skinedit_model_t *m;
    GError *error = NULL;

    m = skinedit_model_open ("nosuchskin", &error);
    ck_assert_ptr_null (m);
    ck_assert_ptr_nonnull (error);
    g_clear_error (&error);

    dir = g_build_filename (test_dir, "mc6", "skins", (char *) NULL);
    ck_assert_int_eq (g_mkdir_with_parents (dir, 0700), 0);
    path = g_build_filename (dir, "mine.ini", (char *) NULL);
    ck_assert (g_file_set_contents (path, skin_text, -1, NULL));

    m = skinedit_model_open ("mine", &error);
    ck_assert_msg (m != NULL, "%s", error != NULL ? error->message : "?");
    ck_assert_str_eq (m->name, "mine");
    ck_assert_str_eq (m->path, path);
    ck_assert (!m->system);
    skinedit_model_free (m);

    g_free (path);
    g_free (dir);
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

    tcase_add_test (tc_core, test_open);
    tcase_add_test (tc_core, test_set_parts);
    tcase_add_test (tc_core, test_core_fallback);
    tcase_add_test (tc_core, test_group_fallback);
    tcase_add_test (tc_core, test_base);
    tcase_add_test (tc_core, test_alias);
    tcase_add_test (tc_core, test_alias_blank);
    tcase_add_test (tc_core, test_extra_items);
    tcase_add_test (tc_core, test_engine_fallbacks);
    tcase_add_test (tc_core, test_broken_file);
    tcase_add_test (tc_core, test_set_and_inherit);
    tcase_add_test (tc_core, test_core_default_stays);
    tcase_add_test (tc_core, test_needs);
    tcase_add_test (tc_core, test_unknown_keys);
    tcase_add_test (tc_core, test_open_by_name);

    {
        int rc;

        rc = mctest_run_all (tc_core);
        test_env_deinit ();
        return rc;
    }
}

/* --------------------------------------------------------------------------------------------- */
