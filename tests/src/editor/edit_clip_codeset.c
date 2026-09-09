/*
   tests/src/editor - unit tests for the codeset of the clipfile

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

#define TEST_SUITE_NAME "/src/editor"

#include "tests/mctest.h"

#include <unistd.h>

#include "lib/charsets.h"
#include "src/vfs/local/local.c"
#include "src/selcodepage.h"
#include "src/clipboard.h"

#include "src/editor/editwidget.h"

/* the fixture mc.charsets: 0 is KOI8-R, 1 is UTF-8 */
#define CP_KOI8R 0
#define CP_UTF8  1

static WDialog owner;
static WEdit *test_edit;

/* --------------------------------------------------------------------------------------------- */

static void
set_codeset (const int cp)
{
    mc_global.source_codepage = cp;
    cp_source = get_codepage_id (cp);
    do_set_codepage (cp);
    edit_set_codeset (test_edit);
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    WRect r;

    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();

    mc_global.sysconfig_dir = (char *) TEST_SHARE_DIR;
    load_codepages_list ();

    edit_options.filesize_threshold = (char *) "64M";

    rect_init (&r, 0, 0, 24, 80);
    test_edit = edit_init (NULL, &r, NULL);
    memset (&owner, 0, sizeof (owner));
    group_add_widget (&owner.group, WIDGET (test_edit));

    mc_global.display_codepage = CP_UTF8;
    cp_display = get_codepage_id (CP_UTF8);
    set_codeset (CP_UTF8);
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    edit_clean (test_edit);
    group_remove_widget (test_edit);
    g_free (test_edit);

    free_codepages_list ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

static char *
make_clip_path (void)
{
    char *path;
    int fd;

    path = g_build_filename (g_get_tmp_dir (), "mc-test-clip-XXXXXX", NULL);
    fd = g_mkstemp (path);
    if (fd >= 0)
        close (fd);

    return path;
}

/* --------------------------------------------------------------------------------------------- */

static void
load_text (const char *text)
{
    for (; *text != '\0'; text++)
    {
        edit_buffer_insert (&test_edit->buffer, *text);
        if (*text == '\n')
            test_edit->buffer.lines++;
    }
}

/* --------------------------------------------------------------------------------------------- */

static GString *
buffer_text (void)
{
    GString *s = g_string_new ("");

    for (off_t i = 0; i < test_edit->buffer.size; i++)
        g_string_append_c (s, (gchar) edit_buffer_get_byte (&test_edit->buffer, i));

    return s;
}

/* --------------------------------------------------------------------------------------------- */

static GString *
file_text (const char *path)
{
    gchar *data = NULL;
    gsize len = 0;
    GString *s;

    g_file_get_contents (path, &data, &len, NULL);
    s = g_string_new_len (data, len);
    g_free (data);

    return s;
}

/* --------------------------------------------------------------------------------------------- */

/* a clipfile as the editor leaves it: the text and the info file with its digest */
static void
write_clip (const char *path, const char *data, size_t len, gboolean vertical, const char *codeset)
{
    char *sum;

    g_file_set_contents (path, data, len, NULL);
    sum = g_compute_checksum_for_data (CLIP_DIGEST_TYPE, (const guchar *) data, len);
    clipboard_info_write (path, sum, vertical, codeset);
    g_free (sum);
}

/* --------------------------------------------------------------------------------------------- */

static GString *
to_koi8r (const char *utf8)
{
    gchar *s;
    gsize len = 0;
    GString *r;

    s = g_convert (utf8, -1, "KOI8-R", "UTF-8", NULL, &len, NULL);
    r = g_string_new_len (s, len);
    g_free (s);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* copy in a UTF-8 editor, Alt-E to KOI8-R, paste: the text is recoded, not the bytes */
START_TEST (test_paste_recodes_to_editor_codeset)
{
    char *clip = make_clip_path ();
    vfs_path_t *vp;
    GString *actual, *expected, *saved;

    load_text ("привет\n");
    mctest_assert_true (edit_save_clip_block (test_edit, clip, 0, test_edit->buffer.size));

    saved = file_text (clip);
    mctest_assert_str_eq (saved->str, "привет\n");
    g_string_free (saved, TRUE);
    {
        char digest[CLIP_DIGEST_LEN + 1];
        char codeset[CLIP_CODESET_MAX + 1];
        gboolean vertical;

        mctest_assert_true (clipboard_info_read (clip, digest, &vertical, codeset));
        mctest_assert_false (vertical);
        mctest_assert_str_eq (codeset, "UTF-8");
    }

    set_codeset (CP_KOI8R);

    edit_cursor_move (test_edit, test_edit->buffer.size - test_edit->buffer.curs1);
    vp = vfs_path_from_str (clip);
    ck_assert (edit_insert_file (test_edit, vp) >= 0);
    vfs_path_free (vp, TRUE);

    actual = buffer_text ();
    expected = g_string_new ("привет\n");
    {
        GString *k = to_koi8r ("привет\n");

        ck_assert (k->len == 7);
        g_string_append_len (expected, k->str, k->len);
        g_string_free (k, TRUE);
    }
    ck_assert (actual->len == expected->len);
    ck_assert (memcmp (actual->str, expected->str, actual->len) == 0);

    g_string_free (actual, TRUE);
    g_string_free (expected, TRUE);
    clipboard_info_drop (clip);
    unlink (clip);
    g_free (clip);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* the way back: a KOI8-R clip pasted into a UTF-8 editor */
START_TEST (test_paste_koi8r_clip_into_utf8_editor)
{
    char *clip = make_clip_path ();
    vfs_path_t *vp;
    GString *actual, *body;

    body = to_koi8r ("мир\n");
    write_clip (clip, body->str, body->len, FALSE, "KOI8-R");
    g_string_free (body, TRUE);

    vp = vfs_path_from_str (clip);
    ck_assert (edit_insert_file (test_edit, vp) >= 0);
    vfs_path_free (vp, TRUE);

    actual = buffer_text ();
    mctest_assert_str_eq (actual->str, "мир\n");

    g_string_free (actual, TRUE);
    clipboard_info_drop (clip);
    unlink (clip);
    g_free (clip);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* a character the target codeset lacks becomes '?', the rest of the line survives */
START_TEST (test_paste_unrepresentable_char)
{
    char *clip = make_clip_path ();
    vfs_path_t *vp;
    GString *actual;

    write_clip (clip, "a€b\n", 6, FALSE, "UTF-8");

    set_codeset (CP_KOI8R);

    vp = vfs_path_from_str (clip);
    ck_assert (edit_insert_file (test_edit, vp) >= 0);
    vfs_path_free (vp, TRUE);

    actual = buffer_text ();
    mctest_assert_str_eq (actual->str, "a?b\n");

    g_string_free (actual, TRUE);
    clipboard_info_drop (clip);
    unlink (clip);
    g_free (clip);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* the same codeset on both sides: the bytes are taken as they are */
START_TEST (test_paste_same_codeset_keeps_bytes)
{
    char *clip = make_clip_path ();
    vfs_path_t *vp;
    GString *actual;

    // invalid UTF-8 inside: no recoding must touch it
    write_clip (clip, "ab\xff\xfe\n", 5, FALSE, "UTF-8");

    vp = vfs_path_from_str (clip);
    ck_assert (edit_insert_file (test_edit, vp) >= 0);
    vfs_path_free (vp, TRUE);

    actual = buffer_text ();
    ck_assert (actual->len == 5);
    ck_assert (memcmp (actual->str, "ab\xff\xfe\n", 5) == 0);

    g_string_free (actual, TRUE);
    clipboard_info_drop (clip);
    unlink (clip);
    g_free (clip);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* a vertical block carries the codeset too and is still inserted as a column */
START_TEST (test_paste_vertical_block_recoded)
{
    char *clip = make_clip_path ();
    vfs_path_t *vp;
    GString *actual, *expected;

    write_clip (clip, "п\nр\n", 6, TRUE, "UTF-8");

    set_codeset (CP_KOI8R);
    load_text ("xx\nyy\n");
    edit_cursor_move (test_edit, -test_edit->buffer.curs1);

    vp = vfs_path_from_str (clip);
    ck_assert (edit_insert_file (test_edit, vp) >= 0);
    vfs_path_free (vp, TRUE);

    actual = buffer_text ();
    expected = to_koi8r ("пxx\nрyy\n");
    ck_assert (actual->len == expected->len);
    ck_assert (memcmp (actual->str, expected->str, actual->len) == 0);

    g_string_free (actual, TRUE);
    g_string_free (expected, TRUE);
    clipboard_info_drop (clip);
    unlink (clip);
    g_free (clip);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* a clip without an info file (the input line, xclip) is pasted byte by byte as before */
START_TEST (test_paste_plain_clip)
{
    char *clip = make_clip_path ();
    vfs_path_t *vp;
    GString *actual;

    g_file_set_contents (clip, "plain\n", 6, NULL);

    set_codeset (CP_KOI8R);

    vp = vfs_path_from_str (clip);
    ck_assert (edit_insert_file (test_edit, vp) >= 0);
    vfs_path_free (vp, TRUE);

    actual = buffer_text ();
    mctest_assert_str_eq (actual->str, "plain\n");

    g_string_free (actual, TRUE);
    unlink (clip);
    g_free (clip);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
/* the info file of an older copy does not apply to a clip written by someone else */
START_TEST (test_paste_stale_info)
{
    char *clip = make_clip_path ();
    vfs_path_t *vp;
    GString *actual;

    write_clip (clip, "old\n", 4, TRUE, "KOI8-R");
    g_file_set_contents (clip, "new text\n", 9, NULL);

    vp = vfs_path_from_str (clip);
    ck_assert (edit_insert_file (test_edit, vp) >= 0);
    vfs_path_free (vp, TRUE);

    // neither a column nor recoded
    actual = buffer_text ();
    mctest_assert_str_eq (actual->str, "new text\n");
    ck_assert_int_eq (test_edit->column_highlight, 0);

    g_string_free (actual, TRUE);
    clipboard_info_drop (clip);
    unlink (clip);
    g_free (clip);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_paste_recodes_to_editor_codeset);
    tcase_add_test (tc_core, test_paste_koi8r_clip_into_utf8_editor);
    tcase_add_test (tc_core, test_paste_unrepresentable_char);
    tcase_add_test (tc_core, test_paste_same_codeset_keeps_bytes);
    tcase_add_test (tc_core, test_paste_vertical_block_recoded);
    tcase_add_test (tc_core, test_paste_plain_clip);
    tcase_add_test (tc_core, test_paste_stale_info);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
