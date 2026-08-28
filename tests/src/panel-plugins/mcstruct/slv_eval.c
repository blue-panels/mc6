/*
   src/panel-plugins/mcstruct - tests for the STL evaluator

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

/* --------------------------------------------------------------------------------------------- */

static slv_file_t *
load_data_file (const char *name)
{
    char *path;
    slv_file_t *file;
    GError *error = NULL;

    path = g_build_filename (MCSTRUCT_DATA_DIR, name, (char *) NULL);
    file = slv_file_load (path, &error);
    ck_assert_msg (file != NULL, "%s: %s", path, error != NULL ? error->message : "?");
    ck_assert_int_eq (file->errors->len, 0);
    g_free (path);
    return file;
}

/* --------------------------------------------------------------------------------------------- */

static const slv_node_t *
find_child (const slv_node_t *node, const char *key)
{
    guint i;

    ck_assert_ptr_nonnull (node->children);
    for (i = 0; i < node->children->len; i++)
    {
        const slv_node_t *c = g_ptr_array_index (node->children, i);

        if (c->key != NULL && strcmp (c->key, key) == 0)
            return c;
    }
    ck_abort_msg ("child '%s' not found", key);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const slv_node_t *
child_at (const slv_node_t *node, guint i)
{
    ck_assert_ptr_nonnull (node->children);
    ck_assert_uint_lt (i, node->children->len);
    return g_ptr_array_index (node->children, i);
}

/* --------------------------------------------------------------------------------------------- */

static void
put16 (unsigned char *p, unsigned v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void
put32 (unsigned char *p, unsigned v)
{
    put16 (p, v & 0xFFFF);
    put16 (p + 2, v >> 16);
}

/* --------------------------------------------------------------------------------------------- */

/* local header + "abc" + "hello", central directory entry, end of central directory */
static gsize
build_zip (unsigned char *z)
{
    unsigned char *p = z;

    memcpy (p, "PK\3\4", 4);
    put16 (p + 4, 20);
    put16 (p + 6, 2);       /* flags: 8K dictionary */
    put16 (p + 8, 8);       /* method: imploded, 8K */
    put16 (p + 10, 0x6000); /* 12:00:00 */
    put16 (p + 12, 0x26F6); /* 1999-07-22 */
    put32 (p + 14, 0x12345678);
    put32 (p + 18, 5);
    put32 (p + 22, 5);
    put16 (p + 26, 3);
    put16 (p + 28, 0);
    memcpy (p + 30, "abc", 3);
    memcpy (p + 33, "hello", 5);
    p += 38;

    memcpy (p, "PK\1\2", 4);
    p[4] = 20;
    p[5] = 3; /* *nix */
    p[6] = 20;
    p[7] = 0;
    put16 (p + 8, 8);
    put16 (p + 10, 8);
    put16 (p + 12, 0x6000);
    put16 (p + 14, 0x26F6);
    put32 (p + 16, 0x12345678);
    put32 (p + 20, 5);
    put32 (p + 24, 5);
    put16 (p + 28, 3);
    put16 (p + 30, 0);
    put16 (p + 32, 0);
    put16 (p + 34, 0);
    put16 (p + 36, 1);
    put32 (p + 38, 0);
    put32 (p + 42, 0); /* offset of local header */
    memcpy (p + 46, "abc", 3);
    p += 49;

    memcpy (p, "PK\5\6", 4);
    put16 (p + 4, 0);
    put16 (p + 6, 0);
    put16 (p + 8, 1);
    put16 (p + 10, 1);
    put32 (p + 12, 49);
    put32 (p + 16, 38);
    put16 (p + 20, 0);
    p += 22;

    return p - z;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_zip_local_header)
{
    unsigned char z[128];
    gsize len = build_zip (z);
    slv_file_t *file = load_data_file ("zip.stl");
    slv_reader_t *reader = slv_reader_new_memory (z, len);
    slv_eval_t ev = { file, reader, SLV_DEFAULT_LAZY_ROWS, "%g" };
    slv_node_t *root;
    const slv_node_t *n;

    root = slv_eval_struct (&ev, slv_file_lookup (file, "Zip"), 0);
    ck_assert_ptr_nonnull (root);
    ck_assert_int_eq (root->kind, SLV_NODE_STRUCT);
    ck_assert_int_eq (root->offset, 0);
    ck_assert_int_eq (root->size, 38); /* header + name + data skipped by '+ csize' */

    n = find_child (root, "signature");
    ck_assert_int_eq (n->kind, SLV_NODE_FIELD);
    ck_assert_str_eq (n->hint, "d");
    ck_assert_str_eq (n->text, "04034B50");
    ck_assert_int_eq (n->size, 4);
    ck_assert_int_eq (n->line, 10);

    n = find_child (root, "flags");
    ck_assert_str_eq (n->hint, "t16");
    ck_assert_str_eq (n->text, "0000000000000010");
    ck_assert_str_eq (n->legend, "compression option 1");

    n = find_child (root, "method");
    ck_assert_str_eq (n->text, "8");
    ck_assert_str_eq (n->legend, "deflated");

    n = find_child (root, "modified");
    ck_assert_str_eq (n->text, "1999-07-22 12:00:00");

    n = find_child (root, "crc-32");
    ck_assert_str_eq (n->text, "12345678");

    n = find_child (root, "compressed size");
    ck_assert_str_eq (n->hint, "u32");
    ck_assert_int_eq (n->value, 5);
    ck_assert_int_eq (n->offset, 18);

    n = find_child (root, "name");
    ck_assert_str_eq (n->hint, "c[3]");
    ck_assert_str_eq (n->text, "abc");
    ck_assert_int_eq (n->offset, 30);
    ck_assert_int_eq (n->size, 3);

    n = find_child (root, "extra");
    ck_assert_int_eq (n->size, 0);
    ck_assert_str_eq (n->text, "");

    slv_node_free (root);

    /* central directory: the branch with a jump back to the local header */
    root = slv_eval_struct (&ev, slv_file_lookup (file, "Zip"), 38);
    ck_assert_int_eq (root->size, 49);
    n = find_child (root, "made on");
    ck_assert_str_eq (n->text, "3");
    ck_assert_str_eq (n->legend, "Unix");
    n = find_child (root, "local header offset");
    ck_assert_int_eq (n->kind, SLV_NODE_JUMP);
    ck_assert_str_eq (n->hint, "-> Zip");
    ck_assert_int_eq (n->jump_target, 0);
    ck_assert_int_eq (n->size, 4);
    n = find_child (root, "name");
    ck_assert_str_eq (n->text, "abc");
    slv_node_free (root);

    /* end of central directory */
    root = slv_eval_struct (&ev, slv_file_lookup (file, "Zip"), 87);
    ck_assert_int_eq (root->size, 22);
    n = find_child (root, "central directory offset");
    ck_assert_int_eq (n->jump_target, 38);
    slv_node_free (root);

    /* not a record: the #else branch */
    root = slv_eval_struct (&ev, slv_file_lookup (file, "Zip"), 4);
    n = child_at (root, 1);
    ck_assert_int_eq (n->kind, SLV_NODE_REMARK);
    ck_assert_str_eq (n->key, "ERROR!!! no ZIP record at this offset");
    slv_node_free (root);

    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_mz_header)
{
    unsigned char mz[80];
    slv_file_t *file = load_data_file ("exe.stl");
    slv_reader_t *reader;
    slv_eval_t ev;
    slv_node_t *root;
    const slv_node_t *n;

    memset (mz, 0, sizeof (mz));
    memcpy (mz, "MZ", 2);
    put16 (mz + 2, 0x90);
    put16 (mz + 4, 3);
    put16 (mz + 6, 1); /* relocations */
    put16 (mz + 8, 4); /* header paragraphs */
    put16 (mz + 10, 0);
    put16 (mz + 12, 0xFFFF);
    put16 (mz + 14, 0x200); /* ss */
    put16 (mz + 16, 0x100); /* sp */
    put16 (mz + 24, 0x40);  /* relocation table */
    put32 (mz + 0x3C, 0x40);
    memcpy (mz + 0x40, "PE\0\0", 4);

    reader = slv_reader_new_memory (mz, sizeof (mz));
    ev.file = file;
    ev.reader = reader;
    ev.lazy_rows = SLV_DEFAULT_LAZY_ROWS;
    ev.float_format = "%g";

    root = slv_eval_struct (&ev, slv_file_first_struct (file), 0);
    ck_assert_str_eq (root->key, "EXE");

    n = find_child (root, "MZ signature");
    ck_assert_str_eq (n->text, "5A4D");
    n = find_child (root, "relocations");
    ck_assert_str_eq (n->hint, "u16");
    ck_assert_int_eq (n->value, 1);
    n = find_child (root, "maximum paragraphs");
    ck_assert_str_eq (n->text, "65535");
    n = find_child (root, "initial sp");
    ck_assert_str_eq (n->text, "0100");

    n = find_child (root, "relocation table offset");
    ck_assert_int_eq (n->kind, SLV_NODE_JUMP);
    ck_assert_str_eq (n->hint, "-> :MzReloc");
    ck_assert_int_eq (n->jump_target, 0x40);
    ck_assert_int_eq (n->rows, 1);

    n = find_child (root, "PE header offset");
    ck_assert_int_eq (n->offset, 0x3C);
    ck_assert_int_eq (n->jump_target, 0x40);
    ck_assert_str_eq (n->hint, "-> PE");

    /* '.' 0x3c moved the cursor, the structure ends after the dword there */
    ck_assert_int_eq (root->size, 0x40);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static const char inline_def[] = "STL 4.00\n"
                                 "/Main\n"
                                 "n:   u8 1 count\n"
                                 "     * n Rec\n"
                                 "     t16 745 date\n"
                                 "#if n == 2\n"
                                 "     :two\n"
                                 "#elseif n == 3\n"
                                 "     :three\n"
                                 "#else\n"
                                 "     :other\n"
                                 "#fi\n"
                                 "#fi\n"
                                 "     . 0\n"
                                 "     j8 Rec=@+^ first\n"
                                 "     sp 0 pstr\n"
                                 "     sc 0 cstr\n"
                                 "     - 4\n"
                                 "     m16 1 big\n"
                                 "     f32 1 pi\n"
                                 "     d 0 peek\n"
                                 "     -4\n"
                                 "     p32 1 far\n"
                                 "     tu 1 when\n"
                                 "/Rec\n"
                                 "a:   u16 1 A\n"
                                 "     c 2 B\n"
                                 "/:T\n"
                                 "u8 x\n"
                                 "u8 y\n"
                                 "/Bad\n"
                                 "     * 1 Nope\n"
                                 "     d 1 x\n";

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_inline_structures)
{
    static const unsigned char data[] = {
        2,                      /* n */
        1,    0,    'a',  'b',  /* Rec 1 */
        2,    0,    'c',  'd',  /* Rec 2 */
        0x21, 0x2A,             /* t16 745 */
        2,    'h',  'i',        /* sp */
        'o',  'k',  0,          /* sc */
        0xDB, 0x0F, 0x49, 0x40, /* f32 pi, also read as m16 and the peek */
        0x00, 0x00, 0x00, 0x00, /* p32 (re-read after -4 covers f32 too) */
        0x80, 0x33, 0xE1, 0x01,
    };
    slv_file_t *file;
    slv_reader_t *reader;
    slv_eval_t ev;
    slv_node_t *root;
    const slv_node_t *n, *rec;
    gint64 v = 0;
    char *err = NULL;

    file = slv_file_parse (inline_def, strlen (inline_def), "inline.stl");
    ck_assert_int_eq (file->errors->len, 0);
    reader = slv_reader_new_memory (data, sizeof (data));
    ev.file = file;
    ev.reader = reader;
    ev.lazy_rows = SLV_DEFAULT_LAZY_ROWS;
    ev.float_format = "%.2f";

    root = slv_eval_struct (&ev, slv_file_lookup (file, "Main"), 0);

    n = child_at (root, 0);
    ck_assert_str_eq (n->key, "count");
    ck_assert_int_eq (n->value, 2);

    n = child_at (root, 1);
    ck_assert_int_eq (n->kind, SLV_NODE_NESTED);
    ck_assert_str_eq (n->hint, "Rec[2]");
    ck_assert_int_eq (n->rows, 2);
    ck_assert (!n->lazy);
    ck_assert_int_eq (n->children->len, 2);
    ck_assert_int_eq (n->offset, 1);
    ck_assert_int_eq (n->size, 8);
    rec = child_at (n, 1);
    ck_assert_int_eq (rec->kind, SLV_NODE_STRUCT);
    ck_assert_int_eq (rec->offset, 5);
    ck_assert_int_eq (rec->size, 4);
    ck_assert_int_eq (find_child (rec, "A")->value, 2);
    ck_assert_str_eq (find_child (rec, "B")->text, "cd");

    n = child_at (root, 2);
    ck_assert_str_eq (n->text, "0010101.0001.00001");
    ck_assert_int_eq (n->offset, 9);

    n = child_at (root, 3);
    ck_assert_int_eq (n->kind, SLV_NODE_REMARK);
    ck_assert_str_eq (n->key, "two");

    n = child_at (root, 4); /* j8 Rec=@+^ at local 0 */
    ck_assert_int_eq (n->kind, SLV_NODE_JUMP);
    ck_assert_int_eq (n->offset, 0);
    ck_assert_int_eq (n->jump_target, 2);
    ck_assert_str_eq (n->key, "first");

    n = child_at (root, 5); /* sp at 1: length 1, "\0"? no: re-read from offset 1 */
    ck_assert_int_eq (n->kind, SLV_NODE_FIELD);
    ck_assert_int_eq (n->offset, 1);
    ck_assert_int_eq (n->size, 2); /* length byte 1 + 1 char */

    slv_node_free (root);

    /* the string part, evaluated from offset 11 with a fresh view */
    {
        static const char def2[] = "STL 4.00\n/S\n sp 0 pstr\n sc 0 cstr\n -4\n m16 1 big\n"
                                   " f32 1 pi\n d 0 peek\n -4\n p32 1 far\n tu 1 when\n"
                                   " c 4 tail\n";
        slv_file_t *f2 = slv_file_parse (def2, strlen (def2), "s.stl");
        slv_eval_t ev2 = { f2, reader, SLV_DEFAULT_LAZY_ROWS, "%.2f" };

        ck_assert_int_eq (f2->errors->len, 0);
        root = slv_eval_struct (&ev2, slv_file_lookup (f2, "S"), 11);
        n = find_child (root, "pstr");
        ck_assert_str_eq (n->text, "hi");
        ck_assert_int_eq (n->size, 3);
        n = find_child (root, "cstr");
        ck_assert_str_eq (n->text, "ok");
        ck_assert_int_eq (n->size, 3);
        ck_assert_int_eq (n->offset, 14);
        n = find_child (root, "big");
        ck_assert_int_eq (n->offset, 13); /* -4 from 17 */
        ck_assert_str_eq (n->text, "696F");
        n = find_child (root, "pi");
        ck_assert_int_eq (n->offset, 15);
        n = find_child (root, "peek");
        ck_assert_int_eq (n->offset, 19);
        ck_assert (n->item->hidden);
        n = find_child (root, "far");
        ck_assert_int_eq (n->offset, 19);
        ck_assert_str_eq (n->text, "0000:4049");
        n = find_child (root, "when");
        ck_assert_str_eq (n->text, "1997-05-19 07:23:44");
        n = child_at (root, root->children->len - 1);
        ck_assert_int_eq (n->kind, SLV_NODE_ERROR);
        slv_node_free (root);

        slv_file_free (f2);
    }

    /* float formatting */
    {
        static const char def3[] = "STL 4.00\n/F\n f32 1 pi\n";
        slv_file_t *f3 = slv_file_parse (def3, strlen (def3), "f.stl");
        slv_eval_t ev3 = { f3, reader, SLV_DEFAULT_LAZY_ROWS, "%.2f" };

        root = slv_eval_struct (&ev3, slv_file_lookup (f3, "F"), 17);
        n = find_child (root, "pi");
        ck_assert_str_eq (n->text, "3.14");
        slv_node_free (root);
        slv_file_free (f3);
    }

    /* lazy rows */
    ev.lazy_rows = 1;
    root = slv_eval_struct (&ev, slv_file_lookup (file, "Main"), 0);
    n = child_at (root, 1);
    ck_assert (n->lazy);
    ck_assert_ptr_null (n->children);
    ck_assert_int_eq (n->size, 8);
    ck_assert_int_eq (child_at (root, 2)->offset, 9);
    ck_assert (slv_node_expand (&ev, (slv_node_t *) n));
    ck_assert (!n->lazy);
    ck_assert_int_eq (n->children->len, 2);
    ck_assert_int_eq (child_at (n, 1)->offset, 5);

    /* calculator in the scope of the structure */
    ck_assert (slv_eval_calc (&ev, root, "n * 3", &v, &err));
    ck_assert_int_eq (v, 6);
    ck_assert (slv_eval_calc (&ev, root, "^n", &v, &err));
    ck_assert_int_eq (v, 0);
    ck_assert (slv_eval_calc (&ev, child_at (root, 2), "@", &v, &err));
    ck_assert_int_eq (v, 0x2A21);
    slv_node_free (root);

    /* table */
    root = slv_eval_table (&ev, slv_file_lookup (file, "T"), 1, 3);
    ck_assert_int_eq (root->kind, SLV_NODE_TABLE);
    ck_assert_int_eq (root->children->len, 3);
    ck_assert_int_eq (root->size, 6);
    rec = child_at (root, 2);
    ck_assert_int_eq (rec->offset, 5);
    ck_assert_int_eq (find_child (rec, "x")->value, 2);
    ck_assert_int_eq (find_child (rec, "y")->value, 0);
    slv_node_free (root);

    /* errors are nodes, evaluation continues */
    root = slv_eval_struct (&ev, slv_file_lookup (file, "Bad"), sizeof (data) - 2);
    n = child_at (root, 0);
    ck_assert_int_eq (n->kind, SLV_NODE_ERROR);
    ck_assert_str_eq (n->text, "unknown structure 'nope'");
    n = child_at (root, 1);
    ck_assert_int_eq (n->kind, SLV_NODE_ERROR);
    ck_assert_str_eq (n->text, "read past end of file");
    slv_node_free (root);

    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_dump)
{
    static const char def[] = "STL 4.00\n/D\nv: w 1 word\n :note\n * 1 D2\n/D2\n b 1 byte\n";
    static const unsigned char data[] = { 0x34, 0x12, 0xAB };
    slv_file_t *file = slv_file_parse (def, strlen (def), "d.stl");
    slv_reader_t *reader = slv_reader_new_memory (data, sizeof (data));
    slv_eval_t ev = { file, reader, SLV_DEFAULT_LAZY_ROWS, "%g" };
    slv_node_t *root = slv_eval_struct (&ev, slv_file_lookup (file, "D"), 0);
    char *s = slv_node_dump (root, 0);

    ck_assert_str_eq (s,
                      "00000000 /D\n"
                      "00000000   word             w      1234\n"
                      "00000002   : note\n"
                      "00000002   D2               D2[1]  \n"
                      "00000002     /D2\n"
                      "00000002       byte             b      AB\n");
    g_free (s);
    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_zip_local_header);
    tcase_add_test (tc_core, test_mz_header);
    tcase_add_test (tc_core, test_inline_structures);
    tcase_add_test (tc_core, test_dump);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
