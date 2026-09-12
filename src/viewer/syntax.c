/*
   Internal file viewer for the M-Commander
   Syntax colors for the shown text

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

/** \file
 *  \brief Source: syntax colors for the internal viewer
 *
 *  Unlike the diff viewer, which colors everything once while it parses, the
 *  viewer asks as it draws: it shows files it has not read to the end, and the
 *  user may never scroll far enough to care.
 *
 *  That costs something, because the state at an offset is a function of every
 *  byte before it: reaching the end of a large file means reading all of it.
 *  Above mcview_syntax_limit the text therefore gets the line-local rules, which
 *  color numbers, quotes and punctuation and keep no state between lines, so any
 *  line can be colored without looking at the ones above it.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/fileloc.h"  // EDIT_SYNTAX_FILE
#include "lib/mcconfig.h"
#include "lib/skin.h"
#include "lib/util.h"

#include "src/syntax/tty-backend.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* Full rules above this size would mean reading the whole file to color its end. */
static const off_t mcview_syntax_limit = 4 * 1024 * 1024;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
mcview_syntax_get_byte (void *data, off_t byte_index)
{
    int c;

    return mcview_get_byte ((WView *) data, byte_index, &c) ? c : '\n';
}

/* --------------------------------------------------------------------------------------------- */

/** The first line, which is one of the two things a rule set is chosen by. */

static const char *
mcview_syntax_first_line (WView *view)
{
    static char s[256];
    size_t i;

    for (i = 0; i < sizeof (s) - 1; i++)
    {
        int c;

        if (!mcview_get_byte (view, (off_t) i, &c) || c == '\n')
            break;
        s[i] = (char) c;
    }

    s[i] = '\0';

    return s;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
mcview_syntax_unload (WView *view)
{
    view->ll_bol = -1;
    view->ll_at = -1;
    syntax_palette_free (view->palette);
    view->palette = NULL;
    syntax_scanner_free (view->syntax);
    view->syntax = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
mcview_syntax_load (WView *view)
{
    syntax_rules_t *rules = NULL;
    syntax_select_t sel;
    char *syntax_file;
    char *error_file = NULL;
    off_t size;

    mcview_syntax_unload (view);

    /* ANSI mode is the other reading of the same bytes: there the text says what
       color it wants, and the escapes have to be eaten rather than shown. */
    if (!view->mode_flags.highlight || view->mode_flags.ansi || !tty_use_colors ()
        || view->mode_flags.hex || view->mode_flags.terminal)
        return;

    // a growing source has no size to speak of yet; treat it as large
    size = mcview_get_filesize (view);

    sel.type = size > mcview_syntax_limit ? "unknown" : NULL;
    sel.filename = view->filename_vpath != NULL ? vfs_path_as_str (view->filename_vpath) : NULL;
    sel.first_line = mcview_syntax_first_line (view);

    if (sel.filename == NULL && sel.first_line[0] == '\0')
        return;

    syntax_file = mc_config_get_full_path (EDIT_SYNTAX_FILE);
    if (syntax_rules_load (syntax_file, &sel, &rules, &error_file) == 0)
    {
        view->syntax = syntax_scanner_new (rules, mcview_syntax_get_byte, view, size);
        view->palette = syntax_palette_new (rules, syntax_tty_alloc_color, syntax_tty_release_color,
                                            (void *) "editor", VIEWER_NORMAL_COLOR);
        syntax_rules_unref (rules);
    }

    g_free (error_file);  // a broken syntax file is the editor's business to report
    g_free (syntax_file);
}

/* --------------------------------------------------------------------------------------------- */

int
mcview_syntax_color (WView *view, off_t offset)
{
    const syntax_rules_t *rules;

    if (view->syntax == NULL)
        return VIEWER_NORMAL_COLOR;

    rules = syntax_scanner_rules (view->syntax);

    /* the source grows while it is being read, and the scanner refuses to look
       past what it believes the size to be */
    syntax_scanner_set_size (view->syntax, mcview_get_filesize (view));

    if (syntax_rules_is_line_local (rules))
    {
        /* Line-local state is cheap but still has to be walked up from the start
           of the line; drawing goes left to right, so carry it between calls and
           only rewind when the caller jumps. */
        if (view->ll_bol >= 0 && offset == view->ll_at)
        {
            /* The next byte of the line being drawn: only a newline just passed
               can have started a new one.  Looking the start of the line up here
               would walk the whole line back for every character on it. */
            if (offset > 0 && mcview_syntax_get_byte (view, offset - 1) == '\n')
            {
                syntax_line_local_reset (&view->ll, offset);
                view->ll_bol = offset;
            }
        }
        else
        {
            const off_t bol = mcview_bol (view, offset, 0);

            if (bol != view->ll_bol || offset < view->ll_at)
            {
                syntax_line_local_reset (&view->ll, bol);
                view->ll_bol = bol;
                view->ll_at = bol;
            }
        }

        while (view->ll_at < offset)
        {
            (void) syntax_line_local_color (rules, &view->ll, mcview_syntax_get_byte, view,
                                            view->ll_at);
            view->ll_at++;
        }
        view->ll_at = offset + 1;

        return syntax_palette_get (
            view->palette,
            syntax_line_local_color (rules, &view->ll, mcview_syntax_get_byte, view, offset));
    }

    return syntax_palette_get (
        view->palette, syntax_rules_color_of (rules, syntax_state_at (view->syntax, offset)));
}

/* --------------------------------------------------------------------------------------------- */
