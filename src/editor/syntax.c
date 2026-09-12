/*
   Editor syntax highlighting: the editor's end of the lexical scanner.

   Copyright (C) 1996-2025
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Paul Sheer, 1998
   Leonard den Ottolander <leonard den ottolander nl>, 2005, 2006
   Egmont Koblinger <egmont@gmail.com>, 2010
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013, 2014, 2021
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the M-Commander
   a fork of GNU Midnight Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file
 *  \brief Source: editor syntax highlighting
 *
 *  What is left here now that the scanner lives in src/syntax: the byte source,
 *  the color backend that turns the scanner's symbolic colors into terminal
 *  pairs, the dialog for choosing a syntax by hand, and the wrappers the rest of
 *  the editor has always called.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include "lib/global.h"
#include "lib/fileloc.h"     // EDIT_SYNTAX_FILE
#include "lib/glibcompat.h"  // g_ptr_array_sort_values() on GLib before 2.76
#include "lib/mcconfig.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/tty/color.h"
#include "lib/util.h"
#include "lib/widget.h"  // Listbox, message()

#include "src/util.h"  // file_error_message()

#include "src/syntax/tty-backend.h"

#include "edit-impl.h"
#include "editwidget.h"

/*** global variables ****************************************************************************/

gboolean auto_syntax = TRUE;

/*** file scope macro definitions ****************************************************************/

