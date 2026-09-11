/*
   tests/src/syntax/line_local.c -- unit tests for line-local rules

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

#define TEST_SUITE_NAME "/src/syntax"

#include "tests/mctest.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lib/global.h"
#include "src/syntax/syntax.h"

/* The rule set is built from a real syntax file, because that is the only way one
   is ever built; nothing else here needs mc to be running. */
static char *tmpdir = NULL;
static char *syntax_file = NULL;
static syntax_rules_t *rules = NULL;

/* the text under test, and the byte source over it */
static const char *text = NULL;
static off_t text_len = 0;

/* --------------------------------------------------------------------------------------------- */

static int
get_byte (void *data, off_t byte_index)
{
    (void) data;

    return byte_index < 0 || byte_index >= text_len ? '\n' : (unsigned char) text[byte_index];
}

/* --------------------------------------------------------------------------------------------- */

static void
write_file (const char *path, const char *content)
{
    FILE *f = fopen (path, "w");

    ck_assert_msg (f != NULL, "cannot write %s", path);
    fputs (content, f);
    fclose (f);
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    char *lang;
    char *top;
    syntax_select_t sel;
    int res;

    tmpdir = g_dir_make_tmp ("mc-syntax-XXXXXX", NULL);
    ck_assert_ptr_nonnull (tmpdir);

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    write_file (lang,
                "line-local\n"
                "number 16 brightcyan\n"
                "string \" brightgreen\n"
                "string ' brightgreen\n"
                "symbols {}[]()/;,. yellow\n");

    top = g_strdup_printf ("file .\\* Tested\ninclude %s\n", lang);
    syntax_file = g_build_filename (tmpdir, "Syntax", (char *) NULL);
    write_file (syntax_file, top);

    g_free (top);
    g_free (lang);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    res = syntax_rules_load (syntax_file, &sel, &rules, NULL);
    ck_assert_int_eq (res, 0);
    ck_assert_ptr_nonnull (rules);
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    syntax_rules_unref (rules);
    rules = NULL;
    if (syntax_file != NULL)
        unlink (syntax_file);
    g_free (syntax_file);
    syntax_file = NULL;
    if (tmpdir != NULL)
    {
        char *lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);

        unlink (lang);
        g_free (lang);
        rmdir (tmpdir);
    }
    g_free (tmpdir);
    tmpdir = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/** Colors of every byte of @s, as the line-local rules see them. */
static guint *
colorize (const char *s)
{
    guint *out;
    syntax_line_local_state_t st;
    off_t i, line_start = 0;

    text = s;
    text_len = (off_t) strlen (s);
    out = g_new0 (guint, text_len + 1);

    syntax_line_local_reset (&st, 0);
    for (i = 0; i < text_len; i++)
    {
        if (i != 0 && s[i - 1] == '\n')
        {
            line_start = i;
            syntax_line_local_reset (&st, line_start);
        }
        out[i] = syntax_line_local_color (rules, &st, get_byte, NULL, i);
    }

    return out;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_line_local_is_recognized)
{
    ck_assert_msg (syntax_rules_is_line_local (rules), "the rule set must be line-local");
    /* three distinct colors plus the "no color" slot at 0: the two string rules
       name the same brightgreen and are interned to one entry */
    ck_assert_int_eq (syntax_rules_color_count (rules), 4);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_numbers_quotes_symbols)
{
    static const char s[] = "12 \"ab\" 'c' {x};\n";
    guint *c;
    guint num, str, sym;

    c = colorize (s);

    num = c[0];
    str = c[strchr (s, '"') - s];
    sym = c[strchr (s, '{') - s];

    ck_assert_msg (num != 0, "a number must be colored");
    ck_assert_msg (str != 0, "a quoted string must be colored");
    ck_assert_msg (sym != 0, "a symbol must be colored");
    ck_assert_msg (num != str && str != sym && num != sym, "the three must differ");

    ck_assert_int_eq (c[1], num);                        // both digits
    ck_assert_int_eq (c[2], 0u);                         // the space between
    ck_assert_int_eq (c[strchr (s, '"') - s + 3], str);  // the closing quote
    ck_assert_int_eq (c[strchr (s, 'x') - s], 0u);       // plain text inside braces
    ck_assert_int_eq (c[strchr (s, ';') - s], sym);

    g_free (c);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_state_does_not_cross_lines)
{
    static const char s[] = "\"unterminated\nplain 7\n";
    guint *c;
    const char *second;

    c = colorize (s);
    second = strchr (s, '\n') + 1;

    /* the open quote runs to the end of its line and stops there; that is the
       whole point of line-local rules */
    ck_assert_msg (c[0] != 0, "the quote itself is colored");
    ck_assert_int_eq (c[second - s], 0u);
    ck_assert_msg (c[second - s + 6] != 0, "the number on the next line is colored");

    g_free (c);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_number_length_limit)
{
    /* the rule says 16; a longer run of digits is not a number */
    static const char s[] = "1234567890123456 12345678901234567\n";
    guint *c;

    c = colorize (s);
    ck_assert_msg (c[0] != 0, "sixteen digits are a number");
    ck_assert_int_eq (c[17], 0u);

    g_free (c);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_line_local_is_recognized);
    tcase_add_test (tc_core, test_numbers_quotes_symbols);
    tcase_add_test (tc_core, test_state_does_not_cross_lines);
    tcase_add_test (tc_core, test_number_length_limit);

    return mctest_run_all (tc_core);
}
