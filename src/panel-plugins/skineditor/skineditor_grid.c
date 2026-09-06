/*
   Skin editor plugin - a palette of color cells.

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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/tty/color.h"
#include "lib/tty/color-internal.h"  // convert_256color_to_truecolor
#include "lib/tty/key.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "skineditor_grid.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define CELL_COLS 2

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* two rows like the reference: the dark colors over their bright counterparts */
static const char *const named_rows[2][8] = {
    { "black", "blue", "green", "cyan", "red", "magenta", "brown", "lightgray" },
    { "gray", "brightblue", "brightgreen", "brightcyan", "brightred", "brightmagenta", "yellow",
      "white" },
};

static const char *color256_names[256];

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static const char *
name256 (int n)
{
    if (color256_names[n] == NULL)
        color256_names[n] = g_strdup_printf ("color%d", n);
    return color256_names[n];
}

/* --------------------------------------------------------------------------------------------- */

static void
add_cell (WColorGrid *g, int y, int x, const char *value)
{
    colorgrid_cell_t c = { y, x, value };

    g_array_append_val (g->cells, c);
}

/* --------------------------------------------------------------------------------------------- */

static void
build_map (WColorGrid *g, colorgrid_map_t map)
{
    int i, j;

    if (map == COLORGRID_16)
    {
        for (i = 0; i < 2; i++)
            for (j = 0; j < 8; j++)
                add_cell (g, i, j * (CELL_COLS + 1), named_rows[i][j]);
        return;
    }

    for (i = 0; i < 16; i++)
        add_cell (g, 0, i * (CELL_COLS + 1), name256 (i));
    // the cube: one 6x6 block per red level, green down, blue across
    for (i = 0; i < 6; i++)
        for (j = 0; j < 36; j++)
        {
            int block = j / 6, blue = j % 6;
            int n = 16 + block * 36 + i * 6 + blue;

            add_cell (g, 2 + i, block * (6 * CELL_COLS + 1) + blue * CELL_COLS, name256 (n));
        }
    for (i = 0; i < 24; i++)
        add_cell (g, 9, i * (CELL_COLS + 1), name256 (232 + i));
}

/* --------------------------------------------------------------------------------------------- */

static const colorgrid_cell_t *
cell_at (const WColorGrid *g, int i)
{
    if (i < 0 || i >= (int) g->cells->len)
        return NULL;
    return &g_array_index (g->cells, colorgrid_cell_t, i);
}

/* --------------------------------------------------------------------------------------------- */

