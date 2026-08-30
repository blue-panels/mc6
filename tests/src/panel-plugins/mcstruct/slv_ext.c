/*
   src/panel-plugins/mcstruct - tests for the STL 5.00 extensions

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

static void
print_errors (const slv_file_t *file)
{
    guint i;

    for (i = 0; i < file->errors->len; i++)
    {
        const slv_error_t *err = g_ptr_array_index (file->errors, i);

        fprintf (stderr, "%s:%d: %s\n", file->path, err->line, err->message);
    }
}

static slv_node_t *
eval_text (const char *def, const unsigned char *data, gsize len, const char *name,
           slv_file_t **file_out, slv_reader_t **reader_out)
{
    slv_file_t *file = slv_file_parse (def, strlen (def), "inline.stl");
    slv_reader_t *reader = slv_reader_new_memory (data, len);
    slv_eval_t ev = { file, reader, 64, "%g" };
    slv_node_t *root;

    print_errors (file);
    ck_assert_int_eq (file->errors->len, 0);
    root = slv_eval_struct (&ev, slv_file_lookup (file, name), 0);
    ck_assert_ptr_nonnull (root);
    *file_out = file;
    *reader_out = reader;
    return root;
}

static const slv_node_t *
child (const slv_node_t *node, guint i)
{
    ck_assert_ptr_nonnull (node->children);
    ck_assert_uint_lt (i, node->children->len);
    return g_ptr_array_index (node->children, i);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_endian)
{
    static const char def[] = "STL 5.00\n"
                              "/E\n"
                              " w 1 le\n"
                              "#endian big\n"
                              " w 1 be\n"
                              " w.le 1 forced_le\n"
                              " m16 1 motorola\n"
                              " i16 1 sbe\n"
                              " -8\n"
                              "#if @w == 0x1234\n"
                              " :big seen\n"
                              "#fi\n"
                              " +8\n"
                              " * 1 Inner\n"
                              "#endian little\n"
                              " w 1 le_again\n"
                              "/Inner\n"
                              " w 1 inherited\n";
    static const unsigned char data[] = { 0x34, 0x12, 0x12, 0x34, 0x34, 0x12, 0x12,
                                          0x34, 0xFF, 0xFE, 0x12, 0x34, 0x34, 0x12 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "E", &file, &reader);
    const slv_node_t *n;
    unsigned char out[2];
    char *err = NULL;

    ck_assert_str_eq (child (root, 0)->text, "1234");
    ck_assert (!child (root, 0)->big_endian);
    n = child (root, 1);
    ck_assert_str_eq (n->text, "1234");
    ck_assert (n->big_endian);
    ck_assert_str_eq (child (root, 2)->text, "1234"); /* .le under #endian big */
    ck_assert_str_eq (child (root, 3)->text, "1234");
    ck_assert_str_eq (child (root, 4)->text, "-2");
    n = child (root, 5);
    ck_assert_int_eq (n->kind, SLV_NODE_REMARK);
    ck_assert_str_eq (n->key, "big seen");
    n = child (root, 6);
    ck_assert_int_eq (n->kind, SLV_NODE_NESTED);
    ck_assert_str_eq (child (child (n, 0), 0)->text, "1234"); /* nested inherits big */
    ck_assert_str_eq (child (root, 7)->text, "1234");         /* little again */

    /* encode respects the byte order the field was read with */
    ck_assert (slv_node_encode (child (root, 1), "ABCD", out, &err));
    ck_assert_int_eq (out[0], 0xAB);
    ck_assert_int_eq (out[1], 0xCD);
    ck_assert (slv_node_encode (child (root, 0), "ABCD", out, &err));
    ck_assert_int_eq (out[0], 0xCD);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_repeat)
{
    static const char def[] = "STL 5.00\n"
                              "/R\n"
                              "n: u8 1 count\n"
                              "#repeat n\n"
                              " u8 1 v\n"
                              "#end\n"
                              "#repeat while @b != 0xFF\n"
                              "tag: u8 1 tag\n"
                              " c 2 text\n"
                              "#end\n"
                              " u8 1 end\n"
                              "/Bad\n"
                              "#repeat while 1\n"
                              " :nothing\n"
                              "#end\n";
    static const unsigned char data[] = { 3, 10, 20, 30, 1, 'a', 'b', 2, 'c', 'd', 0xFF };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "R", &file, &reader);
    const slv_node_t *rep, *row;
    slv_eval_t ev = { file, reader, 64, "%g" };
    slv_node_t *bad;

    rep = child (root, 1);
    ck_assert_int_eq (rep->kind, SLV_NODE_REPEAT);
    ck_assert_int_eq (rep->rows, 3);
    ck_assert_int_eq (rep->offset, 1);
    ck_assert_int_eq (rep->size, 3);
    ck_assert_str_eq (rep->hint, "[3]");
    row = child (rep, 2);
    ck_assert_int_eq (row->kind, SLV_NODE_STRUCT);
    ck_assert_int_eq (row->offset, 3);
    ck_assert_str_eq (child (row, 0)->text, "30");

    rep = child (root, 2);
    ck_assert_int_eq (rep->rows, 2);
    ck_assert_int_eq (rep->size, 6);
    ck_assert_str_eq (child (child (rep, 1), 1)->text, "cd");

    ck_assert_str_eq (child (root, 3)->text, "255");
    ck_assert_int_eq (root->size, 11);
    slv_node_free (root);

    /* a body without bytes stops with an error node */
    bad = slv_eval_struct (&ev, slv_file_lookup (file, "Bad"), 0);
    rep = child (bad, 0);
    ck_assert_int_eq (rep->kind, SLV_NODE_REPEAT);
    ck_assert_int_eq (child (rep, rep->children->len - 1)->kind, SLV_NODE_ERROR);
    slv_node_free (bad);

    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_set_and_linked_list)
{
    static const char def[] = "STL 5.00\n"
                              "/Walker\n"
                              "#set ptr = 0x10\n"
                              "#set count = 0\n"
                              "#repeat while ptr != 0\n"
                              "        ..      ptr\n"
                              "        *       1       Node\n"
                              "#set    count = count + 1\n"
                              "#set    ptr = @u32(ptr)\n"
                              "#end\n"
                              "        u8      count   visited\n"
                              "/Node\n"
                              "next:   u32     1       next\n"
                              "        u8      1       value\n";
    /* three nodes: 0x10 -> 0x20 -> 0x08 -> end */
    unsigned char data[0x30];
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root;
    const slv_node_t *rep, *n;
    gint64 v = 0;
    char *err = NULL;
    slv_eval_t ev;

    memset (data, 0, sizeof (data));
    data[0x10] = 0x20;
    data[0x14] = 1;
    data[0x20] = 0x08;
    data[0x24] = 2;
    data[0x08] = 0;
    data[0x0C] = 3;
    data[0x00] = 0xAA; /* the count field is read at offset 0 after the walk */
    data[0x01] = 0xBB;
    data[0x02] = 0xCC;

    root = eval_text (def, data, sizeof (data), "Walker", &file, &reader);
    rep = child (root, 0);
    ck_assert_int_eq (rep->kind, SLV_NODE_REPEAT);
    ck_assert_int_eq (rep->rows, 3);
    n = child (child (child (rep, 0), 0), 0); /* row -> nested -> struct */
    ck_assert_int_eq (n->offset, 0x10);
    ck_assert_int_eq (child (n, 1)->value, 1);
    n = child (child (child (rep, 2), 0), 0);
    ck_assert_int_eq (n->offset, 0x08);
    ck_assert_int_eq (child (n, 1)->value, 3);
    /* the walk ended at 0x0D; "u8 count" reads 3 bytes there */
    n = child (root, 1);
    ck_assert_int_eq (n->offset, 0x0D);
    ck_assert_int_eq (n->size, 3);

    ev.file = file;
    ev.reader = reader;
    ev.lazy_rows = 64;
    ev.float_format = "%g";
    ck_assert (slv_eval_calc (&ev, root, "count", &v, &err));
    ck_assert_int_eq (v, 3);
    ck_assert (slv_eval_calc (&ev, root, "@u16(0x1)", &v, &err));
    ck_assert_int_eq (v, 0xCCBB);
    ck_assert (slv_eval_calc (&ev, root, "@u8(0x20) + @u8 (0x24)", &v, &err));
    ck_assert_int_eq (v, 0x0A);
    ck_assert (!slv_eval_calc (&ev, root, "@u32(0x100)", &v, &err));
    g_free (err);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_outer_labels)
{
    static const char def[] = "STL 5.00\n"
                              "/Outer\n"
                              "rlen:   u8      1       record length\n"
                              "        *       2       :Rec\n"
                              "/:Rec\n"
                              "c       1       flag\n"
                              "c       \"rlen - 1\"  data\n"
                              "/Alone\n"
                              "c       \"rlen - 1\"  data\n";
    static const unsigned char data[] = { 3, ' ', 'a', 'b', '*', 'c', 'd' };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root, *alone;
    const slv_node_t *tab;
    slv_eval_t ev = { NULL, NULL, 64, "%g" };

    root = eval_text (def, data, sizeof (data), "Outer", &file, &reader);
    tab = child (root, 1);
    ck_assert_int_eq (tab->kind, SLV_NODE_TABLE);
    ck_assert_int_eq (tab->size, 6);
    ck_assert_str_eq (child (child (tab, 1), 1)->text, "cd");

    /* a title read from the file: "=expr" */
    {
        static const char def2[] = "STL 5.00\n/T\n u8 1 =2\n u8 1 =9\n";
        static const unsigned char d2[] = { 7, 8, 'a', 'g', 'e', 0, 1, 2, 3 };
        slv_file_t *f2 = slv_file_parse (def2, strlen (def2), "t.stl");
        slv_reader_t *r2 = slv_reader_new_memory (d2, sizeof (d2));
        slv_eval_t ev2 = { f2, r2, 64, "%g" };
        slv_node_t *t;

        ck_assert_int_eq (f2->errors->len, 0);
        t = slv_eval_struct (&ev2, slv_file_lookup (f2, "T"), 0);
        ck_assert_str_eq (child (t, 0)->key, "age");
        ck_assert_str_eq (child (t, 1)->key, "=9"); /* past the end: the text stays */
        slv_node_free (t);
        slv_reader_free (r2);
        slv_file_free (f2);
    }

    /* without an enclosing structure the label is unknown */
    ev.file = file;
    ev.reader = reader;
    alone = slv_eval_struct (&ev, slv_file_lookup (file, "Alone"), 1);
    ck_assert_int_eq (child (alone, 0)->kind, SLV_NODE_ERROR);
    slv_node_free (alone);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_strings_and_checks)
{
    static const char def[] = "STL 5.00\n"
                              "/S\n"
                              " s8 6 name\n"
                              " s16 4 wide\n"
                              "crc: d 1 crc\n"
                              " crc32 0..14 == crc ok_crc\n"
                              " crc32 0..14 == 1 bad_crc\n"
                              " sum8 0..2 sum\n"
                              " sum16 0..14 wide_sum\n";
    static const unsigned char data[] = {
        'h', 'i', 0,   0, 0, 0,       /* s8 6 */
        'o', 0,   'k', 0, 0, 0, 0, 0, /* s16 4 */
        0,   0,   0,   0,             /* crc placeholder */
    };
    unsigned char buf[sizeof (data)];
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root;
    const slv_node_t *n;
    unsigned char out[8];
    char *err = NULL;

    /* crc32 of "hi\0\0\0\0o\0k\0\0\0\0\0" computed by the same routine through a first pass */
    memcpy (buf, data, sizeof (data));
    root = eval_text (def, buf, sizeof (buf), "S", &file, &reader);
    n = child (root, 3);
    ck_assert_str_eq (n->hint, "crc32");
    ck_assert_int_eq (n->size, 0);
    ck_assert_int_eq (n->offset, 0);
    ck_assert (g_str_has_prefix (n->legend, "MISMATCH"));
    {
        guint32 v = (guint32) n->value;

        buf[14] = v & 0xFF;
        buf[15] = (v >> 8) & 0xFF;
        buf[16] = (v >> 16) & 0xFF;
        buf[17] = (v >> 24) & 0xFF;
    }
    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);

    root = eval_text (def, buf, sizeof (buf), "S", &file, &reader);
    n = child (root, 0);
    ck_assert_str_eq (n->hint, "s8[6]");
    ck_assert_str_eq (n->text, "hi");
    ck_assert_int_eq (n->size, 6);
    n = child (root, 1);
    ck_assert_str_eq (n->hint, "s16[4]");
    ck_assert_str_eq (n->text, "ok");
    ck_assert_int_eq (n->size, 8);
    n = child (root, 3);
    ck_assert_str_eq (n->legend, "OK");
    ck_assert_str_eq (n->key, "ok_crc");
    n = child (root, 4);
    ck_assert (g_str_has_prefix (n->legend, "MISMATCH, expected 1"));
    n = child (root, 5);
    ck_assert_str_eq (n->hint, "sum8");
    ck_assert_int_eq (n->value, ('h' + 'i') & 0xFF);
    n = child (root, 6);
    ck_assert_int_eq (n->value, 'h' + 'i' + 'o' + 'k');
    ck_assert (!slv_node_editable (n));

    /* unicode edits */
    ck_assert (slv_node_encode (child (root, 0), "yo", out, &err));
    ck_assert (memcmp (out, "yo\0\0\0\0", 6) == 0);
    ck_assert (!slv_node_encode (child (root, 0), "toolongtext", out, &err));
    g_free (err);
    err = NULL;
    ck_assert (slv_node_encode (child (root, 1), "ab", out, &err));
    ck_assert (memcmp (out, "a\0b\0\0\0\0\0", 8) == 0);

    /* the crc32 routine itself */
    ck_assert_uint_eq (
        slv_crc32 (0xFFFFFFFFu, (const unsigned char *) "123456789", 9) ^ 0xFFFFFFFFu, 0xCBF43926u);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_literal_with_space)
{
    static const char def[] =
        "STL 5.00\n/S\n a: u16 1 a\n#if a == \"K \"\n :yes\n#else\n :no\n#fi\n"
        " td.be 1 t\n";
    static const unsigned char data[] = { ' ', 'K', 0x21, 0x00, 0x28, 0x5B };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "S", &file, &reader);

    ck_assert_int_eq (file->errors->len, 0);
    ck_assert_str_eq (child (root, 1)->key, "yes");
    /* td.be: date 0x285B, time 0x2100 read big-endian */
    ck_assert_int_eq (child (root, 2)->value, 0x2100285B);
    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_expr_limits)
{
    static const char def[] = "STL 5.00\n/L\n a: i64 1 a\n b: u32 \"a / -1\" b\n"
                              " c: u32 \"a % -1\" c\n";
    static const unsigned char data[] = { 0, 0, 0, 0, 0, 0, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root;
    GString *deep;
    char *err = NULL;
    slv_expr_t *e;
    int i;

    /* INT64_MIN / -1 and INT64_MIN % -1 do not trap */
    root = eval_text (def, data, sizeof (data), "L", &file, &reader);
    ck_assert_int_eq (child (root, 1)->kind, SLV_NODE_FIELD);
    ck_assert_int_eq (child (root, 2)->kind, SLV_NODE_FIELD);
    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);

    /* nesting is limited, no stack overflow */
    deep = g_string_new (NULL);
    for (i = 0; i < 100000; i++)
        g_string_append_c (deep, '(');
    g_string_append_c (deep, '1');
    for (i = 0; i < 100000; i++)
        g_string_append_c (deep, ')');
    e = slv_expr_parse (deep->str, &err);
    ck_assert_ptr_null (e);
    ck_assert_ptr_nonnull (err);
    g_free (err);
    err = NULL;
    g_string_assign (deep, "");
    for (i = 0; i < 100000; i++)
        g_string_append_c (deep, '-');
    g_string_append_c (deep, '1');
    e = slv_expr_parse (deep->str, &err);
    ck_assert_ptr_null (e);
    g_free (err);
    g_string_free (deep, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* a file-supplied row count is capped, rows of zero size stop the array */
START_TEST (test_row_limits)
{
    static const char def[] = "STL 5.00\n/R\n n: u32 1 n\n * n Empty\n/Empty\n * 0 Other\n"
                              "/Other\n u8 1 x\n";
    static const unsigned char data[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "R", &file, &reader);
    const slv_node_t *arr = child (root, 1);

    ck_assert (arr->children == NULL || arr->children->len < 10);
    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_endian_float_and_bits)
{
    static const char def[] = "STL 5.00\n"
                              "/E\n"
                              "#endian big\n"
                              " f32 1 f\n"
                              " f64 1 d\n"
                              "b:  t16 1 bits\n"
                              "#if b == 0x0102\n"
                              " :label is the big endian value\n"
                              "#fi\n"
                              " f32.le 1 fle\n";
    /* 1.5f big endian, 2.5 big endian, 0x0102, 1.5f little endian */
    static const unsigned char data[] = { 0x3F, 0xC0, 0x00, 0x00, 0x40, 0x04, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0xC0, 0x3F };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "E", &file, &reader);

    ck_assert_str_eq (child (root, 0)->text, "1.5");
    ck_assert_str_eq (child (root, 1)->text, "2.5");
    ck_assert_str_eq (child (root, 2)->text, "0000000100000010");
    ck_assert_int_eq (child (root, 2)->value, 0x0102);
    ck_assert_int_eq (child (root, 3)->kind, SLV_NODE_REMARK);
    ck_assert_str_eq (child (root, 4)->text, "1.5");

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_value_rows)
{
    static const char def[] = "STL 5.00\n"
                              "/V\n"
                              "lo: u16 1 low\n"
                              "hi: u16 1 high\n"
                              "all: = \"lo + (hi << 16)\" both words\n"
                              " =x all hex\n"
                              " =t 86400 a day after the epoch\n"
                              "#if all == 0x00020001\n"
                              " :label seen\n"
                              "#fi\n"
                              " u8 1 next\n";
    static const unsigned char data[] = { 0x01, 0x00, 0x02, 0x00, 0x77 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "V", &file, &reader);
    const slv_node_t *n;

    n = child (root, 2);
    ck_assert_int_eq (n->kind, SLV_NODE_FIELD);
    ck_assert_int_eq ((int) n->size, 0);
    ck_assert_int_eq ((int) n->offset, 4);
    ck_assert_str_eq (n->key, "both words");
    ck_assert_str_eq (n->text, "131073");
    ck_assert_str_eq (n->hint, "=");
    ck_assert (!slv_node_editable (n));
    ck_assert_str_eq (child (root, 3)->text, "20001");
    ck_assert_str_eq (child (root, 4)->text, "1970-01-02 00:00:00");
    ck_assert_int_eq (child (root, 5)->kind, SLV_NODE_REMARK);
    ck_assert_int_eq ((int) child (root, 6)->offset, 4);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_switch)
{
    static const char def[] = "STL 5.00\n"
                              "/S\n"
                              "#repeat 3\n"
                              "t: u8 1 type\n"
                              "#switch t\n"
                              "#case 1\n"
                              " u8 1 one\n"
                              "#case 2, 3\n"
                              " u16 1 two or three\n"
                              "#default\n"
                              " :other\n"
                              "#end\n"
                              "#end\n";
    static const unsigned char data[] = { 1, 0xAA, 3, 0x34, 0x12, 9 };
    static const char bad[] = "STL 5.00\n/B\n u8 1 x\n#switch x\n u8 1 stray\n#case 1\n#fi\n";
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "S", &file, &reader);
    const slv_node_t *rep = child (root, 0);

    ck_assert_str_eq (child (child (rep, 0), 1)->key, "one");
    ck_assert_str_eq (child (child (rep, 1), 1)->text, "4660");
    ck_assert_int_eq (child (child (rep, 2), 1)->kind, SLV_NODE_REMARK);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);

    file = slv_file_parse (bad, strlen (bad), "bad.stl");
    ck_assert_uint_ge (file->errors->len, 2); /* stray item, #fi inside #switch */
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_terminator_and_encoding)
{
    static const char def[] = "STL 5.00\n"
                              "/T\n"
                              " sc.0a 0 line\n"
                              " sc 0 plain\n"
                              "#encoding cp866\n"
                              " c 2 dos\n"
                              " sc 0 dosz\n"
                              "#encoding utf-8\n"
                              " c 2 raw\n";
    /* "ab\n", "c\0", cp866 "AB" (0x80 0x81), cp866 "B\0", raw 0x80 0x81 */
    static const unsigned char data[] = { 'a', 'b', 0x0A, 'c', 0, 0x80, 0x81, 0x81, 0, 0x80, 0x81 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "T", &file, &reader);

    ck_assert_str_eq (child (root, 0)->text, "ab");
    ck_assert_int_eq ((int) child (root, 0)->size, 3);
    ck_assert_str_eq (child (root, 1)->text, "c");
    ck_assert_str_eq (child (root, 2)->text, "\xd0\x90\xd0\x91");
    ck_assert_str_eq (child (root, 3)->text, "\xd0\x91");
    ck_assert_str_eq (child (root, 4)->text, "..");

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_check_expr)
{
    static const char def[] = "STL 5.00\n"
                              "/C\n"
                              "m: w 1 magic\n"
                              " check \"m == 0x5A4D\" magic check\n"
                              "ok: check \"m == 1\"\n"
                              " u8 1 next\n";
    static const unsigned char data[] = { 0x4D, 0x5A, 0x77 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "C", &file, &reader);

    ck_assert_str_eq (child (root, 1)->key, "magic check");
    ck_assert_str_eq (child (root, 1)->legend, "OK");
    ck_assert_int_eq ((int) child (root, 1)->size, 0);
    ck_assert_str_eq (child (root, 2)->legend, "MISMATCH");
    ck_assert_int_eq ((int) child (root, 3)->offset, 2);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_time64)
{
    static const char def[] = "STL 5.00\n"
                              "/T\n"
                              " tu64 1 unix\n"
                              " tf 1 win\n"
                              " tf 1 none\n";
    /* 86400 as u64; FILETIME of 1970-01-02 00:00:00: (11644473600 + 86400) * 1e7 */
    static const unsigned char data[] = { 0x80, 0x51, 0x01, 0,    0,    0,    0,    0,
                                          0x00, 0x40, 0xA8, 0xFF, 0xA7, 0xB2, 0x9D, 0x01,
                                          0,    0,    0,    0,    0,    0,    0,    0 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "T", &file, &reader);

    ck_assert_str_eq (child (root, 0)->text, "1970-01-02 00:00:00");
    ck_assert_str_eq (child (root, 0)->hint, "tu64");
    ck_assert_str_eq (child (root, 1)->text, "1970-01-02 00:00:00");
    ck_assert (!slv_node_editable (child (root, 1)));
    ck_assert_str_eq (child (root, 2)->text, "0");

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_call)
{
    static const char def[] = "STL 5.00\n"
                              "/C\n"
                              " b 4 data\n"
                              " = \"call('sum8', 0, 4)\" sum\n"
                              " =x \"call('crc32', 0, 4)\" crc\n"
                              " = \"call('find', 0, 4, 0x0403, 2)\" where\n"
                              " = \"call('find', 0, 4, 0x99, 1)\" nowhere\n"
                              " = \"call('exec:cat', 0, 4)\" refused\n";
    static const unsigned char data[] = { 1, 2, 3, 4 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "C", &file, &reader);

    ck_assert_str_eq (child (root, 1)->text, "10");
    ck_assert_str_eq (child (root, 2)->text, "B63CFBCD");
    ck_assert_str_eq (child (root, 3)->text, "2");
    ck_assert_str_eq (child (root, 4)->text, "-1");
    ck_assert_int_eq (child (root, 5)->kind, SLV_NODE_ERROR);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_via_buffer)
{
    static const char def[] = "STL 5.00\n"
                              "/O\n"
                              " u8 1 lead\n"
                              " * 1 In via call('raw', 1, 2)\n"
                              " * 1 In via call('xor', 1, 2, 0xFF)\n"
                              " * 1 In via call('nosuch', 1, 2)\n"
                              " u8 1 after\n"
                              "/In\n"
                              " u8 1 a\n"
                              " u8 1 b\n";
    static const unsigned char data[] = { 9, 1, 2, 3 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "O", &file, &reader);
    const slv_node_t *n = child (root, 1);
    gsize len = 0;
    const unsigned char *b;

    ck_assert_int_eq (n->kind, SLV_NODE_BUFFER);
    ck_assert_str_eq (n->text, "2 bytes");
    ck_assert_str_eq (n->hint, "via raw");
    b = g_bytes_get_data (n->buffer, &len);
    ck_assert_uint_eq (len, 2);
    ck_assert_int_eq (b[0], 1);
    b = g_bytes_get_data (child (root, 2)->buffer, &len);
    ck_assert_int_eq (b[1], 0xFD);
    ck_assert_int_eq (child (root, 3)->kind, SLV_NODE_ERROR);
    ck_assert_int_eq ((int) child (root, 4)->offset, 1); /* a buffer takes no bytes */

    /* the structure inside is read from the buffer, offsets start at 0 */
    {
        slv_reader_t *mem = slv_reader_new_memory (g_bytes_get_data (n->buffer, NULL), 2);
        slv_eval_t ev = { file, mem, 64, "%g", 0 };
        slv_node_t *in = slv_eval_struct (&ev, n->def, 0);

        ck_assert_str_eq (child (in, 1)->text, "2");
        ck_assert_int_eq ((int) child (in, 1)->offset, 1);
        slv_node_free (in);
        slv_reader_free (mem);
    }

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_quoted_follower)
{
    static const char def[] = "STL 5.00\n"
                              "/Q\n"
                              " c \"$size\" all\n"
                              "#if @w == \"KP\"\n"
                              " :literal seen\n"
                              "#fi\n";
    static const unsigned char data[] = { 'P', 'K', '!' };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "Q", &file, &reader);

    ck_assert_int_eq ((int) child (root, 0)->size, 3);         /* "$size" is the expression */
    ck_assert_int_eq (child (root, 1)->kind, SLV_NODE_REMARK); /* "KP" is still a literal */

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_varint)
{
    static const char def[] =
        "STL 5.00\n/V\n a: v 1 a\n b: v 1 b\n c: v 1 c\n d: vl 1 d\n e: v 1 e\n";
    /* 0x7F, 300 (0x82 0x2C), 9 bytes with a full last byte, LEB128 300 (0xAC 0x02), unterminated */
    static const unsigned char data[] = { 0x7F, 0x82, 0x2C, 0x81, 0x80, 0x80, 0x80, 0x80,
                                          0x80, 0x80, 0x80, 0xFF, 0xAC, 0x02, 0x80 };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_node_t *root = eval_text (def, data, sizeof (data), "V", &file, &reader);
    const slv_node_t *n;

    n = child (root, 0);
    ck_assert_str_eq (n->hint, "v");
    ck_assert_int_eq (n->size, 1);
    ck_assert_int_eq (n->value, 0x7F);
    n = child (root, 1);
    ck_assert_int_eq (n->size, 2);
    ck_assert_int_eq (n->value, 300);
    ck_assert_str_eq (n->text, "300");
    n = child (root, 2);
    ck_assert_int_eq (n->size, 9);
    ck_assert_int_eq (n->value, ((gint64) 1 << 57) | 0xFF);
    n = child (root, 3);
    ck_assert_str_eq (n->hint, "vl");
    ck_assert_int_eq (n->size, 2);
    ck_assert_int_eq (n->value, 300);
    n = child (root, 4);
    ck_assert_int_eq (n->kind, SLV_NODE_ERROR);
    ck_assert (!slv_node_editable (child (root, 1)));

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_include_and_versions)
{
    char *dir = g_dir_make_tmp ("mcstruct-XXXXXX", NULL);
    char *inc = g_build_filename (dir, "legends.stl", (char *) NULL);
    char *main_path = g_build_filename (dir, "main.stl", (char *) NULL);
    slv_file_t *file;
    const slv_error_t *err;
    GError *error = NULL;
    char *cmd;

    ck_assert (g_file_set_contents (
        inc, "STL 5.00\n/'colors\n1 red\n2 green\n/Shared\n u8 'colors c\n", -1, NULL));
    ck_assert (g_file_set_contents (main_path,
                                    "STL 5.00\n#include \"legends.stl\"\n/Main\n u8 'colors x\n"
                                    "#if 1\n :a\n#elseif 0\n :b\n#else\n :c\n#fi\n"
                                    "#include \"missing.stl\"\n",
                                    -1, NULL));
    file = slv_file_load (main_path, &error);
    ck_assert_ptr_nonnull (file);
    ck_assert_ptr_nonnull (slv_file_lookup (file, "colors"));
    ck_assert_ptr_nonnull (slv_file_lookup (file, "Shared"));
    ck_assert_ptr_nonnull (slv_file_lookup (file, "Main"));
    /* only the main file's lines are kept for the def zone */
    ck_assert_int_eq (file->lines->len, 12);
    /* one #fi closes an #if/#elseif/#else chain in 5.00, missing include is an error */
    ck_assert_int_eq (file->errors->len, 1);
    err = g_ptr_array_index (file->errors, 0);
    ck_assert_int_eq (err->line, 12);
    ck_assert (strstr (err->message, "missing.stl") != NULL);
    slv_file_free (file);

    /* a file that includes itself, an absolute path and '..' are errors, not loops */
    ck_assert (g_file_set_contents (main_path,
                                    "STL 5.00\n#include \"main.stl\"\n#include \"/etc/passwd\"\n"
                                    "#include \"../x.stl\"\n#include \"legends.stl\"\n",
                                    -1, NULL));
    ck_assert (g_file_set_contents (inc, "STL 5.00\n#include \"legends.stl\"\n/Shared\n u8 1 c\n",
                                    -1, NULL));
    file = slv_file_load (main_path, &error);
    ck_assert_ptr_nonnull (file);
    ck_assert_int_eq (file->errors->len, 4);
    ck_assert_ptr_nonnull (slv_file_lookup (file, "Shared"));
    slv_file_free (file);

    /* 4.00 rejects the extensions */
    {
        static const char def4[] =
            "STL 4.00\n/A\n q 1 x\n s8 4 y\n#endian big\n#repeat 2\n w 1 z\n#end\n"
            " w.be 1 v\n crc32 0..2 c\n";

        file = slv_file_parse (def4, strlen (def4), "old.stl");
        ck_assert_int_ge (file->errors->len, 6);
        err = g_ptr_array_index (file->errors, 0);
        ck_assert_str_eq (err->message, "'q' needs STL 5.00");
        slv_file_free (file);
    }

    cmd = g_strdup_printf ("rm -rf '%s'", dir);
    ck_assert_int_eq (system (cmd), 0);
    g_free (cmd);
    g_free (inc);
    g_free (main_path);
    g_free (dir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_endian);
    tcase_add_test (tc_core, test_repeat);
    tcase_add_test (tc_core, test_set_and_linked_list);
    tcase_add_test (tc_core, test_outer_labels);
    tcase_add_test (tc_core, test_endian_float_and_bits);
    tcase_add_test (tc_core, test_value_rows);
    tcase_add_test (tc_core, test_switch);
    tcase_add_test (tc_core, test_terminator_and_encoding);
    tcase_add_test (tc_core, test_check_expr);
    tcase_add_test (tc_core, test_time64);
    tcase_add_test (tc_core, test_call);
    tcase_add_test (tc_core, test_via_buffer);
    tcase_add_test (tc_core, test_quoted_follower);
    tcase_add_test (tc_core, test_varint);
    tcase_add_test (tc_core, test_expr_limits);
    tcase_add_test (tc_core, test_literal_with_space);
    tcase_add_test (tc_core, test_row_limits);
    tcase_add_test (tc_core, test_strings_and_checks);
    tcase_add_test (tc_core, test_include_and_versions);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
