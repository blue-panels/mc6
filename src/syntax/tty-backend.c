/*
   Lexical scanner for the M-Commander
   A rule set's colors as terminal color pairs

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

/** \file tty-backend.c
 *  \brief Source: terminal color backend for the lexical scanner
 *
 *  Kept apart from the scanner itself, which knows nothing about terminals or
 *  skins: this is one of several ways a rule set's symbolic colors can be
 *  realized, and the only one that needs a screen.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/skin.h"
#include "lib/tty/color.h"

#include "tty-backend.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Terminal pair for one symbolic color of a rule set.
 *
 * The part of a name after a slash is the 256-color alternative, which is not
 * what this backend draws with.
 *
 * @param backend_data skin section a half-specified color falls back to
 * @param fg foreground the rule set asks for, NULL for none
 * @param bg background it asks for, NULL for none
 * @param attrs attributes it asks for, NULL for none
 * @return index of the allocated pair
 */

int
syntax_tty_alloc_color (void *backend_data, const char *fg, const char *bg, const char *attrs)
{
    char f[80], b[80], a[80], *p;
    tty_color_pair_t color;

    color.fg = fg != NULL && *fg != '\0' ? (char *) fg : NULL;
    color.bg = bg != NULL && *bg != '\0' ? (char *) bg : NULL;
    color.attrs = attrs != NULL && *attrs != '\0' ? (char *) attrs : NULL;

    if (color.fg == NULL && color.bg == NULL)
        return tty_try_alloc_color_pair (&color, TRUE);

    if (color.fg != NULL)
    {
        g_strlcpy (f, color.fg, sizeof (f));
        p = strchr (f, '/');
        if (p != NULL)
            *p = '\0';
        color.fg = f;
    }
    if (color.bg != NULL)
    {
        g_strlcpy (b, color.bg, sizeof (b));
        p = strchr (b, '/');
        if (p != NULL)
            *p = '\0';
        color.bg = b;
    }
    if (color.fg == NULL || color.bg == NULL)
    {
        char *editnormal;

        editnormal = mc_skin_get (backend_data != NULL ? (const char *) backend_data : "core",
                                  "_default_", "default;default");

        if (color.fg == NULL)
        {
            g_strlcpy (f, editnormal, sizeof (f));
            p = strchr (f, ';');
            if (p != NULL)
                *p = '\0';
            if (f[0] == '\0')
                g_strlcpy (f, "default", sizeof (f));
            color.fg = f;
        }
        if (color.bg == NULL)
        {
            p = strchr (editnormal, ';');
            if ((p != NULL) && (*(++p) != '\0'))
                g_strlcpy (b, p, sizeof (b));
            else
                g_strlcpy (b, "default", sizeof (b));
            color.bg = b;
        }

        g_free (editnormal);
    }

    if (color.attrs != NULL)
    {
        g_strlcpy (a, color.attrs, sizeof (a));
        p = strchr (a, '/');
        if (p != NULL)
            *p = '\0';
        color.attrs = a;
    }

    return tty_try_alloc_color_pair (&color, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Give back a pair taken by syntax_tty_alloc_color().
 *
 * The pair is shared with whoever else asked for the same colors, so it goes away
 * only when the last holder lets go; a skin pair is not temporary and is ignored.
 */

void
syntax_tty_release_color (int color)
{
    tty_color_release_temp (color);
}