static int
cell_under (const WColorGrid *g, int y, int x)
{
    int i;

    for (i = 0; i < (int) g->cells->len; i++)
    {
        const colorgrid_cell_t *c = cell_at (g, i);

        if (c->y == y && x >= c->x && x < c->x + CELL_COLS)
            return i;
    }
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/* the cell nearest to (@y, @x) in the row @y, or -1 when the row is empty */

static int
cell_nearest_in_row (const WColorGrid *g, int y, int x)
{
    int i, best = -1, best_d = 0;

    for (i = 0; i < (int) g->cells->len; i++)
    {
        const colorgrid_cell_t *c = cell_at (g, i);
        int d;

        if (c->y != y)
            continue;
        d = abs (c->x - x);
        if (best < 0 || d < best_d)
        {
            best = i;
            best_d = d;
        }
    }
    return best;
}

/* --------------------------------------------------------------------------------------------- */

static void
set_current (WColorGrid *g, int i)
{
    if (i < 0 || i == g->current)
        return;
    g->current = i;
    widget_draw (WIDGET (g));
    if (g->on_move != NULL)
        g->on_move (g, cell_at (g, i)->value, g->data);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_vertical (WColorGrid *g, int dir)
{
    const colorgrid_cell_t *c = cell_at (g, g->current);
    int y, i = -1;

    if (c == NULL)
        return;
    for (y = c->y + dir; y >= 0 && y < WIDGET (g)->rect.lines && i < 0; y += dir)
        i = cell_nearest_in_row (g, y, c->x);
    set_current (g, i);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_horizontal (WColorGrid *g, int dir)
{
    const colorgrid_cell_t *c = cell_at (g, g->current);
    int i, best = -1, best_d = 0;

    if (c == NULL)
        return;
    for (i = 0; i < (int) g->cells->len; i++)
    {
        const colorgrid_cell_t *o = cell_at (g, i);
        int d = (o->x - c->x) * dir;

        if (o->y != c->y || d <= 0)
            continue;
        if (best < 0 || d < best_d)
        {
            best = i;
            best_d = d;
        }
    }
    set_current (g, best);
}

/* --------------------------------------------------------------------------------------------- */

static void
pick (WColorGrid *g)
{
    const colorgrid_cell_t *c = cell_at (g, g->current);

    if (c == NULL)
        return;
    colorgrid_set_selected (g, c->value);
    if (g->on_pick != NULL)
        g->on_pick (g, c->value, g->data);
}

/* --------------------------------------------------------------------------------------------- */

static void
grid_draw (WColorGrid *g)
{
    Widget *w = WIDGET (g);
    const int *colors = widget_get_colors (w);
    int i;

    tty_setcolor (colors[DLG_COLOR_NORMAL]);
    tty_fill_region (w->rect.y, w->rect.x, w->rect.lines, w->rect.cols, ' ');

    for (i = 0; i < (int) g->cells->len; i++)
    {
        const colorgrid_cell_t *c = cell_at (g, i);
        tty_color_pair_t pair;
        gboolean is_selected = (g->selected != NULL && strcmp (g->selected, c->value) == 0);

        pair.fg = (char *) colorgrid_contrast (c->value);
        pair.bg = (char *) c->value;
        pair.attrs = NULL;
        pair.pair_index = 0;
        tty_setcolor (tty_try_alloc_color_pair (&pair, TRUE));
        widget_gotoyx (w, c->y, c->x);
        // a bullet on a UTF-8 display, a star elsewhere
        tty_print_string (!is_selected ? "  " : mc_global.utf8_display ? "\xe2\x80\xa2 " : "* ");
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
grid_key (WColorGrid *g, int key)
{
    switch (key)
    {
    case KEY_UP:
        move_vertical (g, -1);
        return MSG_HANDLED;
    case KEY_DOWN:
        move_vertical (g, 1);
        return MSG_HANDLED;
    case KEY_LEFT:
        move_horizontal (g, -1);
        return MSG_HANDLED;
    case KEY_RIGHT:
        move_horizontal (g, 1);
        return MSG_HANDLED;
    case KEY_HOME:
        set_current (g, 0);
        return MSG_HANDLED;
    case KEY_END:
        set_current (g, (int) g->cells->len - 1);
        return MSG_HANDLED;
    case ' ':
    case '\n':
    case KEY_ENTER:
        pick (g);
        return MSG_HANDLED;
    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
grid_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WColorGrid *g = COLORGRID (w);

    switch (msg)
    {
    case MSG_KEY:
        return grid_key (g, parm);

    case MSG_CURSOR:
    {
        const colorgrid_cell_t *c = cell_at (g, g->current);

        if (c != NULL)
            widget_gotoyx (w, c->y, c->x);
        return MSG_HANDLED;
    }

    case MSG_DRAW:
        grid_draw (g);
        return MSG_HANDLED;

    case MSG_DESTROY:
        g_array_free (g->cells, TRUE);
        g_free (g->selected);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
grid_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WColorGrid *g = COLORGRID (w);
    int i;

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        widget_select (w);
        i = cell_under (g, event->y, event->x);
        if (i >= 0)
            set_current (g, i);
        break;

    case MSG_MOUSE_CLICK:
        i = cell_under (g, event->y, event->x);
        if (i >= 0 && i == g->current)
            pick (g);
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
colorgrid_size (colorgrid_map_t map, int *lines, int *cols)
{
    if (map == COLORGRID_16)
    {
        *lines = 2;
        *cols = 8 * (CELL_COLS + 1) - 1;
    }
    else
    {
        *lines = 10;
        *cols = 6 * (6 * CELL_COLS + 1) - 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

WColorGrid *
colorgrid_new (int y, int x, colorgrid_map_t map)
{
    WColorGrid *g;
    Widget *w;
    WRect r = { y, x, 0, 0 };

    colorgrid_size (map, &r.lines, &r.cols);
    g = g_new0 (WColorGrid, 1);
    w = WIDGET (g);
    widget_init (w, &r, grid_callback, grid_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_WANT_CURSOR;
    g->cells = g_array_new (FALSE, FALSE, sizeof (colorgrid_cell_t));
    build_map (g, map);
    return g;
}

/* --------------------------------------------------------------------------------------------- */

void
colorgrid_set_selected (WColorGrid *g, const char *value)
{
    int i;

    g_free (g->selected);
    g->selected = g_strdup (value);
    if (value != NULL)
        for (i = 0; i < (int) g->cells->len; i++)
            if (strcmp (cell_at (g, i)->value, value) == 0)
            {
                g->current = i;
                break;
            }
    widget_draw (WIDGET (g));
}

/* --------------------------------------------------------------------------------------------- */

const char *
colorgrid_current (const WColorGrid *g)
{
    const colorgrid_cell_t *c = cell_at (g, g->current);

    return c != NULL ? c->value : NULL;
}

/* --------------------------------------------------------------------------------------------- */

const char *
colorgrid_contrast (const char *value)
{
    static const char *const dark[] = { "black",   "blue",  "green", "cyan",       "red",
                                        "magenta", "brown", "gray",  "brightblue", NULL };
    int idx, i, rgb, lum;

    for (i = 0; dark[i] != NULL; i++)
        if (strcmp (value, dark[i]) == 0)
            return "white";
    idx = tty_color_get_index_by_name (value);
    if (idx < 16)
        return "black";
    rgb = convert_256color_to_truecolor (idx) & 0xFFFFFF;
    lum = (((rgb >> 16) & 0xFF) * 3 + ((rgb >> 8) & 0xFF) * 6 + (rgb & 0xFF)) / 10;
    return lum < 128 ? "white" : "black";
}

/* --------------------------------------------------------------------------------------------- */
