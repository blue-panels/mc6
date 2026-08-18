/*
   Midnight Commander - the shell prompt in front of the command line.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

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

/** \file wprompt.c
 *  \brief Source: the shell prompt in front of the command line
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "command.h"  // cmdline
#include "wprompt.h"

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/* Whether the shell has a prompt of its own on screen for us to show. */
gboolean
wprompt_from_shell (const WPrompt *p)
{
    return (p != NULL && p->term != NULL && mcterm_osc7_capable (p->term)
            && mcterm_shell_at_prompt (p->term));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
wprompt_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WPrompt *p = WPROMPT (w);

    switch (msg)
    {
    case MSG_DRAW:
        if (wprompt_from_shell (p))
        {
            /* The shell's own row, colors and all. Where it asked for no background, it gets
               the terminal's: the colors it chose were chosen for a terminal, and on the
               file manager's own background a blue path on blue would disappear. */
            mcterm_draw_prompt_row (p->term, w->rect.y, "mcterm", MCTERM_NORMAL_COLOR);
        }
        else if (p->text != NULL)
        {
            tty_setcolor (CORE_DEFAULT_COLOR);
            widget_gotoyx (w, 0, 0);
            tty_print_string (str_fit_to_term (p->text, w->rect.cols, J_LEFT));
        }
        return MSG_HANDLED;

    case MSG_DESTROY:
        g_free (p->text);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* A click on the prompt is meant for the command line behind it. */
static void
wprompt_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    (void) w;
    (void) event;

    if (msg == MSG_MOUSE_CLICK && cmdline != NULL
        && widget_get_options (WIDGET (cmdline), WOP_SELECTABLE))
        widget_select (WIDGET (cmdline));
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WPrompt *
wprompt_new (int y, int x, const char *text)
{
    WRect r = { y, x, 1, 1 };
    WPrompt *p;
    Widget *w;

    if (text != NULL)
        r.cols = MAX (str_term_width1 (text), 1);

    p = g_new0 (WPrompt, 1);
    w = WIDGET (p);
    widget_init (w, &r, wprompt_callback, wprompt_mouse_callback);

    p->text = g_strdup (text);
    p->term = NULL;

    return p;
}

/* --------------------------------------------------------------------------------------------- */

void
wprompt_set_text (WPrompt *p, const char *text)
{
    if (p == NULL)
        return;

    g_free (p->text);
    p->text = g_strdup (text);
    widget_draw (WIDGET (p));
}

/* --------------------------------------------------------------------------------------------- */

void
wprompt_set_terminal (WPrompt *p, WMcTerm *term)
{
    if (p != NULL)
        p->term = term;
}

/* --------------------------------------------------------------------------------------------- */

int
wprompt_width (const WPrompt *p)
{
    if (p == NULL)
        return 0;

    if (wprompt_from_shell (p))
    {
        /* Where the shell left its cursor is where the prompt ends and typing begins. A prompt
           of more than one row shows its last one: the rest stays in the terminal. */
        const int col = mcterm_cursor_col (p->term);

        return MAX (col, 0);
    }

    return (p->text != NULL) ? str_term_width1 (p->text) : 0;
}

/* --------------------------------------------------------------------------------------------- */
