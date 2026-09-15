/*
   lib/widget - unit tests for the state a group hands to its widgets

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

#define TEST_SUITE_NAME "lib/widget/group"

#include "tests/mctest.h"

#include "lib/widget.h"

/* --------------------------------------------------------------------------------------------- */

static int focus_msgs = 0;
static int unfocus_msgs = 0;

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
child_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_FOCUS:
        focus_msgs++;
        break;
    case MSG_UNFOCUS:
        unfocus_msgs++;
        break;
    default:
        break;
    }

    return widget_default_callback (w, sender, msg, parm, data);
}

/* --------------------------------------------------------------------------------------------- */

static WGroup *
group_with_one_widget (Widget **child)
{
    WGroup *g;
    WRect r;

    g = g_new0 (WGroup, 1);
    rect_init (&r, 0, 0, 20, 20);
    group_init (g, &r, group_default_callback, NULL);

    *child = g_new0 (Widget, 1);
    rect_init (&r, 0, 0, 5, 5);
    widget_init (*child, &r, child_callback, NULL);
    widget_set_options (*child, WOP_SELECTABLE, TRUE);
    group_add_widget (g, *child);

    send_message (g, NULL, MSG_INIT, 0, NULL);
    widget_set_state (WIDGET (g), WST_ACTIVE, TRUE);
    widget_set_state (WIDGET (g), WST_FOCUSED, TRUE);

    focus_msgs = 0;
    unfocus_msgs = 0;

    return g;
}

/* --------------------------------------------------------------------------------------------- */

/* A state the group holds for itself must leave the focus of its current widget alone. */
START_TEST (test_own_state_keeps_the_focus)
{
    WGroup *g;
    Widget *child;

    g = group_with_one_widget (&child);

    widget_idle (WIDGET (g), TRUE);
    widget_idle (WIDGET (g), FALSE);

    ck_assert_msg (focus_msgs == 0, "MSG_FOCUS sent %d times", focus_msgs);
    ck_assert_msg (unfocus_msgs == 0, "MSG_UNFOCUS sent %d times", unfocus_msgs);
    ck_assert_msg (widget_get_state (child, WST_FOCUSED), "the widget lost the focus");

    widget_destroy (WIDGET (g));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

// The focus itself still reaches the current widget.
START_TEST (test_focus_reaches_the_current_widget)
{
    WGroup *g;
    Widget *child;

    g = group_with_one_widget (&child);

    widget_set_state (WIDGET (g), WST_FOCUSED, FALSE);
    ck_assert_msg (unfocus_msgs == 1, "MSG_UNFOCUS sent %d times", unfocus_msgs);
    ck_assert_msg (!widget_get_state (child, WST_FOCUSED), "the widget still holds the focus");

    widget_set_state (WIDGET (g), WST_FOCUSED, TRUE);
    ck_assert_msg (focus_msgs == 1, "MSG_FOCUS sent %d times", focus_msgs);
    ck_assert_msg (widget_get_state (child, WST_FOCUSED), "the widget did not take the focus");

    widget_destroy (WIDGET (g));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_own_state_keeps_the_focus);
    tcase_add_test (tc_core, test_focus_reaches_the_current_widget);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
