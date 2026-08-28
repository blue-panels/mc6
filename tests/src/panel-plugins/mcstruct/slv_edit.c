/*
   src/panel-plugins/mcstruct - tests for field value encoding

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

#include "src/panel-plugins/mcstruct/slv.h"
#include "src/panel-plugins/mcstruct/slv_edit.h"

/* --------------------------------------------------------------------------------------------- */

static const char def[] = "STL 4.00\n"
                          "/E\n"
                          " w 2 words\n"
                          " i16 1 int\n"
                          " u8 1 byte\n"
                          " m32 1 big\n"
                          " c 4 text\n"
                          " sp 5 pstr\n"
                          " sc 3 cstr\n"
                          " t16 745 bits\n"
                          " f32 1 flt\n"
                          " p32 1 far\n"
                          " td 1 dos\n"
                          " tu 1 unix\n"
                          " j16 E=@ jump\n"
                          " e32 1 msbin\n"
                          " : remark\n";

static unsigned char data[64];
static slv_file_t *file;
static slv_reader_t *reader;
static slv_node_t *root;

static void
setup (void)
{
    slv_eval_t ev;

    memset (data, 0, sizeof (data));
    file = slv_file_parse (def, strlen (def), "e.stl");
    ck_assert_int_eq (file->errors->len, 0);
    reader = slv_reader_new_memory (data, sizeof (data));
    ev.file = file;
    ev.reader = reader;
    ev.lazy_rows = 64;
    ev.float_format = "%g";
    root = slv_eval_struct (&ev, slv_file_lookup (file, "E"), 0);
    ck_assert_ptr_nonnull (root);
}

static void
teardown (void)
{
    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}

static const slv_node_t *
field (const char *key)
{
    guint i;

    for (i = 0; i < root->children->len; i++)
    {
        const slv_node_t *n = g_ptr_array_index (root->children, i);

        if (n->key != NULL && strcmp (n->key, key) == 0)
            return n;
    }
    ck_abort_msg ("no field %s", key);
    return NULL;
}

static void
expect_bytes (const char *key, const char *text, const unsigned char *bytes, gsize len)
{
    const slv_node_t *n = field (key);
    unsigned char out[64];
    char *err = NULL;

    ck_assert_int_eq (n->size, (off_t) len);
    ck_assert_msg (slv_node_encode (n, text, out, &err), "%s '%s': %s", key, text, err);
    ck_assert_msg (memcmp (out, bytes, len) == 0, "%s '%s': bytes differ", key, text);
}

static void
expect_error (const char *key, const char *text)
{
    const slv_node_t *n = field (key);
    unsigned char out[64];
    char *err = NULL;

    ck_assert_msg (!slv_node_encode (n, text, out, &err), "%s '%s' should fail", key, text);
    ck_assert_ptr_nonnull (err);
    g_free (err);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_numbers)
{
    static const unsigned char w[] = { 0x34, 0x12, 0xFF, 0x00 };
    static const unsigned char i16[] = { 0xFE, 0xFF };
    static const unsigned char u8[] = { 200 };
    static const unsigned char big[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    static const unsigned char j[] = { 0x10, 0x00 };

    expect_bytes ("words", "1234 ff", w, 4);
    expect_bytes ("words", "0x1234, 0xFF", w, 4);
    expect_error ("words", "1234");
    expect_error ("words", "1234 ff 00");
    expect_error ("words", "12345 ff");
    expect_error ("words", "zz ff");

    expect_bytes ("int", "-2", i16, 2);
    expect_error ("int", "40000");
    expect_bytes ("byte", "200", u8, 1);
    expect_bytes ("byte", "0xC8", u8, 1);
    expect_error ("byte", "-1");
    expect_error ("byte", "256");
    expect_bytes ("big", "DEADBEEF", big, 4);
    expect_bytes ("jump", "10", j, 2);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_strings_and_bits)
{
    static const unsigned char text[] = { 'a', 'b', 0, 0 };
    static const unsigned char ps[] = { 2, 'h', 'i', 0, 0, 0 };
    static const unsigned char cs[] = { 'o', 'k', 0 };
    static const unsigned char cs3[] = { 'a', 'b', 'c' };
    static const unsigned char bits[] = { 0x21, 0x2A };

    expect_bytes ("text", "ab", text, 4);
    expect_error ("text", "abcde");
    expect_bytes ("pstr", "hi", ps, 6);
    expect_error ("pstr", "toolong");
    expect_bytes ("cstr", "ok", cs, 3);
    expect_bytes ("cstr", "abc", cs3, 3);
    expect_error ("cstr", "abcd");

    expect_bytes ("bits", "0010101.0001.00001", bits, 2);
    expect_bytes ("bits", "0010101000100001", bits, 2);
    expect_bytes ("bits", "0x2A21", bits, 2);
    expect_error ("bits", "0101");
    expect_error ("bits", "2A21");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_float_ptr_time)
{
    static const unsigned char pi[] = { 0xDB, 0x0F, 0x49, 0x40 };
    static const unsigned char far[] = { 0x34, 0x12, 0xF0, 0x00 };
    static const unsigned char dos[] = { 0x00, 0x60, 0xF6, 0x26 };
    static const unsigned char unix_t[] = { 0x80, 0x33, 0xE1, 0x01 };
    const slv_node_t *n;
    char *s;

    expect_bytes ("flt", "3.14159274", pi, 4);
    expect_error ("flt", "abc");
    expect_bytes ("far", "00F0:1234", far, 4);
    expect_error ("far", "1234");
    expect_bytes ("dos", "1999-07-22 12:00:00", dos, 4);
    expect_error ("dos", "1970-01-01 00:00:00");
    expect_bytes ("unix", "1971-01-01 00:00:00", unix_t, 4);
    expect_error ("unix", "yesterday");

    n = field ("msbin");
    ck_assert (!slv_node_editable (n));
    expect_error ("msbin", "1");
    n = field ("remark");
    ck_assert (!slv_node_editable (n));
    ck_assert (!slv_node_editable (root));

    s = slv_node_edit_text (field ("words"), NULL);
    ck_assert_str_eq (s, "0000 0000");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* the text offered for editing comes from the bytes, not from the display text */
START_TEST (test_edit_text_from_bytes)
{
    static const unsigned char text_nul[4] = { 'a', 'b', 0, 0 };
    static const unsigned char text_bin[4] = { 'a', 0x80, 0, 0 };
    static const unsigned char pstr[5] = { 2, 'h', 'i', 'x', 'x' };
    static const unsigned char flt[4] = { 0xDB, 0x0F, 0x49, 0x40 }; /* pi as float */
    char *s;

    s = slv_node_edit_text (field ("text"), text_nul);
    ck_assert_str_eq (s, "ab");
    g_free (s);
    ck_assert_ptr_null (slv_node_edit_text (field ("text"), text_bin));
    s = slv_node_edit_text (field ("pstr"), pstr);
    ck_assert_str_eq (s, "hi");
    g_free (s);
    s = slv_node_edit_text (field ("flt"), flt);
    ck_assert_str_eq (s, "3.14159274");
    g_free (s);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_numbers);
    tcase_add_test (tc_core, test_strings_and_bits);
    tcase_add_test (tc_core, test_float_ptr_time);
    tcase_add_test (tc_core, test_edit_text_from_bytes);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
