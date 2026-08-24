/*
   src/filemanager - tests for the panel quick filter

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

#define TEST_SUITE_NAME "/src/filemanager/panel_quick_filter"

#include "tests/mctest.h"

#include "lib/strutil.h"

#include "src/setup.h"
#include "src/filemanager/dir.h"
#include "src/filemanager/panel.h"
#include "src/filemanager/panel-quick-filter.h"

/* --------------------------------------------------------------------------------------------- */

static WPanel panel;

/* --------------------------------------------------------------------------------------------- */

static void
append_entry (const char *name, gboolean marked)
{
    struct stat st;
    file_entry_t *entry;

    memset (&st, 0, sizeof (st));
    st.st_mode = strcmp (name, "..") == 0 ? S_IFDIR | 0755 : S_IFREG | 0644;
    st.st_size = 10;

    ck_assert (dir_list_append (&panel.dir, name, &st, FALSE, FALSE));
    entry = &panel.dir.list[panel.dir.len - 1];
    entry->f.marked = marked ? 1 : 0;
    if (marked)
    {
        panel.marked++;
        panel.total += (uintmax_t) st.st_size;
    }
}

/* --------------------------------------------------------------------------------------------- */

static file_entry_t *
find_entry (dir_list *list, const char *name)
{
    int i;

    for (i = 0; i < list->len; i++)
        if (strcmp (list->list[i].fname->str, name) == 0)
            return &list->list[i];

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
set_pattern (const char *pattern)
{
    g_string_assign (panel.quick_search.buffer, pattern);
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    memset (&panel, 0, sizeof (panel));
    WIDGET (&panel)->rect.lines = 20;
    panel.list_cols = 1;
    panel.current = 0;
    panel.quick_search.buffer = g_string_new ("");
    panel.quick_search.prev_buffer = g_string_new ("");
    panels_options.qsearch_mode = QSEARCH_CASE_SENSITIVE;
    str_init_strings (NULL);

    append_entry ("..", FALSE);
    append_entry ("contrib", FALSE);
    append_entry ("config.h", FALSE);
    append_entry ("font.c", FALSE);
    append_entry ("README", FALSE);
}

static void
teardown (void)
{
    dir_list_free_list (&panel.dir);
    if (panel.quick_search.source.list != NULL)
        dir_list_free_list (&panel.quick_search.source);
    g_string_free (panel.quick_search.buffer, TRUE);
    g_string_free (panel.quick_search.prev_buffer, TRUE);
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_contains_matching_keeps_parent_and_moves_to_next_match)
{
    panel.current = 2;  // config.h
    set_pattern ("ont");

    panel_quick_filter_begin (&panel);
    ck_assert (panel_quick_filter_apply (&panel));

    ck_assert_int_eq (panel.dir.len, 3);
    ck_assert_str_eq (panel.dir.list[0].fname->str, "..");
    ck_assert_str_eq (panel.dir.list[1].fname->str, "contrib");
    ck_assert_str_eq (panel.dir.list[2].fname->str, "font.c");
    ck_assert_str_eq (panel_current_entry (&panel)->fname->str, "font.c");
    ck_assert_int_eq (panel.quick_search.source.len, 5);
}
END_TEST

START_TEST (test_no_match_rejects_pattern_without_replacing_list)
{
    file_entry_t *original_list = panel.dir.list;

    set_pattern ("missing");
    panel_quick_filter_begin (&panel);

    ck_assert (!panel_quick_filter_apply (&panel));
    ck_assert_ptr_eq (panel.dir.list, original_list);
    ck_assert_int_eq (panel.dir.len, 5);
    ck_assert_ptr_null (panel.quick_search.source.list);
}
END_TEST

START_TEST (test_empty_pattern_restores_full_list_but_keeps_filter_mode)
{
    set_pattern ("ont");
    panel_quick_filter_begin (&panel);
    ck_assert (panel_quick_filter_apply (&panel));

    set_pattern ("");
    ck_assert (panel_quick_filter_apply (&panel));

    ck_assert (panel.quick_search.filtering);
    ck_assert_ptr_null (panel.quick_search.source.list);
    ck_assert_int_eq (panel.dir.len, 5);
    ck_assert_ptr_nonnull (find_entry (&panel.dir, "README"));
}
END_TEST

START_TEST (test_broader_pattern_restores_matches_from_source)
{
    set_pattern ("font");
    panel_quick_filter_begin (&panel);
    ck_assert (panel_quick_filter_apply (&panel));
    ck_assert_int_eq (panel.dir.len, 2);

    set_pattern ("ont");
    ck_assert (panel_quick_filter_apply (&panel));

    ck_assert_int_eq (panel.dir.len, 3);
    ck_assert_ptr_nonnull (find_entry (&panel.dir, "contrib"));
    ck_assert_ptr_nonnull (find_entry (&panel.dir, "font.c"));
}
END_TEST

START_TEST (test_no_match_keeps_previous_filtered_view)
{
    file_entry_t *filtered_list;

    set_pattern ("ont");
    panel_quick_filter_begin (&panel);
    ck_assert (panel_quick_filter_apply (&panel));
    filtered_list = panel.dir.list;

    set_pattern ("missing");
    ck_assert (!panel_quick_filter_apply (&panel));

    ck_assert_ptr_eq (panel.dir.list, filtered_list);
    ck_assert_int_eq (panel.dir.len, 3);
    ck_assert_int_eq (panel.quick_search.source.len, 5);
    ck_assert_ptr_nonnull (find_entry (&panel.dir, "contrib"));
    ck_assert_ptr_nonnull (find_entry (&panel.dir, "font.c"));
}
END_TEST

START_TEST (test_restore_disables_filter_and_preserves_current_file)
{
    panel.current = 3;  // font.c
    set_pattern ("ont");
    panel_quick_filter_begin (&panel);
    ck_assert (panel_quick_filter_apply (&panel));

    panel_quick_filter_restore (&panel);

    ck_assert (!panel.quick_search.filtering);
    ck_assert_ptr_null (panel.quick_search.source.list);
    ck_assert_int_eq (panel.dir.len, 5);
    ck_assert_str_eq (panel_current_entry (&panel)->fname->str, "font.c");
}
END_TEST

START_TEST (test_mark_sync_uses_names_not_list_order)
{
    file_entry_t tmp;

    set_pattern ("ont");
    panel_quick_filter_begin (&panel);
    ck_assert (panel_quick_filter_apply (&panel));

    find_entry (&panel.dir, "font.c")->f.marked = 1;

    tmp = panel.quick_search.source.list[1];
    panel.quick_search.source.list[1] = panel.quick_search.source.list[3];
    panel.quick_search.source.list[3] = tmp;
    panel_quick_filter_sync_marks (&panel);

    ck_assert_int_eq (find_entry (&panel.quick_search.source, "font.c")->f.marked, 1);
    ck_assert_int_eq (find_entry (&panel.quick_search.source, "contrib")->f.marked, 0);
}
END_TEST

START_TEST (test_marked_count_reports_filtered_out_marks)
{
    int visible;
    int hidden;

    find_entry (&panel.dir, "contrib")->f.marked = 1;
    find_entry (&panel.dir, "README")->f.marked = 1;
    panel.marked = 2;
    panel.total = 20;

    set_pattern ("font");
    panel_quick_filter_begin (&panel);
    ck_assert (panel_quick_filter_apply (&panel));
    panel_quick_filter_get_marked_count (&panel, &visible, &hidden);

    ck_assert_int_eq (visible, 0);
    ck_assert_int_eq (hidden, 2);

    find_entry (&panel.dir, "font.c")->f.marked = 1;
    panel.marked = 1;
    panel_quick_filter_sync_marks (&panel);
    panel_quick_filter_get_marked_count (&panel, &visible, &hidden);

    ck_assert_int_eq (visible, 1);
    ck_assert_int_eq (hidden, 2);

    panel_quick_filter_restore (&panel);
    ck_assert_int_eq (panel.marked, 3);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_contains_matching_keeps_parent_and_moves_to_next_match);
    tcase_add_test (tc_core, test_no_match_rejects_pattern_without_replacing_list);
    tcase_add_test (tc_core, test_empty_pattern_restores_full_list_but_keeps_filter_mode);
    tcase_add_test (tc_core, test_broader_pattern_restores_matches_from_source);
    tcase_add_test (tc_core, test_no_match_keeps_previous_filtered_view);
    tcase_add_test (tc_core, test_restore_disables_filter_and_preserves_current_file);
    tcase_add_test (tc_core, test_mark_sync_uses_names_not_list_order);
    tcase_add_test (tc_core, test_marked_count_reports_filtered_out_marks);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
