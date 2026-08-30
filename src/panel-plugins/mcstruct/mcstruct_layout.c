/*
   mcstruct - zone layout, status bars and skin colors

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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

/** \file mcstruct_layout.c
 *  \brief Source: zone layout, status bars and skin colors
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "mcstruct_ui_priv.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static cb_ret_t status_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data);
static void ui_load_colors (ui_colors_t *c);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
status_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WStatus *st = (WStatus *) w;

    switch (msg)
    {
    case MSG_DRAW:
        tty_setcolor (st->color >= 0 ? st->color : STATUSBAR_COLOR);
        widget_gotoyx (w, 0, 0);
        tty_print_string (str_fit_to_term (st->text != NULL ? st->text : "", w->rect.cols, J_LEFT));
        return MSG_HANDLED;
    case MSG_DESTROY:
        g_free (st->text);
        return MSG_HANDLED;
    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

WStatus *
status_new (int y, int x, int cols)
{
    WStatus *st = g_new0 (WStatus, 1);
    WRect r = { y, x, 1, cols };

    widget_init (WIDGET (st), &r, status_callback, NULL);
    st->color = -1;
    return st;
}

/* --------------------------------------------------------------------------------------------- */

void
status_set_text (WStatus *st, const char *text)
{
    g_free (st->text);
    st->text = g_strdup (text);
    widget_draw (WIDGET (st));
}

/* --------------------------------------------------------------------------------------------- */

