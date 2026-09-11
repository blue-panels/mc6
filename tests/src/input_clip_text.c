/*
   tests/src/input_clip_text.c -- unit tests for the clipfile as one input line

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

#define TEST_SUITE_NAME "/src/input_clip_text"

#include "tests/mctest.h"

#include <unistd.h>

#include "lib/event.h"
#include "lib/fileloc.h"
#include "lib/mcconfig.h"
#include "lib/widget.h"

#include "src/clipboard.h"

/*** file scope variables ************************************************************************/

// What the fake clipfile reader hands out.
static const char *clip_data = NULL;
static size_t clip_len = 0;
static gboolean clip_ret = TRUE;

static char *data_home = NULL;

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static gboolean
fake_text_from_file (const gchar *event_group_name, const gchar *event_name, gpointer init_data,
                     gpointer data)
{
    ev_clipboard_text_from_file_t *ev = (ev_clipboard_text_from_file_t *) data;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    *(ev->text) = clip_ret ? g_memdup2 (clip_data, clip_len + 1) : NULL;
    ev->len = clip_ret ? clip_len : 0;
    ev->ret = clip_ret;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
as_line (const char *data, size_t len)
{
    clip_data = data;
    clip_len = len;
    clip_ret = TRUE;
    return input_clip_text ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    GError *error = NULL;

    // The real reader looks for the clipfile under XDG_DATA_HOME, which is a fresh directory.
    data_home = g_dir_make_tmp ("mc-clip-XXXXXX", NULL);
    g_setenv ("XDG_DATA_HOME", data_home, TRUE);

    ck_assert_msg (mc_event_init (&error), "Failed to initialize event transport: %s",
                   error != NULL ? error->message : "unknown");
    mc_event_add (MCEVENT_GROUP_CORE, "clipboard_text_from_file", fake_text_from_file, NULL, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    char *dir;

    mc_event_deinit (NULL);

    dir = g_build_filename (data_home, MC_USERCONF_DIR, EDIT_HOME_CLIP_FILE, (char *) NULL);
    unlink (dir);
    g_free (dir);
    dir = g_build_filename (data_home, MC_USERCONF_DIR, EDIT_HOME_DIR, (char *) NULL);
    rmdir (dir);
    g_free (dir);
    dir = g_build_filename (data_home, MC_USERCONF_DIR, (char *) NULL);
    rmdir (dir);
    g_free (dir);
    rmdir (data_home);
    g_free (data_home);
    data_home = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/*** tests ***************************************************************************************/
/* --------------------------------------------------------------------------------------------- */

START_TEST (test_line_breaks_become_spaces)
{
    char *s = as_line ("a\nb\r\nc\n", 7);

    mctest_assert_str_eq (s, "a b c");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_lone_cr_is_a_break)
{
    char *s = as_line ("foo\rbar", 7);

    mctest_assert_str_eq (s, "foo bar");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_a_nul_byte_does_not_cut_the_text)
{
    char *s = as_line ("foo\0bar", 7);

    mctest_assert_str_eq (s, "foo bar");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_control_characters_become_spaces)
{
    char *s = as_line ("x\ty\003z\177", 6);

    mctest_assert_str_eq (s, "x y z ");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_only_line_breaks_is_nothing)
{
    char *s = as_line ("\r\n\n", 3);

    mctest_assert_null (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_no_clipfile_is_nothing)
{
    clip_ret = FALSE;
    mctest_assert_null (input_clip_text ());
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_the_reader_keeps_the_length)
{
    char *path, *dir;
    char *text = NULL;
    ev_clipboard_text_from_file_t ev = { &text, FALSE, 0 };

    path = mc_config_get_full_path (EDIT_HOME_CLIP_FILE);
    dir = g_path_get_dirname (path);
    ck_assert_int_eq (g_mkdir_with_parents (dir, 0700), 0);
    ck_assert (g_file_set_contents (path, "one\0two\n", 8, NULL));

    mctest_assert_true (
        clipboard_text_from_file (MCEVENT_GROUP_CORE, "clipboard_text_from_file", NULL, &ev));
    mctest_assert_true (ev.ret);
    ck_assert_int_eq (ev.len, 8);
    ck_assert_int_eq (memcmp (text, "one\0two\n", 8), 0);

    g_free (text);
    g_free (dir);
    g_free (path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_line_breaks_become_spaces);
    tcase_add_test (tc_core, test_a_lone_cr_is_a_break);
    tcase_add_test (tc_core, test_a_nul_byte_does_not_cut_the_text);
    tcase_add_test (tc_core, test_control_characters_become_spaces);
    tcase_add_test (tc_core, test_only_line_breaks_is_nothing);
    tcase_add_test (tc_core, test_no_clipfile_is_nothing);
    tcase_add_test (tc_core, test_the_reader_keeps_the_length);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
