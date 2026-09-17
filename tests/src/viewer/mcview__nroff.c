/*
   src/viewer - unit tests for the nroff mode

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

#define TEST_SUITE_NAME "/src/viewer/nroff"

#include "tests/mctest.h"

#include "lib/skin.h"
#include "lib/strutil.h"

#include "src/viewer/internal.h"

/*** fixtures ************************************************************************************/

/* distinct skin colors, so that a test can tell which one a character got */
#define NORMAL         101
#define BOLD           102
#define UNDERLINE      103
#define BOLD_UNDERLINE 104
#define HEADING        105

static WView view;
static mcview_state_machine_t state;

/* @Before */
static void
setup (void)
{
    str_init_strings (NULL);

    memset (&view, 0, sizeof (view));
    view.converter = str_cnv_from_term;
    view.force_max = -1;
    view.utf8 = TRUE;
    view.mode_flags.nroff = TRUE;

    VIEWER_NORMAL_COLOR = NORMAL;
    VIEWER_BOLD_COLOR = BOLD;
    VIEWER_UNDERLINED_COLOR = UNDERLINE;
    VIEWER_BOLD_UNDERLINED_COLOR = BOLD_UNDERLINE;
    VIEWER_HEADING_COLOR = HEADING;
}

/* @After */
static void
teardown (void)
{
    mcview_close_datasource (&view);
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

static void
load (const char *text)
{
    mcview_close_datasource (&view);
    mcview_set_datasource_string (&view, text);
    mcview_state_machine_init (&state, 0);
}

/* --------------------------------------------------------------------------------------------- */

/* the next character on screen and its color */
static int
next (int *color)
{
    int cs[1 + MAX_COMBINING_CHARS];

    ck_assert_int_gt (
        mcview_next_combining_char_sequence (&view, &state, cs, G_N_ELEMENTS (cs), color), 0);
    return cs[0];
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_backspace_styles)
{
    int color;

    load ("a b\bb _\bc _\bd\bd e\be\be");

    ck_assert_int_eq (next (&color), 'a');
    ck_assert_int_eq (color, NORMAL);
    ck_assert_int_eq (next (&color), ' ');
    ck_assert_int_eq (next (&color), 'b');
    ck_assert_int_eq (color, BOLD);
    ck_assert_int_eq (next (&color), ' ');
    ck_assert_int_eq (next (&color), 'c');
    ck_assert_int_eq (color, UNDERLINE);
    ck_assert_int_eq (next (&color), ' ');
    ck_assert_int_eq (next (&color), 'd');
    ck_assert_int_eq (color, BOLD_UNDERLINE);
    ck_assert_int_eq (next (&color), ' ');
    ck_assert_int_eq (next (&color), 'e');
    ck_assert_int_eq (color, HEADING);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_escape_sequences_are_not_shown)
{
    // an italic color pair needs a skin, so only the SGR state is checked
    load ("\033[3mi\033[23m ");

    ck_assert_int_eq (next (NULL), 'i');
    ck_assert (state.ansi.italic);
    ck_assert_int_eq (next (NULL), ' ');
    ck_assert (!state.ansi.italic);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sgr_and_backspaces_style_one_character)
{
    int color;

    // SGR bold over an underlined letter, SGR underline over a bold one
    load ("\033[1m_\ba\033[22m \033[4mb\bb\033[24m");

    ck_assert_int_eq (next (&color), 'a');
    ck_assert_int_eq (color, BOLD_UNDERLINE);
    ck_assert_int_eq (next (&color), ' ');
    ck_assert_int_eq (color, NORMAL);
    ck_assert_int_eq (next (&color), 'b');
    ck_assert_int_eq (color, BOLD_UNDERLINE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_sgr_inside_a_backspace_sequence)
{
    int color;

    load ("x\033[1m\bx y");

    ck_assert_int_eq (next (&color), 'x');
    ck_assert_int_eq (color, BOLD);
    ck_assert_int_eq (next (&color), ' ');
    ck_assert_int_eq (color, BOLD);
    ck_assert_int_eq (next (&color), 'y');
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_styled_heading_is_bold)
{
    int color;

    load ("\033[4mh\bh\bh\033[24m");

    ck_assert_int_eq (next (&color), 'h');
    ck_assert_int_eq (color, BOLD_UNDERLINE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_overstrike_glyphs)
{
    int color;

    load ("+\bo '\be \"\bU ,\bc |\b^ -\bL a\bb");

    ck_assert_int_eq (next (&color), 0x2022);
    ck_assert_int_eq (color, NORMAL);
    ck_assert_int_eq (next (NULL), ' ');
    ck_assert_int_eq (next (NULL), 0x00E9);
    ck_assert_int_eq (next (NULL), ' ');
    ck_assert_int_eq (next (NULL), 0x00DC);
    ck_assert_int_eq (next (NULL), ' ');
    ck_assert_int_eq (next (NULL), 0x00E7);
    ck_assert_int_eq (next (NULL), ' ');
    ck_assert_int_eq (next (NULL), 0x2191);
    ck_assert_int_eq (next (NULL), ' ');
    ck_assert_int_eq (next (NULL), 0x00A3);
    ck_assert_int_eq (next (NULL), ' ');
    ck_assert_int_eq (next (NULL), 'b');
    ck_assert_int_eq (state.offset, 27);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_backspace_styles);
    tcase_add_test (tc_core, test_escape_sequences_are_not_shown);
    tcase_add_test (tc_core, test_sgr_and_backspaces_style_one_character);
    tcase_add_test (tc_core, test_sgr_inside_a_backspace_sequence);
    tcase_add_test (tc_core, test_styled_heading_is_bold);
    tcase_add_test (tc_core, test_overstrike_glyphs);

    return mctest_run_all (tc_core);
}