#define MAX_ENTRY_LEN  40
#define LIST_LINES     14
#define N_DFLT_ENTRIES 2

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
edit_syntax_get_byte (void *data, off_t byte_index)
{
    return edit_buffer_get_byte (&((WEdit *) data)->buffer, byte_index);
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

static const char *
edit_first_line (const WEdit *edit)
{
    static char s[256];

    s[0] = '\0';

    if (edit != NULL)
    {
        size_t i;

        for (i = 0; i < sizeof (s) - 1; i++)
        {
            s[i] = edit_buffer_get_byte (&edit->buffer, i);
            if (s[i] == '\n')
            {
                s[i] = '\0';
                break;
            }
        }

        s[sizeof (s) - 1] = '\0';
    }

    return s;
}

/* --------------------------------------------------------------------------------------------- */

static int
pstrcmp (gconstpointer p1, gconstpointer p2)
{
    return strcmp ((const char *) p1, (const char *) p2);
}

/* --------------------------------------------------------------------------------------------- */

static int
exec_edit_syntax_dialog (const GPtrArray *names, const char *current_syntax)
{
    Listbox *syntaxlist;
    guint i;

    syntaxlist =
        listbox_window_new (LIST_LINES, MAX_ENTRY_LEN, _ ("Choose syntax highlighting"), NULL);
    LISTBOX_APPEND_TEXT (syntaxlist, 'A', _ ("< Auto >"), NULL, FALSE);
    LISTBOX_APPEND_TEXT (syntaxlist, 'R', _ ("< Reload Current Syntax >"), NULL, FALSE);

    for (i = 0; i < names->len; i++)
    {
        const char *name;

        name = g_ptr_array_index (names, i);
        LISTBOX_APPEND_TEXT (syntaxlist, 0, name, NULL, FALSE);
        if (current_syntax != NULL && strcmp (name, current_syntax) == 0)
            listbox_set_current (syntaxlist->list, i + N_DFLT_ENTRIES);
    }

    return listbox_run (syntaxlist);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
edit_get_syntax_color (WEdit *edit, off_t byte_index)
{
    if (!tty_use_colors ())
        return 0;

    if (edit->syntax == NULL)
        return EDITOR_NORMAL_COLOR;

    syntax_scanner_set_size (edit->syntax, edit->buffer.size);

    return syntax_palette_get (edit->palette,
                               syntax_rules_color_of (syntax_scanner_rules (edit->syntax),
                                                      syntax_state_at (edit->syntax, byte_index)));
}

/* --------------------------------------------------------------------------------------------- */

void
edit_line_local_syntax_reset (syntax_line_local_state_t *state, off_t line_start)
{
    syntax_line_local_reset (state, line_start);
}

/* --------------------------------------------------------------------------------------------- */

int
edit_get_line_local_syntax_color (const WEdit *edit, syntax_line_local_state_t *state,
                                  off_t byte_index)
{
    if (edit->syntax == NULL)
        return EDITOR_NORMAL_COLOR;

    return syntax_palette_get (edit->palette,
                               syntax_line_local_color (syntax_scanner_rules (edit->syntax), state,
                                                        edit_syntax_get_byte, (void *) edit,
                                                        byte_index));
}

/* --------------------------------------------------------------------------------------------- */

gboolean
edit_syntax_is_line_local (const WEdit *edit)
{
    return edit != NULL && edit->syntax != NULL
        && syntax_rules_is_line_local (syntax_scanner_rules (edit->syntax));
}

/* --------------------------------------------------------------------------------------------- */

void
edit_free_syntax_rules (WEdit *edit)
{
    if (edit == NULL)
        return;

    syntax_palette_free (edit->palette);
    edit->palette = NULL;
    syntax_scanner_free (edit->syntax);
    edit->syntax = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Load rules into edit struct.  Either edit or pnames must be NULL.  If edit is
 * NULL, a list of types is stored into pnames.  If type is NULL, the type is
 * selected according to the file name and its first line.
 */

void
edit_load_syntax (WEdit *edit, GPtrArray *pnames, const char *type)
{
    char *f;
    char *error_file = NULL;
    char *saved_type;
    int r = 0;

    if (auto_syntax)
        type = NULL;

    /* The name a caller passes is usually the one the current rule set carries,
       and that is about to be freed. */
    saved_type = g_strdup (type);

    edit_free_syntax_rules (edit);

    if (!tty_use_colors ())
    {
        g_free (saved_type);
        return;
    }

    f = mc_config_get_full_path (EDIT_SYNTAX_FILE);

    if (edit == NULL)
    {
        if (pnames != NULL)
            r = syntax_rules_list_types (f, pnames);
    }
    else if (edit->filename_vpath != NULL)
    {
        syntax_select_t sel;
        syntax_rules_t *rules = NULL;

        sel.type = saved_type;
        sel.filename = vfs_path_as_str (edit->filename_vpath);
        sel.first_line = edit_first_line (edit);

        r = syntax_rules_load (f, &sel, &rules, &error_file);
        if (r == 0)
        {
            edit->syntax =
                syntax_scanner_new (rules, edit_syntax_get_byte, edit, edit->buffer.size);
            edit->palette =
                syntax_palette_new (rules, syntax_tty_alloc_color, syntax_tty_release_color,
                                    (void *) "editor", EDITOR_NORMAL_COLOR);
            syntax_rules_unref (rules);
        }
        else if (r < 0 && g_file_test (f, G_FILE_TEST_EXISTS))
            r = 0;  // the file simply has no syntax of its own
    }

    if (r == -1)
        file_error_message (_ ("Cannot open file\n%s"), f);
    else if (r > 0)
        message (D_ERROR, _ ("Load syntax file"), _ ("Error in file %s on line %d"),
                 error_file != NULL ? error_file : f, r);

    g_free (error_file);
    g_free (f);
    g_free (saved_type);
}

/* --------------------------------------------------------------------------------------------- */

const char *
edit_get_syntax_type (const WEdit *edit)
{
    return edit->syntax == NULL ? NULL : syntax_rules_type (syntax_scanner_rules (edit->syntax));
}

/* --------------------------------------------------------------------------------------------- */

void
edit_syntax_dialog (WEdit *edit)
{
    GPtrArray *names;
    int syntax;
    char *current_syntax;

    names = g_ptr_array_new_with_free_func (g_free);
    current_syntax = g_strdup (edit_get_syntax_type (edit));

    // We fill the list of syntax files every time the editor is invoked.
    edit_load_syntax (NULL, names, NULL);
    g_ptr_array_sort_values (names, pstrcmp);

    syntax = exec_edit_syntax_dialog (names, current_syntax);
    if (syntax >= 0)
    {
        gboolean old_auto_syntax;
        const char *chosen = NULL;

        old_auto_syntax = auto_syntax;

        switch (syntax)
        {
        case 0:  // auto syntax
            auto_syntax = TRUE;
            break;
        case 1:  // reload current syntax
            break;
        default:
            auto_syntax = FALSE;
            chosen = g_ptr_array_index (names, syntax - N_DFLT_ENTRIES);
            break;
        }

        // Load or unload syntax rules if the state has changed
        if (syntax == 1 || old_auto_syntax != auto_syntax
            || (chosen != NULL && g_strcmp0 (current_syntax, chosen) != 0))
            edit_load_syntax (edit, NULL, chosen != NULL ? chosen : current_syntax);
    }

    g_free (current_syntax);
    g_ptr_array_free (names, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
