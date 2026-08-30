/*
   src/panel-plugins/mcstruct - tests for the shipped def-files

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
    guint i;

    path = g_build_filename (MCSTRUCT_DATA_DIR, name, (char *) NULL);
    file = slv_file_load (path, &error);
    ck_assert_msg (file != NULL, "%s: %s", path, error != NULL ? error->message : "?");
    for (i = 0; i < file->errors->len; i++)
    {
        const slv_error_t *err = g_ptr_array_index (file->errors, i);

        fprintf (stderr, "%s:%d: %s\n", name, err->line, err->message);
    }
    ck_assert_msg (file->errors->len == 0, "%s: %u parse errors", name, file->errors->len);
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

START_TEST (test_all_def_files_parse)
{
    static const char *const names[] = { "zip.stl", "exe.stl",      "dbf.stl",    "elf.stl",
                                         "mbr.stl", "fat_boot.stl", "uimage.stl", "dtb.stl",
                                         "png.stl", "bmp.stl",      "wav.stl",    "sqlite.stl" };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
        slv_file_t *file = load_data_file (names[i]);

        ck_assert_msg (slv_file_first_struct (file) != NULL, "%s: no structure", names[i]);
        slv_file_free (file);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
put_le (unsigned char *p, guint64 v, int size)
{
    int i;

    for (i = 0; i < size; i++)
        p[i] = (unsigned char) (v >> (8 * i));
}

/* a minimal ELF64 little-endian image: header, one program header, one section header */
static gsize
build_elf (unsigned char *e)
{
    memset (e, 0, 256);
    memcpy (e, "\177ELF", 4);
    e[4] = 2; /* ELF64 */
    e[5] = 1; /* little endian */
    e[6] = 1;
    put_le (e + 16, 3, 2);  /* shared object */
    put_le (e + 18, 62, 2); /* x86-64 */
    put_le (e + 20, 1, 4);
    put_le (e + 24, 0x1000, 8);
    put_le (e + 32, 64, 8);  /* phoff */
    put_le (e + 40, 120, 8); /* shoff */
    put_le (e + 52, 64, 2);  /* ehsize */
    put_le (e + 54, 56, 2);  /* phentsize */
    put_le (e + 56, 1, 2);   /* phnum */
    put_le (e + 58, 64, 2);  /* shentsize */
    put_le (e + 60, 1, 2);   /* shnum */
    put_le (e + 62, 0, 2);
    put_le (e + 64, 1, 4);      /* PT_LOAD */
    put_le (e + 68, 5, 4);      /* flags */
    put_le (e + 120 + 4, 1, 4); /* SHT_PROGBITS */
    return 184;
}

/* --------------------------------------------------------------------------------------------- */

static void
check_elf (slv_file_t *file, const unsigned char *data, gsize len)
{
    slv_reader_t *reader;
    slv_eval_t ev;
    slv_node_t *root;
    const slv_node_t *n, *ph;

    reader = slv_reader_new_memory (data, len);
    ev.file = file;
    ev.reader = reader;
    ev.lazy_rows = 1000;
    ev.float_format = "%g";

    root = slv_eval_struct (&ev, slv_file_lookup (file, "ELF"), 0);
    n = find_child (root, "magic");
    ck_assert_str_eq (n->text, ".ELF");
    n = find_child (root, "class");
    ck_assert_ptr_nonnull (n->legend);
    n = find_child (root, "machine");
    ck_assert_ptr_nonnull (n->legend);
    n = find_child (root, "program header count");
    ck_assert_int_gt (n->value, 0);
    ph = find_child (root, "Phdr64");
    ck_assert_int_eq (ph->kind, SLV_NODE_TABLE);
    ck_assert_int_eq (ph->rows, n->value);
    ck_assert_int_eq (ph->children->len, (guint) n->value);
    n = find_child (g_ptr_array_index (ph->children, 0), "type");
    ck_assert_ptr_nonnull (n->legend);
    n = find_child (root, "section header count");
    ph = find_child (root, "Shdr64");
    ck_assert_int_eq (ph->children->len, (guint) n->value);

    slv_node_free (root);
    slv_reader_free (reader);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_elf)
{
    slv_file_t *file = load_data_file ("elf.stl");
    unsigned char e[256];
    gsize len = build_elf (e);
    char *data = NULL;
    gsize dlen = 0;

    check_elf (file, e, len);

    /* the test binary itself where /proc exists and the binary is an ELF (Linux) */
    if (g_file_get_contents ("/proc/self/exe", &data, &dlen, NULL))
    {
        if (dlen > 64 && memcmp (data, "\177ELF", 4) == 0 && data[4] == 2 && data[5] == 1)
            check_elf (file, (const unsigned char *) data, dlen);
        g_free (data);
    }

    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
put_be32 (GByteArray *a, guint32 v)
{
    unsigned char b[4] = { v >> 24, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF };

    g_byte_array_append (a, b, 4);
}

static void
add_chunk (GByteArray *a, const char *type, const unsigned char *data, gsize len)
{
    guint32 crc = 0xFFFFFFFFu;

    put_be32 (a, (guint32) len);
    g_byte_array_append (a, (const unsigned char *) type, 4);
    if (len > 0)
        g_byte_array_append (a, data, len);
    crc = slv_crc32 (crc, (const unsigned char *) type, 4);
    crc = slv_crc32 (crc, data, len);
    put_be32 (a, crc ^ 0xFFFFFFFFu);
}

START_TEST (test_png_chunks)
{
    static const unsigned char sig[] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    static const unsigned char ihdr[] = { 0, 0, 0, 2, 0, 0, 0, 1, 8, 2, 0, 0, 0 };
    static const unsigned char idat[] = { 1, 2, 3 };
    GByteArray *png = g_byte_array_new ();
    slv_file_t *file = load_data_file ("png.stl");
    slv_reader_t *reader;
    slv_eval_t ev = { file, NULL, 64, "%g", 0 };
    slv_node_t *root;
    const slv_node_t *rep, *row, *n;

    g_byte_array_append (png, sig, sizeof (sig));
    add_chunk (png, "IHDR", ihdr, sizeof (ihdr));
    add_chunk (png, "IDAT", idat, sizeof (idat));
    add_chunk (png, "IEND", NULL, 0);
    /* corrupt the IDAT crc */
    png->data[png->len - 12 - 1] ^= 0xFF;

    reader = slv_reader_new_memory (png->data, png->len);
    ev.reader = reader;
    root = slv_eval_struct (&ev, slv_file_lookup (file, "PNG"), 0);

    rep = find_child (root, "#repeat");
    ck_assert_int_eq (rep->rows, 3);
    row = g_ptr_array_index (rep->children, 0);
    ck_assert_str_eq (find_child (row, "type")->text, "IHDR");
    ck_assert_int_eq (find_child (row, "width")->value, 2);
    ck_assert_str_eq (find_child (row, "color type")->legend, "RGB");
    n = find_child (row, "crc check");
    ck_assert_str_eq (n->legend, "OK");
    row = g_ptr_array_index (rep->children, 1);
    ck_assert_str_eq (find_child (row, "type")->text, "IDAT");
    n = find_child (row, "crc check");
    ck_assert (g_str_has_prefix (n->legend, "MISMATCH"));
    row = g_ptr_array_index (rep->children, 2);
    ck_assert_str_eq (find_child (row, "type")->text, "IEND");
    ck_assert_int_eq (root->size, (off_t) png->len);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
    g_byte_array_free (png, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_all_def_files_parse);
    tcase_add_test (tc_core, test_elf);
    tcase_add_test (tc_core, test_png_chunks);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
