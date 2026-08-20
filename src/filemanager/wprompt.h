/** \file wprompt.h
 *  \brief Header: the shell prompt in front of the command line
 */

#ifndef MC__WPROMPT_H
#define MC__WPROMPT_H

#include "lib/global.h"
#include "lib/widget.h"

#include "src/mcterm/mcterm.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define WPROMPT(x) ((WPrompt *) (x))

/*** structures declarations (and typedefs of structures)*****************************************/

/**
 * The row in front of the command line.
 *
 * With a terminal to read it from, the prompt is drawn from the terminal's own cells: the shell
 * has already worked out its colors, its git branch and its exit status, and those cells are the
 * only place where the result of that work exists. The widget keeps no copy of them.
 *
 * Without a terminal - a shell that speaks no protocol, or none running yet - it draws the plain
 * text it was given.
 */
typedef struct
{
    Widget widget;
    char *text;     // what to draw when the terminal has nothing to say
    WMcTerm *term;  // not owned
} WPrompt;

/*** declarations of public functions ************************************************************/

WPrompt *wprompt_new (int y, int x, const char *text);
void wprompt_set_text (WPrompt *p, const char *text);
/* The terminal to read the prompt from, or NULL to fall back to the text. */
void wprompt_set_terminal (WPrompt *p, WMcTerm *term);
/* Columns the prompt takes up, so that the command line knows where to start. */
int wprompt_width (const WPrompt *p);
/* Whether the row on screen is the shell's own, rather than text of ours. */
gboolean wprompt_from_shell (const WPrompt *p);

/*** inline functions ****************************************************************************/

#endif /* MC__WPROMPT_H */