void
ui_layout (ui_t *ui)
{
    const WRect *d = &WIDGET (ui->dlg)->rect;
    int cols = d->cols;
    int y = d->y;
    int hex_lines = ui->hex_hidden || ui->settings.hex_lines == 0 ? 0 : ui->settings.hex_lines + 1;
    int def_lines = ui->settings.def_lines > 0 ? ui->settings.def_lines + 1 : 0;
    int tree_lines = ui->settings.tree_lines > 0 ? ui->settings.tree_lines + 1 : 0;
    int avail = d->lines - 1 - 1 - 2; /* title, buttonbar, tree frame; zones count their head */
    gboolean side = ui->settings.def_layout == 1 || (ui->settings.def_layout == 0 && cols >= 120);
    int tree_cols = cols, def_cols = 0, def_x = d->x;

    if (ui->zoom != 0)
    {
        /* one zone takes everything between the title and the button bar */
        int rows = d->lines - 2;
        Widget *head = ui->zoom == 1 ? WIDGET (ui->tree_head)
            : ui->zoom == 2          ? WIDGET (ui->hex_head)
                                     : WIDGET (ui->def_head);
        Widget *body = ui->zoom == 1 ? WIDGET (ui->tree)
            : ui->zoom == 2          ? WIDGET (ui->hex)
                                     : WIDGET (ui->def);

        memset (ui->box, 0, sizeof (ui->box));
        widget_set_size (WIDGET (ui->title), y, d->x, 1, cols);
        widget_set_visibility (WIDGET (ui->tree_head), ui->zoom == 1);
        widget_set_visibility (WIDGET (ui->tree), ui->zoom == 1 && ui->grid == NULL);
        if (ui->grid != NULL)
            widget_set_visibility (WIDGET (ui->grid), ui->zoom == 1);
        widget_set_visibility (WIDGET (ui->hex_head), ui->zoom == 2);
        widget_set_visibility (WIDGET (ui->hex), ui->zoom == 2);
        widget_set_visibility (WIDGET (ui->def_head), ui->zoom == 3);
        widget_set_visibility (WIDGET (ui->def), ui->zoom == 3);
        if (ui->zoom == 2)
        {
            widget_set_size (head, y + 1, d->x, 1, cols);
            widget_set_size (body, y + 2, d->x, MAX (rows - 2, 1), cols);
        }
        else
        {
            ui->box[ui->zoom - 1] = (WRect) { y + 1, d->x, rows, cols };
            widget_set_size (head, y + 2, d->x + 1, 1, cols - 2);
            widget_set_size (body, y + 3, d->x + 1, MAX (rows - 3, 1), cols - 1);
        }
        if (ui->zoom == 1)
        {
            widget_set_size (WIDGET (ui->tree), y + 3, d->x + 1, MAX (rows - 3, 1), cols - 1);
            if (ui->grid != NULL)
                ui_grid_build (ui);
        }
        goto columns;
    }

    if (hex_lines > 0)
        avail -= hex_lines;
    if (avail < 3)
    {
        hex_lines = 0;
        avail = d->lines - 4;
    }

    if (side)
    {
        /* tree and def-file share the rows above the hex zone */
        def_cols = cols * ui->settings.def_width / 100;
        if (def_cols < 20)
            def_cols = 0;
        tree_cols = cols - def_cols;
        def_x = d->x + tree_cols;
        tree_lines = MAX (avail, 1);
        def_lines = def_cols > 0 ? tree_lines : 0;
    }
    else if (tree_lines > 0 && def_lines == 0)
    {
        /* fixed tree, the def-file zone takes the rest */
        if (tree_lines > avail - 4)
            tree_lines = MAX (avail - 4, 2);
        def_lines = avail - tree_lines - 2;
        if (def_lines < 2)
            def_lines = 0;
    }
    else
    {
        if (def_lines > 0)
            avail -= def_lines + 2;
        if (avail < 3)
        {
            def_lines = 0;
            avail = d->lines - 4 - (hex_lines > 0 ? hex_lines : 0);
        }
        tree_lines = tree_lines > 0 ? MIN (tree_lines, avail) : avail;
        if (tree_lines < avail)
            def_lines = avail - tree_lines - 2 > 1 ? avail - tree_lines - 2 : 0;
        tree_lines = MAX (tree_lines, 1);
    }

    /* every zone sits inside a frame, its head line is the first row inside */
    memset (ui->box, 0, sizeof (ui->box));
    widget_set_visibility (WIDGET (ui->tree_head), TRUE);
    widget_set_visibility (WIDGET (ui->tree), ui->grid == NULL);
    widget_set_size (WIDGET (ui->title), y, d->x, 1, cols);
    y++;
    ui->box[0] = (WRect) { y, d->x, tree_lines + 2, tree_cols };
    widget_set_size (WIDGET (ui->tree_head), y + 1, d->x + 1, 1, tree_cols - 2);
    /* the table is one column wider than the frame's inside: its scrollbar sits on the frame */
    widget_set_size (WIDGET (ui->tree), y + 2, d->x + 1, MAX (tree_lines - 1, 1), tree_cols - 1);
    if (side && def_lines > 0)
    {
        ui->box[1] = (WRect) { y, def_x, def_lines + 2, def_cols };
        widget_set_size (WIDGET (ui->def_head), y + 1, def_x + 1, 1, def_cols - 2);
        widget_set_size (WIDGET (ui->def), y + 2, def_x + 1, MAX (def_lines - 1, 1), def_cols - 1);
    }
    y += tree_lines + 2;
    if (ui->grid != NULL)
        ui_grid_build (ui);

    widget_set_visibility (WIDGET (ui->hex_head), hex_lines > 0);
    widget_set_visibility (WIDGET (ui->hex), hex_lines > 0);
    if (hex_lines > 0)
    {
        /* no frame: the status line, then the dump rows */
        widget_set_size (WIDGET (ui->hex_head), y, d->x, 1, cols);
        widget_set_size (WIDGET (ui->hex), y + 1, d->x, MAX (hex_lines - 1, 1), cols);
        y += hex_lines;
    }

    widget_set_visibility (WIDGET (ui->def_head), def_lines > 0);
    widget_set_visibility (WIDGET (ui->def), def_lines > 0);
    if (!side && def_lines > 0)
    {
        ui->box[1] = (WRect) { y, d->x, def_lines + 2, cols };
        widget_set_size (WIDGET (ui->def_head), y + 1, d->x + 1, 1, cols - 2);
        widget_set_size (WIDGET (ui->def), y + 2, d->x + 1, MAX (def_lines - 1, 1), cols - 1);
        y += def_lines + 2;
    }

columns:
    /* tree columns */
    {
        table_column_def_t *c = ui->tree->col_defs;
        int tw = WIDGET (ui->tree)->rect.cols;
        int dw = WIDGET (ui->def)->rect.cols;
        int off_w = ui->settings.offset_column != 0 ? 9 : 0;
        int key_w = ui->settings.name_width + 4;
        int hint_w = 8;
        int comment_w = tw > 110 ? 24 : 0;
        /* WTable: one column of margin, a separator after every column but the last,
           the scrollbar in the last column: TREE_COLS + 1 cells are not for text */
        int value_w = tw - (TREE_COLS + 1) - off_w - key_w - hint_w - comment_w;

        if (value_w < 10)
        {
            comment_w = 0;
            value_w = tw - (TREE_COLS + 1) - off_w - key_w - hint_w;
        }
        c[COL_OFFSET].width = off_w;
        c[COL_KEY].width = key_w;
        c[COL_HINT].width = hint_w;
        c[COL_VALUE].width = MAX (value_w, 1);
        c[COL_COMMENT].width = comment_w;

        ui->def->col_defs[0].width = 6;
        ui->def->col_defs[1].width = MAX (dw - 6 - (DEF_COLS + 1), 1);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* colors from the skin; a missing section falls back to the zone default, then core */
static void
ui_load_colors (ui_colors_t *c)
{
    c->tree_normal = mc_skin_color_get ("mcstruct-tree", "_default_");
    c->tree_frame = mc_skin_color_get ("mcstruct-tree", "frame");
    c->tree_frame_active = mc_skin_color_get ("mcstruct-tree", "frame-active");
    c->tree_head = mc_skin_color_get ("mcstruct-tree", "head");
    c->tree_selected = mc_skin_color_get ("mcstruct-tree", "selected");
    c->tree_offset = mc_skin_color_get ("mcstruct-tree", "offset");
    c->tree_name = mc_skin_color_get ("mcstruct-tree", "name");
    c->tree_type = mc_skin_color_get ("mcstruct-tree", "type");
    c->tree_value = mc_skin_color_get ("mcstruct-tree", "value");
    c->tree_struct = mc_skin_color_get ("mcstruct-tree", "struct");
    c->tree_jump = mc_skin_color_get ("mcstruct-tree", "jump");
    c->tree_remark = mc_skin_color_get ("mcstruct-tree", "remark");
    c->tree_error = mc_skin_color_get ("mcstruct-tree", "error");

    c->hex_head = mc_skin_color_get ("mcstruct-hex", "head");
    c->hex.normal = mc_skin_color_get ("mcstruct-hex", "_default_");
    c->hex.offset = mc_skin_color_get ("mcstruct-hex", "offset");
    c->hex.mark = mc_skin_color_get ("mcstruct-hex", "mark");
    c->hex.changed = mc_skin_color_get ("mcstruct-hex", "changed");
    c->hex.cursor = mc_skin_color_get ("mcstruct-hex", "cursor");
    c->hex.frame = mc_skin_color_get ("mcstruct-hex", "frame");
    c->hex.block = mc_skin_color_get ("mcstruct-hex", "block");

    c->def_normal = mc_skin_color_get ("mcstruct-def", "_default_");
    c->def_frame = mc_skin_color_get ("mcstruct-def", "frame");
    c->def_frame_active = mc_skin_color_get ("mcstruct-def", "frame-active");
    c->def_head = mc_skin_color_get ("mcstruct-def", "head");
    c->def_selected = mc_skin_color_get ("mcstruct-def", "selected");
    c->def_lineno = mc_skin_color_get ("mcstruct-def", "lineno");
    c->def_directive = mc_skin_color_get ("mcstruct-def", "directive");
    c->def_comment = mc_skin_color_get ("mcstruct-def", "comment");
    c->def_label = mc_skin_color_get ("mcstruct-def", "label");
}

/* --------------------------------------------------------------------------------------------- */

void
ui_apply_colors (ui_t *ui)
{
    ui_load_colors (&ui->colors);
    ui->tree->normal_color = ui->colors.tree_normal;
    ui->tree->selected_color = ui->colors.tree_selected;
    ui->tree->scrollbar_color = ui->colors.tree_frame;
    ui->tree_head->color = ui->colors.tree_head;
    ui->def->normal_color = ui->colors.def_normal;
    ui->def->selected_color = ui->colors.def_selected;
    ui->def->scrollbar_color = ui->colors.def_frame;
    ui->def_head->color = ui->colors.def_head;
    ui->hex_head->color = ui->colors.hex_head;
    hexstrip_set_colors (ui->hex, &ui->colors.hex);
}

/* --------------------------------------------------------------------------------------------- */
