/*
   src/filemanager - tests for the hidden names in a plugin panel

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

#define TEST_SUITE_NAME "/src/filemanager"

#include "tests/mctest.h"

#include <string.h>

#include "lib/panel-plugin.h"
#include "src/setup.h"
#include "src/filemanager/panel.h"

/* panel_plugin_get_items() is file-local; the file is compiled in. */
#include "src/filemanager/panel_plugin_ui.c"

/* --------------------------------------------------------------------------------------------- */

static gboolean listing_is_files;
static int plugin_data;

static mc_pp_result_t
mock_get_items (void *data, void *list)
{
    (void) data;

    mc_pp_add_entry (list, ".hidden", S_IFREG | 0644, 0, 0);
    mc_pp_add_entry (list, ".dir", S_IFDIR | 0755, 0, 0);
    mc_pp_add_entry (list, "shown", S_IFREG | 0644, 0, 0);
    mc_pp_add_entry (list, "backup~", S_IFREG | 0644, 0, 0);
    return MC_PPR_OK;
}

static gboolean
mock_is_file_listing (void *data)
{
    (void) data;

    return listing_is_files;
}

static const mc_panel_plugin_t files_plugin = {
    .name = "files",
    .get_items = mock_get_items,
    .is_file_listing = mock_is_file_listing,
};

static const mc_panel_plugin_t plain_plugin = {
    .name = "plain",
    .get_items = mock_get_items,
};

/* --------------------------------------------------------------------------------------------- */

static WPanel *panel;

static void
setup (void)
{
    panel = g_new0 (WPanel, 1);
    panel->is_plugin_panel = TRUE;
    panel->plugin = &files_plugin;
    panel->plugin_data = &plugin_data;
    dir_list_init (&panel->dir);

    listing_is_files = TRUE;
    panels_options.show_dot_files = FALSE;
    panels_options.show_backups = FALSE;
}

static void
teardown (void)
{
    dir_list_free_list (&panel->dir);
    g_free (panel);
}

/* The names in the list, in order, joined with a space. */
static char *
names (void)
{
    GString *s = g_string_new ("");
    int i;

    for (i = 0; i < panel->dir.len; i++)
    {
        if (i > 0)
            g_string_append_c (s, ' ');
        g_string_append (s, panel->dir.list[i].fname->str);
    }

    return g_string_free (s, FALSE);
}

static void
check_names (const char *expected)
{
    char *got = names ();

    ck_assert_str_eq (got, expected);
    g_free (got);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_hidden_names_are_dropped)
{
    panel_plugin_get_items (panel);

    check_names (".. shown");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_dot_names_shown_backups_hidden)
{
    panels_options.show_dot_files = TRUE;

    panel_plugin_get_items (panel);

    check_names (".. .hidden .dir shown");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_everything_shown)
{
    panels_options.show_dot_files = TRUE;
    panels_options.show_backups = TRUE;

    panel_plugin_get_items (panel);

    check_names (".. .hidden .dir shown backup~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* An address book is not a directory: its labels stay whatever they are. */
START_TEST (test_other_listing_is_not_filtered)
{
    listing_is_files = FALSE;

    panel_plugin_get_items (panel);

    check_names (".. .hidden .dir shown backup~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_plugin_without_the_hook_is_not_filtered)
{
    panel->plugin = &plain_plugin;

    panel_plugin_get_items (panel);

    check_names (".. .hidden .dir shown backup~");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_hidden_names_are_dropped);
    tcase_add_test (tc_core, test_dot_names_shown_backups_hidden);
    tcase_add_test (tc_core, test_everything_shown);
    tcase_add_test (tc_core, test_other_listing_is_not_filtered);
    tcase_add_test (tc_core, test_plugin_without_the_hook_is_not_filtered);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
