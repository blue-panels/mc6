/*
   src/panel-plugins/mcstruct - tests for the STL def-file parser

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
parse_text (const char *text)
{
    return slv_file_parse (text, strlen (text), "inline.stl");
}

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
    g_free (path);
    print_errors (file);
    return file;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_shipped_files_parse)
{
    static const char *const names[] = { "zip.stl", "exe.stl", "dbf.stl" };
    size_t i;

    for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
        slv_file_t *file = load_data_file (names[i]);

        ck_assert_int_eq (file->version_major, 5);
        ck_assert_msg (file->errors->len == 0, "%s: %u parse errors", names[i], file->errors->len);
        ck_assert_msg (slv_file_first_struct (file) != NULL, "%s: no structure", names[i]);
        slv_file_free (file);
    }
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_zip_definitions)
{
    slv_file_t *file = load_data_file ("zip.stl");
    const slv_def_t *def;
    const slv_item_t *it;

    def = slv_file_lookup (file, "Zip");
    ck_assert_ptr_nonnull (def);
    ck_assert_int_eq (def->kind, SLV_DEF_STRUCT);
    ck_assert_int_eq (def->items->len, 2); /* signature + the #if chain */

    it = g_ptr_array_index (def->items, 0);
    ck_assert_int_eq (it->kind, SLV_ITEM_FIELD);
    ck_assert_str_eq (it->label, "sig");
    ck_assert_int_eq (it->type, SLV_TYPE_HEX);
    ck_assert_int_eq (it->size, 4);
    ck_assert_str_eq (it->name, "signature");

    it = g_ptr_array_index (def->items, 1);
    ck_assert_int_eq (it->kind, SLV_ITEM_IF);
    ck_assert_int_eq (it->branches->len, 7); /* 5.00: one chain, one #fi */

    def = slv_file_lookup (file, "zip_method");
    ck_assert_ptr_nonnull (def);
    ck_assert_int_eq (def->kind, SLV_DEF_VALUE_LEGEND);
    ck_assert_int_eq (def->entries->len, 20);
    ck_assert_int_eq (g_array_index (def->entries, slv_legend_entry_t, 7).min, 8);
    ck_assert_str_eq (g_array_index (def->entries, slv_legend_entry_t, 0).text, "stored");

    def = slv_file_lookup (file, "zip_flags");
    ck_assert_ptr_nonnull (def);
    ck_assert_int_eq (def->kind, SLV_DEF_BIT_LEGEND);
    ck_assert_int_eq (g_array_index (def->entries, slv_legend_entry_t, 3).min, 0x08);
    ck_assert_int_eq (g_array_index (def->entries, slv_legend_entry_t, 3).max, 0x08);

    def = slv_file_lookup (file, "zip_host");
    ck_assert_ptr_nonnull (def);
    ck_assert_int_eq (def->entries->len, 20);

    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_dbf_table_columns)
{
    slv_file_t *file = load_data_file ("dbf.stl");
    const slv_def_t *def;
    const slv_item_t *it;

    def = slv_file_lookup (file, "Field");
    ck_assert_ptr_nonnull (def);
    ck_assert_int_eq (def->kind, SLV_DEF_TABLE);
    ck_assert_int_eq (def->items->len, 9);

    it = g_ptr_array_index (def->items, 0); /* sc 11 name */
    ck_assert_int_eq (it->type, SLV_TYPE_CSTRING);
    ck_assert_str_eq (it->name, "name");

    it = g_ptr_array_index (def->items, 1); /* c 'dbf_type type */
    ck_assert_int_eq (it->follower_kind, SLV_FOLLOWER_LEGEND);
    ck_assert_str_eq (it->legend, "dbf_type");
    ck_assert_str_eq (it->name, "type");

    it = g_ptr_array_index (def->items, 2); /* d 1 displacement */
    ck_assert_int_eq (it->type, SLV_TYPE_HEX);
    ck_assert_str_eq (it->name, "displacement");

    it = g_ptr_array_index (def->items, 5); /* t8 `dbf_field_flags flags */
    ck_assert_int_eq (it->type, SLV_TYPE_BITS);
    ck_assert_str_eq (it->legend, "dbf_field_flags");

    def = slv_file_lookup (file, "RecordRaw");
    ck_assert_ptr_nonnull (def);
    it = g_ptr_array_index (def->items, 0); /* c:1 'dbf_deleted deleted */
    ck_assert_str_eq (it->legend, "dbf_deleted");
    ck_assert_str_eq (it->name, "deleted");

    def = slv_file_lookup (file, "DBF");
    ck_assert_ptr_nonnull (def);
    it = g_ptr_array_index (def->items, 3); /* hlen: j16 :Record[nrec]=@ */
    ck_assert_int_eq (it->kind, SLV_ITEM_JUMP);
    ck_assert_str_eq (it->label, "hlen");
    ck_assert (it->target_is_table);
    ck_assert_str_eq (it->target, "recordraw");
    ck_assert_ptr_nonnull (it->rows);

    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_exe_offset_ops)
{
    slv_file_t *file = load_data_file ("exe.stl");
    const slv_def_t *def;
    const slv_item_t *it;
    const slv_branch_t *br;
    char *s;

    def = slv_file_lookup (file, "EXE");
    ck_assert_ptr_nonnull (def);
    ck_assert_int_eq (def->items->len, 2);
    it = g_ptr_array_index (def->items, 1);
    ck_assert_int_eq (it->kind, SLV_ITEM_IF);
    ck_assert_int_eq (it->branches->len, 2);
    br = g_ptr_array_index (it->branches, 0);
    s = slv_expr_to_string (br->cond);
    ck_assert_str_eq (s, "(sig != 23117)");
    g_free (s);

    /* the #else branch: relocation table jump, '.' 0x3c, then the PE jump */
    br = g_ptr_array_index (it->branches, 1);
    it = g_ptr_array_index (br->items, 11);
    ck_assert_int_eq (it->kind, SLV_ITEM_JUMP);
    ck_assert (it->target_is_table);
    ck_assert_str_eq (it->target, "mzreloc");
    it = g_ptr_array_index (br->items, 13);
    ck_assert_int_eq (it->kind, SLV_ITEM_SEEK);
    it = g_ptr_array_index (br->items, 14);
    ck_assert_int_eq (it->kind, SLV_ITEM_JUMP);
    ck_assert_str_eq (it->target, "pe");
    ck_assert_int_eq (it->size, 4);

    def = slv_file_lookup (file, "PE");
    ck_assert_ptr_nonnull (def);
    ck_assert (!def->hidden);
    def = slv_file_lookup (file, "Section");
    ck_assert_ptr_nonnull (def);
    ck_assert_int_eq (def->kind, SLV_DEF_TABLE);

    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_inline_syntax)
{
    slv_file_t *file;
    const slv_def_t *def;
    const slv_item_t *it;

    file = parse_text ("STL 4.00\n"
                       "; comment\n"
                       "/Main\n"
                       "cnt:\n"
                       "  u8 1 count ; how many\n"
                       "  t16 745 date\n"
                       "  * cnt _Rec\n"
                       "  :--- remark ---\n"
                       "  -2\n"
                       "  . cnt*2\n"
                       "  sp 0 pstr\n"
                       "  sc 10 cstr\n"
                       "  m32 1 big\n"
                       "  d 0 hidden\n"
                       "/_Rec\n"
                       "  w \"cnt + 1\" words\n");
    print_errors (file);
    ck_assert_int_eq (file->errors->len, 0);
    ck_assert_int_eq (file->defs->len, 2);

    def = slv_file_lookup (file, "main");
    ck_assert_ptr_nonnull (def);
    ck_assert_int_eq (def->items->len, 10);

    it = g_ptr_array_index (def->items, 0);
    ck_assert_str_eq (it->label, "cnt");
    ck_assert_int_eq (it->type, SLV_TYPE_UINT);
    ck_assert_str_eq (it->comment, "how many");

    it = g_ptr_array_index (def->items, 1);
    ck_assert_int_eq (it->type, SLV_TYPE_BITS);
    ck_assert_int_eq (it->follower_kind, SLV_FOLLOWER_BITS);
    ck_assert_str_eq (it->bits, "745");

    it = g_ptr_array_index (def->items, 2);
    ck_assert_int_eq (it->kind, SLV_ITEM_NESTED);
    ck_assert_str_eq (it->target, "_rec");

    it = g_ptr_array_index (def->items, 3);
    ck_assert_int_eq (it->kind, SLV_ITEM_REMARK);
    ck_assert_str_eq (it->name, "--- remark ---");

    it = g_ptr_array_index (def->items, 4);
    ck_assert_int_eq (it->kind, SLV_ITEM_SKIP);
    ck_assert_int_eq (it->direction, -1);

    it = g_ptr_array_index (def->items, 5);
    ck_assert_int_eq (it->kind, SLV_ITEM_SEEK);

    it = g_ptr_array_index (def->items, 6);
    ck_assert_int_eq (it->type, SLV_TYPE_PSTRING);
    it = g_ptr_array_index (def->items, 7);
    ck_assert_int_eq (it->type, SLV_TYPE_CSTRING);
    it = g_ptr_array_index (def->items, 8);
    ck_assert_int_eq (it->type, SLV_TYPE_BE_HEX);
    ck_assert (it->big_endian);
    it = g_ptr_array_index (def->items, 9);
    ck_assert (it->hidden);

    def = slv_file_lookup (file, "_Rec");
    ck_assert_ptr_nonnull (def);
    ck_assert (def->hidden);
    it = g_ptr_array_index (def->items, 0);
    ck_assert_ptr_nonnull (it->follower);
    ck_assert_str_eq (it->name, "words");

    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_errors)
{
    slv_file_t *file;
    const slv_error_t *err;

    file = parse_text ("STL 4.00\n"
                       "/A\n"
                       "  zz 1 bad type\n"
                       "  w\n"
                       "#else\n"
                       "#if 1\n");
    ck_assert_int_eq (file->errors->len, 4);
    err = g_ptr_array_index (file->errors, 0);
    ck_assert_int_eq (err->line, 3);
    ck_assert_str_eq (err->message, "unknown type 'zz'");
    err = g_ptr_array_index (file->errors, 1);
    ck_assert_int_eq (err->line, 4);
    err = g_ptr_array_index (file->errors, 2);
    ck_assert_int_eq (err->line, 5);
    err = g_ptr_array_index (file->errors, 3);
    ck_assert_int_eq (err->line, 6);
    slv_file_free (file);

    file = parse_text ("w 1 x\n");
    ck_assert_int_ge (file->errors->len, 1);
    err = g_ptr_array_index (file->errors, 0);
    ck_assert_str_eq (err->message, "first line must be 'STL n.nn'");
    slv_file_free (file);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_expressions)
{
    static const struct
    {
        const char *text;
        gint64 value;
    } cases[] = {
        { "1+2*3", 7 },  { "(1+2)*3", 9 },     { "0x10<<2", 64 },  { "1010b", 10 },
        { "037", 31 },   { "\"KP\"", 0x4B50 }, { "'PK'", 0x4B50 }, { "7/2", 3 },
        { "7%4", 3 },    { "-3", -3 },         { "!0", 1 },        { "3 && 0", 0 },
        { "0 || 5", 1 }, { "1 == 1", 1 },      { "2 != 2", 0 },    { "0xFF & 0x0F", 15 },
        { "1 | 2", 3 },  { "6 ^ 3", 5 },       { "~0", -1 },       { "1 < 2", 1 },
        { "2 >= 3", 0 }, { "\"1 + 1\"", 2 },
    };
    size_t i;
    slv_eval_t ev;
    slv_reader_t *reader;
    unsigned char data[4] = { 1, 2, 3, 4 };

    memset (&ev, 0, sizeof (ev));
    reader = slv_reader_new_memory (data, sizeof (data));
    ev.reader = reader;

    for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
        gint64 v = 0;
        char *err = NULL;
        gboolean ok;

        ok = slv_eval_calc (&ev, NULL, cases[i].text, &v, &err);
        ck_assert_msg (ok, "%s: %s", cases[i].text, err);
        ck_assert_msg (v == cases[i].value, "%s = %lld, expected %lld", cases[i].text,
                       (long long) v, (long long) cases[i].value);
    }

    {
        gint64 v = 0;
        char *err = NULL;

        ck_assert (!slv_eval_calc (&ev, NULL, "1/0", &v, &err));
        ck_assert_str_eq (err, "division by zero");
        g_free (err);
        err = NULL;
        ck_assert (!slv_eval_calc (&ev, NULL, "1 +", &v, &err));
        g_free (err);
        err = NULL;
        ck_assert (!slv_eval_calc (&ev, NULL, "nolabel", &v, &err));
        ck_assert_str_eq (err, "unknown label 'nolabel'");
        g_free (err);
        err = NULL;
        ck_assert (slv_eval_calc (&ev, NULL, "@w", &v, &err));
        ck_assert_int_eq (v, 0x0201);
        ck_assert (slv_eval_calc (&ev, NULL, "@m16", &v, &err));
        ck_assert_int_eq (v, 0x0102);
        ck_assert (slv_eval_calc (&ev, NULL, "$size", &v, &err));
        ck_assert_int_eq (v, 4);
    }

    slv_reader_free (reader);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_shipped_files_parse);
    tcase_add_test (tc_core, test_zip_definitions);
    tcase_add_test (tc_core, test_dbf_table_columns);
    tcase_add_test (tc_core, test_exe_offset_ops);
    tcase_add_test (tc_core, test_inline_syntax);
    tcase_add_test (tc_core, test_parse_errors);
    tcase_add_test (tc_core, test_expressions);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
