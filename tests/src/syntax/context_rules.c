/*
   tests/src/syntax/context_rules.c -- unit tests for context and keyword matching

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

/* A rule set is only ever built from a real syntax file, so the tests write one.
   Nothing else here needs mc to be running: the scanner reads its bytes through
   the accessor below. */
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

/** Build a rule set out of @body, the part of a syntax file below its first line. */
static void
load (const char *body)
{
    char *lang;
    char *top;
    syntax_select_t sel;
    int res;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    write_file (lang, body);

    top = g_strdup_printf ("file .\\* Tested\ninclude %s\n", lang);
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

/** Load @body and give back what the parser made of it: 0, or the line it stopped at. */
static int
load_result (const char *body)
{
    char *lang;
    char *top;
    syntax_select_t sel;
    int res;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    write_file (lang, body);

    top = g_strdup_printf ("file .\\* Tested\ninclude %s\n", lang);
    write_file (syntax_file, top);
    g_free (top);
    g_free (lang);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    res = syntax_rules_load (syntax_file, &sel, &rules, NULL);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

/** Load a whole Syntax file of its own, rules and all. */
static int
load_toplevel (const char *content)
{
    syntax_select_t sel;

    write_file (syntax_file, content);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";

    return syntax_rules_load (syntax_file, &sel, &rules, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * What the rules make of @s, a character to a byte: the first letter of the
 * foreground color, or '.' where the byte is left alone.
 */
static char *
mask_len (const char *s, off_t len)
{
    syntax_scanner_t *sc;
    GString *out;
    off_t i;

    text = s;
    text_len = len;

    sc = syntax_scanner_new (rules, get_byte, NULL, text_len);
    out = g_string_sized_new ((gsize) text_len);

    for (i = 0; i < text_len; i++)
    {
        guint color;
        const char *fg = NULL;
        const char *bg;
        const char *attrs;

        color = syntax_rules_color_of (rules, syntax_state_at (sc, i));
        if (color != 0)
            syntax_rules_color_spec (rules, color, &fg, &bg, &attrs);

        g_string_append_c (out, fg == NULL || *fg == '\0' ? '.' : *fg);
    }

    syntax_scanner_free (sc);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static char *
mask (const char *s)
{
    return mask_len (s, (off_t) strlen (s));
}

/* --------------------------------------------------------------------------------------------- */

static void
check_mask_len (const char *s, off_t len, const char *expected)
{
    char *got = mask_len (s, len);

    ck_assert_msg (strcmp (got, expected) == 0, "text [%s]\n  expected [%s]\n  got      [%s]", s,
                   expected, got);
    g_free (got);
}

/* --------------------------------------------------------------------------------------------- */

static void
check_mask (const char *s, const char *expected)
{
    char *got = mask (s);

    ck_assert_msg (strcmp (got, expected) == 0, "text [%s]\n  expected [%s]\n  got      [%s]", s,
                   expected, got);
    g_free (got);
}

/* --------------------------------------------------------------------------------------------- */

static void
setup (void)
{
    tmpdir = g_dir_make_tmp ("mc-syntax-XXXXXX", NULL);
    ck_assert_ptr_nonnull (tmpdir);
    syntax_file = g_build_filename (tmpdir, "Syntax", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    char *lang;

    syntax_rules_unref (rules);
    rules = NULL;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    unlink (lang);
    g_free (lang);

    unlink (syntax_file);
    g_free (syntax_file);
    syntax_file = NULL;

    rmdir (tmpdir);
    g_free (tmpdir);
    tmpdir = NULL;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_plain_keyword)
{
    load ("context default\n"
          "  keyword int red\n");

    // a literal keyword, and a byte of it that does not match
    check_mask ("int x", "rrr..");
    check_mask ("ins x", ".....");
    // without "whole" the keyword is found inside a word as well
    check_mask ("print", "..rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_whole_word)
{
    load ("context default\n"
          "  keyword whole int red\n"
          "  keyword wholeleft do green\n"
          "  keyword wholeright if yellow\n");

    check_mask ("int x", "rrr..");
    // a word character on either side turns the match down
    check_mask ("print", ".....");
    check_mask ("into", "....");
    // wholeleft only looks behind, wholeright only ahead
    check_mask ("do done", "gg.gg..");
    check_mask ("if iffy", "yy.....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_line_start)
{
    load ("context default\n"
          "  keyword linestart end red\n");

    check_mask ("end\nthe end", "rrr........");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_star)
{
    load ("context default\n"
          "  keyword a*z red\n"
          "  keyword wholeright q* green\n");

    // the star swallows what lies between
    check_mask ("abcz .", "rrrr..");
    check_mask ("az .", "rr..");
    // and stops at the end of the line
    check_mask ("abc\nz", ".....");
    // a star at the end of a keyword runs to the end of the word
    check_mask ("qwe .", "gggg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_plus)
{
    load ("context default\n"
          "  keyword a+z red\n"
          "  keyword whole i+ green\n");

    // the plus eats the word characters between
    check_mask ("abcz .", "rrrr..");
    // and is satisfied by none of them, which is what the engine does today
    check_mask ("az .", "rr..");
    // a plus at the end takes the rest of the word
    check_mask ("ivar .", "gggg..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bracket)
{
    load ("context default\n"
          "  keyword a\\[0123456789\\]z red\n");

    // the bracket eats a run of the characters it names
    check_mask ("a12z .", "rrrr..");
    check_mask ("a1z .", "rrr..");
    // and nothing else
    check_mask ("abz .", ".....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_brace)
{
    load ("context default\n"
          "  keyword a\\{xy\\}z red\n");

    // the brace eats exactly one of the characters it names
    check_mask ("axz .", "rrr..");
    check_mask ("ayz .", "rrr..");
    check_mask ("abz .", ".....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_case_insensitive)
{
    load ("caseinsensitive\n"
          "context default\n"
          "  keyword int red\n");

    check_mask ("INT x", "rrr..");
    check_mask ("Int x", "rrr..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context)
{
    load ("context default\n"
          "context /\\* \\*/ green\n");

    // the delimiters belong to the context
    check_mask ("a /* b */ c", "..ggggggg..");
    // and it runs to the end of the text when the right side never comes
    check_mask ("a /* b", "..gggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_exclusive)
{
    load ("context default\n"
          "context exclusive /\\* \\*/ green\n");

    // exclusive colors what lies between the delimiters, not the delimiters
    check_mask ("a /* b */ c", "....ggg....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_linestart)
{
    load ("context default\n"
          "context linestart # \\n green\n");

    check_mask ("# one\nx # two\n", "gggggg........");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_in_context)
{
    load ("context default\n"
          "context /\\* \\*/ green\n"
          "  keyword TODO red\n");

    check_mask ("/* TODO x */", "gggrrrrggggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_plus_corners)
{
    load ("context default\n"
          "  keyword a+a red\n"
          "  keyword b+xy green\n"
          "  keyword c+z yellow\n");

    // the character after the plus is the one the keyword starts with
    check_mask ("aba .", "rrr..");
    // the plus lets go where the character at hand turns up further down the
    // keyword, and what follows no longer lines up
    check_mask ("bxy .", ".....");
    // and a word that ends before the plus found anything
    check_mask ("c z", "...");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_bracket_corners)
{
    load ("context default\n"
          "  keyword a\\[xy\\]\\[xy\\]z red\n"
          "  keyword q\\[wer green\n");

    // two sets in a row, the second one starting where the first gave up
    check_mask ("axyz .", "rrrr..");
    // a set that is never closed matches nothing
    check_mask ("qwer .", "......");
    // the character after the set is the last one the set ate
    load ("context default\n"
          "  keyword a\\[xy\\]yz red\n");
    check_mask ("axyz .", "rrrr..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_over_newline)
{
    load ("context default\n"
          "  keyword a\\nb red\n");

    // the keyword reaches over the line break, and is dropped on the next line
    check_mask ("a\nb", "rr.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_nul_byte)
{
    load ("context default\n"
          "  keyword int red\n");

    // a NUL byte is a byte the rules are not applied at; what follows is read on
    check_mask_len ("a\0int", 5, "..rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_exclusive_empty)
{
    load ("context default\n"
          "context exclusive < > green\n");

    // the right delimiter arrives where the left one ended
    check_mask ("a<>b", "....");
    check_mask ("a<x>b", "..g..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_newline_keyword_in_newline_context)
{
    load ("context default\n"
          "context # \\n green\n"
          "  keyword TODO\\n red\n");

    // both the context and the keyword end with the line break, and the keyword
    // gives it back so that the context is not carried into the next line
    check_mask ("# TODO\nx", "ggrrrrg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_at_context_start)
{
    load ("context default\n"
          "context < > green\n"
          "  keyword <x red\n");

    // the context turns on and a keyword of it starts on the same byte
    check_mask ("a<xy>", ".rrgg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_wholechars)
{
    // the characters a word is made of are the file's to choose
    load ("wholechars ab\n"
          "context default\n"
          "  keyword whole ab red\n");
    check_mask ("ab ", "rr.");
    check_mask ("aba", "...");

    load ("wholechars left xy\n"
          "wholechars right z\n"
          "context default\n"
          "  keyword whole q red\n");
    // only x and y stop a word on the left, only z on the right
    check_mask ("aqa", ".r.");
    check_mask ("xq", "..");
    check_mask ("qz", "..");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_word_borders)
{
    load ("context default\n"
          "context whole ab cd green\n");
    // the delimiters are words of their own
    check_mask ("ab cd", "ggggg");
    check_mask ("xab cd", "......");

    load ("context default\n"
          "context wholeleft ab cd green\n");
    check_mask ("ab cd", "ggggg");

    load ("context default\n"
          "context wholeright ab cd green\n");
    check_mask ("ab cd", "ggggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_context_linestart_right)
{
    load ("context default\n"
          "context < linestart > green\n");

    // the right delimiter counts only at the start of a line
    check_mask ("a<b>c\n>d", ".gggggg.");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_spellcheck_and_comments)
{
    load ("# a comment\n"
          "\n"
          "context default\n"
          "  spellcheck\n"
          "  keyword int red\n");

    check_mask ("int", "rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_keyword_color_inherited)
{
    load ("context default green\n"
          "  keyword int red\n"
          "  keyword char\n");

    // a keyword with no color of its own falls back to the context's
    check_mask ("int char", "rrrggggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_define)
{
    load ("define types int char\n"
          "context default\n"
          "  keyword whole int red\n"
          "  keyword whole char red\n");

    check_mask ("int char", "rrr.rrrr");

    // a define given twice keeps the last
    load ("define types int\n"
          "define types char\n"
          "context default\n"
          "  keyword whole char green\n");
    check_mask ("char", "gggg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_parse_errors)
{
    // the parser reports the line it stopped at, and never a dialog
    ck_assert_int_eq (load_result ("context\n"), 1);
    ck_assert_int_eq (load_result ("context default\n  keyword\n"), 2);
    ck_assert_int_eq (load_result ("wholechars\n"), 1);
    ck_assert_int_eq (load_result ("context something else\n"), 1);
    ck_assert_int_eq (load_result ("keyword int red\n"), 1);
    ck_assert_int_eq (load_result ("define\n"), 1);
    ck_assert_int_eq (load_result ("define one\n"), 1);
    ck_assert_int_eq (load_result ("line-local extra\n"), 1);
    ck_assert_int_eq (load_result ("context default\nline-local\n"), 2);
    ck_assert_int_eq (load_result ("number 16 red\n"), 1);
    ck_assert_int_eq (load_result ("line-local\nnumber\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nnumber 0 red\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nnumber 16zz red\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nnumber 16\n"), 2);
    ck_assert_int_eq (load_result ("string \" red\n"), 1);
    ck_assert_int_eq (load_result ("line-local\nstring\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nstring xx red\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nstring x red\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nstring \"\n"), 2);
    ck_assert_int_eq (load_result ("symbols ,. red\n"), 1);
    ck_assert_int_eq (load_result ("line-local\nsymbols\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nsymbols ,.\n"), 2);
    ck_assert_int_eq (load_result ("include\n"), 1);
    ck_assert_int_eq (load_result ("include /nowhere/at/all.syntax\n"), 1);
    ck_assert_int_eq (load_result ("context default\n  spellcheck extra\n"), 0);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rules_without_include)
{
    // the rules can stand in the Syntax file itself
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  keyword int red\n"),
                      0);
    check_mask ("int", "rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_include_that_is_not_there)
{
    // an include of a file that cannot be opened stops at its line
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "include /nowhere/at/all.syntax\n"),
                      2);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_second_file_line_ends_the_rules)
{
    // a second "file" line belongs to the next rule set, not to this one
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  keyword int red\n"
                                     "file \\.c$ C\n"
                                     "context default\n"),
                      0);
    check_mask ("int", "rrr");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_full_colors)
{
    // a color is a foreground, a background and attributes, for contexts as well
    // as for keywords
    load ("context default green black bold\n"
          "  keyword int red black underline\n");

    check_mask ("int x", "rrrgg");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_more_parse_errors)
{
    ck_assert_int_eq (load_result ("line-local\ncontext default\n"), 2);
    ck_assert_int_eq (load_result ("line-local\nspellcheck\n"), 2);
    ck_assert_int_eq (load_result ("spellcheck\n"), 1);
    ck_assert_int_eq (load_result ("line-local\nkeyword int red\n"), 2);
    ck_assert_int_eq (load_result ("context default\n  keyword linestart whole int red\n"), 2);
    ck_assert_int_eq (load_result ("context default\n  keyword linestart\n"), 2);
    ck_assert_int_eq (load_result ("nonsense here\n"), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_no_syntax_file_at_all)
{
    syntax_select_t sel;
    char *missing;

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";

    missing = g_build_filename (tmpdir, "not-here", (char *) NULL);
    // neither the file asked for nor the one mc ships with
    ck_assert_int_eq (syntax_rules_load (missing, &sel, &rules, NULL), -1);
    g_free (missing);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_type_chosen_by_caller)
{
    syntax_select_t sel;

    write_file (syntax_file,
                "\n"
                "# the first lines say nothing\n"
                "file \\.c$ TheC\n"
                "context default\n"
                "  keyword int red\n"
                "file \\.sh$ TheShell\n"
                "context default\n"
                "  keyword echo green\n");

    sel.type = "TheShell";
    sel.filename = "whatever.c";
    sel.first_line = "";
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 0);
    ck_assert_str_eq (syntax_rules_type (rules), "TheShell");

    // the name it was asked for wins over the name of the file
    check_mask ("echo int", "gggg....");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_type_chosen_by_first_line)
{
    syntax_select_t sel;

    write_file (syntax_file,
                "file \\.c$ TheC\n"
                "context default\n"
                "  keyword int red\n"
                "file unmatchable TheShell ^#!.\\*sh\n"
                "context default\n"
                "  keyword echo green\n");

    // the name matches nothing, the first line of the text does
    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "#!/bin/sh";
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 0);
    ck_assert_str_eq (syntax_rules_type (rules), "TheShell");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_list_of_types)
{
    GPtrArray *names;

    write_file (syntax_file,
                "file \\.c$ TheC\n"
                "context default\n"
                "file \\.sh$ TheShell\n"
                "context default\n");

    names = g_ptr_array_new_with_free_func (g_free);
    ck_assert_int_eq (syntax_rules_list_types (syntax_file, names), 0);
    ck_assert_int_eq ((int) names->len, 2);
    ck_assert_str_eq (g_ptr_array_index (names, 0), "TheC");
    ck_assert_str_eq (g_ptr_array_index (names, 1), "TheShell");
    g_ptr_array_free (names, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_include_before_any_file_line)
{
    char *lang;
    syntax_select_t sel;

    lang = g_build_filename (tmpdir, "tested.syntax", (char *) NULL);
    write_file (lang, "context default\n  keyword int red\n");
    g_free (lang);

    // an include above every "file" line is the rule set, whatever the file is
    write_file (syntax_file, "include tested.syntax\n");

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    // the include is looked for where mc keeps its own, so this one is not found
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 1);

    ck_assert_int_eq (load_toplevel ("include\n"), 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_include_that_opens)
{
    char *lang;
    char *content;
    syntax_select_t sel;

    lang = g_build_filename (tmpdir, "included.syntax", (char *) NULL);
    write_file (lang, "context default\n  keyword int red\n");

    // an include above every "file" line names the rule set for any file
    content = g_strdup_printf ("include %s\n", lang);
    write_file (syntax_file, content);
    g_free (content);
    g_free (lang);

    sel.type = NULL;
    sel.filename = "whatever.txt";
    sel.first_line = "";
    ck_assert_int_eq (syntax_rules_load (syntax_file, &sel, &rules, NULL), 0);
    check_mask ("int", "rrr");

    lang = g_build_filename (tmpdir, "included.syntax", (char *) NULL);
    unlink (lang);
    g_free (lang);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_rules_without_any_keyword)
{
    // a rule set of nothing but its default context colors nothing, so it is
    // thrown away and the caller told there is none
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"),
                      -1);
    ck_assert_ptr_null (rules);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_broken_file_line) { ck_assert_int_eq (load_toplevel ("file onlyone\n"), 1); }
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_error_inside_the_chosen_rules)
{
    // the line the parser stopped at is counted from the top of the Syntax file
    ck_assert_int_eq (load_toplevel ("file .\\* Tested\n"
                                     "context default\n"
                                     "  nonsense\n"),
                      3);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

static void
add_tests (TCase *tc_core)
{
    tcase_add_test (tc_core, test_plain_keyword);
    tcase_add_test (tc_core, test_whole_word);
    tcase_add_test (tc_core, test_line_start);
    tcase_add_test (tc_core, test_star);
    tcase_add_test (tc_core, test_plus);
    tcase_add_test (tc_core, test_bracket);
    tcase_add_test (tc_core, test_brace);
    tcase_add_test (tc_core, test_case_insensitive);
    tcase_add_test (tc_core, test_context);
    tcase_add_test (tc_core, test_context_exclusive);
    tcase_add_test (tc_core, test_context_linestart);
    tcase_add_test (tc_core, test_keyword_in_context);
    tcase_add_test (tc_core, test_plus_corners);
    tcase_add_test (tc_core, test_bracket_corners);
    tcase_add_test (tc_core, test_keyword_over_newline);
    tcase_add_test (tc_core, test_nul_byte);
    tcase_add_test (tc_core, test_exclusive_empty);
    tcase_add_test (tc_core, test_newline_keyword_in_newline_context);
    tcase_add_test (tc_core, test_keyword_at_context_start);
    tcase_add_test (tc_core, test_wholechars);
    tcase_add_test (tc_core, test_context_word_borders);
    tcase_add_test (tc_core, test_context_linestart_right);
    tcase_add_test (tc_core, test_spellcheck_and_comments);
    tcase_add_test (tc_core, test_keyword_color_inherited);
    tcase_add_test (tc_core, test_define);
    tcase_add_test (tc_core, test_parse_errors);
    tcase_add_test (tc_core, test_rules_without_include);
    tcase_add_test (tc_core, test_include_that_is_not_there);
    tcase_add_test (tc_core, test_second_file_line_ends_the_rules);
    tcase_add_test (tc_core, test_full_colors);
    tcase_add_test (tc_core, test_more_parse_errors);
    tcase_add_test (tc_core, test_no_syntax_file_at_all);
    tcase_add_test (tc_core, test_type_chosen_by_caller);
    tcase_add_test (tc_core, test_type_chosen_by_first_line);
    tcase_add_test (tc_core, test_list_of_types);
    tcase_add_test (tc_core, test_include_before_any_file_line);
    tcase_add_test (tc_core, test_include_that_opens);
    tcase_add_test (tc_core, test_rules_without_any_keyword);
    tcase_add_test (tc_core, test_broken_file_line);
    tcase_add_test (tc_core, test_error_inside_the_chosen_rules);
}

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);
    add_tests (tc_core);

    return mctest_run_all (tc_core);
}
