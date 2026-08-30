/*
   mcstruct - the grid view of tables, arrays and #repeat rows

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

/** \file mcstruct_grid.c
 *  \brief Source: the grid view of tables, arrays and #repeat rows
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "mcstruct_ui_priv.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static int grid_col_width (const slv_node_t *leaf);
static void collect_leaves (slv_node_t *node, GPtrArray *out);
static int grid_get_nrows (const void *data);
static const char *grid_get_text (const void *data, int row, int col);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --- table screen -------------------------------------------------------------------------- */

static int
grid_col_width (const slv_node_t *leaf)
{
    const slv_item_t *it = leaf->item;
    int w;

    if (it->view_width > 0)
        return it->view_width + 1;
    switch (it->type)
    {
    case SLV_TYPE_CHAR:
    case SLV_TYPE_STR8:
    case SLV_TYPE_STR16:
    case SLV_TYPE_PSTRING:
    case SLV_TYPE_CSTRING:
        /* the width the field has in this file */
        w = leaf->size > 0 ? CLAMP ((int) leaf->size, 4, 40) : 16;
        break;
    case SLV_TYPE_HEX:
    case SLV_TYPE_BE_HEX:
    case SLV_TYPE_JUMP:
        w = it->size * 2;
        break;
    case SLV_TYPE_INT:
    case SLV_TYPE_UINT:
        w = it->size >= 8 ? 20 : it->size == 4 ? 11 : it->size == 2 ? 6 : 4;
        break;
    case SLV_TYPE_BITS:
        w = it->size * 8 + 2;
        break;
    case SLV_TYPE_FLOAT:
    case SLV_TYPE_MSBIN:
    case SLV_TYPE_PTR:
        w = 14;
        break;
    case SLV_TYPE_TIME_DOS:
    case SLV_TYPE_TIME_UNIX:
        w = 19;
        break;
    default:
        w = 10;
        break;
    }
    if (it->legend != NULL)
        w += 14;
    if (it->name != NULL && (int) str_term_width1 (it->name) > w)
        w = MIN ((int) str_term_width1 (it->name), 30);
    return w + 1; /* the column cursor marker */
}

/* --------------------------------------------------------------------------------------------- */

/* the fields of a row in order, through nested structures and #repeat passes */
static void
collect_leaves (slv_node_t *node, GPtrArray *out)
{
    guint i;

    if (node->children == NULL)
        return;
    for (i = 0; i < node->children->len; i++)
    {
        slv_node_t *c = g_ptr_array_index (node->children, i);

        switch (c->kind)
        {
        case SLV_NODE_FIELD:
            if (c->item != NULL && c->item->hidden)
                break;
            g_ptr_array_add (out, c);
            break;
        case SLV_NODE_JUMP:
        case SLV_NODE_ERROR:
            g_ptr_array_add (out, c);
            break;
        case SLV_NODE_REMARK:
            break;
        default:
            collect_leaves (c, out);
            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* the field node of row @row in column @col, or NULL; rows share the shape of the first */
slv_node_t *
ui_grid_cell_node (ui_t *ui, int row, int col)
{
    slv_node_t *rec;

    if (ui->grid_node == NULL || ui->grid_node->children == NULL || row < 0
        || (guint) row >= ui->grid_node->children->len)
        return NULL;
    col += ui->grid_first_col - ui->grid_lead;
    if (col < 0 || (guint) col >= ui->grid_cols->len)
        return NULL;
    rec = g_ptr_array_index (ui->grid_node->children, row);
    if (ui->grid_cache == NULL)
        ui->grid_cache = g_ptr_array_new ();
    if (ui->grid_cache_row != row)
    {
        g_ptr_array_set_size (ui->grid_cache, 0);
        collect_leaves (rec, ui->grid_cache);
        ui->grid_cache_row = row;
    }
    if ((guint) col >= ui->grid_cache->len)
        return NULL;
    return g_ptr_array_index (ui->grid_cache, col);
}

/* --------------------------------------------------------------------------------------------- */

static int
grid_get_nrows (const void *data)
{
    const ui_t *ui = data;

    return ui->grid_node != NULL && ui->grid_node->children != NULL
        ? (int) ui->grid_node->children->len
        : 0;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
grid_get_text (const void *data, int row, int col)
{
    ui_t *ui = (ui_t *) data;
    const slv_node_t *n;

    if (ui->grid_lead > 0 && col == 0)
    {
        /* the row number or the file offset of the row */
        g_string_truncate (ui->cell, 0);
        if (ui->settings.grid_rows == 1)
            g_string_append_printf (ui->cell, "%d", row + 1);
        else if (ui->grid_node->children != NULL && row >= 0
                 && (guint) row < ui->grid_node->children->len)
        {
            const slv_node_t *rec = g_ptr_array_index (ui->grid_node->children, row);

            g_string_append_printf (ui->cell, "%08llX", (unsigned long long) rec->offset);
        }
        return ui->cell->str;
    }
    n = ui_grid_cell_node (ui, row, col);
    if (n == NULL)
        return "";
    if (n->kind == SLV_NODE_ERROR)
        return n->text != NULL ? n->text : "ERROR";
    g_string_truncate (ui->cell, 0);
    if (n->text != NULL)
        g_string_append (ui->cell, n->text);
    if (n->legend != NULL)
        g_string_append_printf (ui->cell, " (%s)", n->legend);
    return ui->cell->str;
}

/* --------------------------------------------------------------------------------------------- */

void
ui_grid_heads (ui_t *ui)
{
    GString *h = g_string_new (" ");
    int c;
    char *padded;

    if (ui->grid_lead > 0)
    {
        g_string_append (h,
                         str_fit_to_term (ui->settings.grid_rows == 1 ? "#" : "offset",
                                          ui->grid->col_defs[0].width - 1, J_LEFT));
        g_string_append (h, "  ");
    }
    for (c = 0; c < ui->grid_ncols; c++)
    {
        const slv_node_t *leaf = g_ptr_array_index (ui->grid_cols, ui->grid_first_col + c);
        int w = ui->grid->col_defs[c + ui->grid_lead].width;
        const char *name = leaf->key != NULL ? leaf->key : "";

        /* the table widget draws a marker column and a separator */
        g_string_append (h, str_fit_to_term (name, w - 1, J_LEFT));
        g_string_append (h, "  ");
    }
    padded = g_strdup_printf ("%-*s", WIDGET (ui->tree)->rect.cols, h->str);
    status_set_text (ui->tree_head, padded);
    g_free (padded);
    g_string_free (h, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* (re)create the grid widget for the columns from grid_first_col that fit the zone */
void
ui_grid_build (ui_t *ui)
{
    const WRect *tr = &WIDGET (ui->tree)->rect;
    table_column_def_t *defs;
    table_datasource_t ds = { grid_get_nrows, grid_get_text, NULL, NULL, ui, NULL };
    int n = 0, used = 0, c;
    int current = ui->grid != NULL ? table_get_current (ui->grid) : 0;
    /* a rebuild (resize, refresh) must not take the focus away from the hex or def-file zone */
    gboolean take_focus = !widget_get_state (WIDGET (ui->hex), WST_FOCUSED)
        && !widget_get_state (WIDGET (ui->def), WST_FOCUSED);
    int avail = tr->cols - 2; /* the margin and the scrollbar */

    if (ui->grid != NULL)
    {
        group_remove_widget (ui->grid);
        widget_destroy (WIDGET (ui->grid));
        ui->grid = NULL;
    }

    defs = g_new0 (table_column_def_t, ui->grid_cols->len + 2);
    ui->grid_lead = ui->settings.grid_rows != 0 ? 1 : 0;
    if (ui->grid_lead > 0)
    {
        defs[0].width = ui->settings.grid_rows == 1 ? 7 : 10;
        defs[0].align = J_LEFT_FIT;
        defs[0].type = TABLE_COL_TEXT;
        used = defs[0].width;
        n = 1;
    }
    for (c = ui->grid_first_col; c < (int) ui->grid_cols->len; c++)
    {
        const slv_node_t *leaf = g_ptr_array_index (ui->grid_cols, c);
        int w = leaf->item != NULL ? grid_col_width (leaf) : 12;
        int sep = n > 0 ? 1 : 0; /* separator before this column */

        if (n > ui->grid_lead && used + sep + w > avail)
            break;
        if (used + sep + w > avail)
            w = MAX (avail - used - sep, 4);
        defs[n].width = w;
        defs[n].align = J_LEFT_FIT;
        defs[n].type = TABLE_COL_CHOICE;
        used += sep + w;
        n++;
    }
    ui->grid_ncols = n - ui->grid_lead;

    ui->grid = table_new (tr->y, tr->x, tr->lines, tr->cols, n, defs);
    g_free (defs);
    table_set_datasource (ui->grid, ds);
    ui->grid->scrollbar = TRUE;
    ui->grid->scrollbar_on_frame = TRUE;
    ui->grid->normal_color = ui->colors.tree_normal;
    ui->grid->selected_color = ui->colors.tree_selected;
    ui->grid->scrollbar_color = ui->colors.tree_frame;
    group_add_widget (GROUP (ui->dlg), ui->grid);
    table_set_current (ui->grid, current);
    if (take_focus)
        widget_select (WIDGET (ui->grid));
    ui_grid_heads (ui);
}

/* --------------------------------------------------------------------------------------------- */

/* tables, arrays of structures and #repeat blocks open as a grid */
gboolean
node_is_gridable (const slv_node_t *n)
{
    return n != NULL
        && (n->kind == SLV_NODE_TABLE || n->kind == SLV_NODE_NESTED || n->kind == SLV_NODE_REPEAT);
}

/* --------------------------------------------------------------------------------------------- */

void
ui_open_grid (ui_t *ui, slv_node_t *node)
{
    if (!node_is_gridable (node))
        return;
    if (node->lazy)
        slv_node_expand (&ui->ev, node);
    if (node->children == NULL || node->children->len == 0)
    {
        message (D_NORMAL, _ ("Table"), _ ("The table has no rows"));
        return;
    }

    if (ui->grid == NULL)
        ui->tree_current = table_get_current (ui->tree);
    g_free (ui->grid_path);
    ui->grid_path = node_path (node);
    ui->grid_node = node;
    ui->grid_first_col = 0;

    if (ui->grid_cols == NULL)
        ui->grid_cols = g_ptr_array_new ();
    g_ptr_array_set_size (ui->grid_cols, 0);
    ui->grid_cache_row = -1;
    collect_leaves (g_ptr_array_index (node->children, 0), ui->grid_cols);
    if (ui->grid_cols->len == 0)
    {
        message (D_NORMAL, _ ("Table"), _ ("The table has no columns"));
        ui->grid_node = NULL;
        return;
    }

    widget_set_visibility (WIDGET (ui->tree), FALSE);
    ui_grid_build (ui);
    ui_update_heads (ui);
    widget_draw (WIDGET (ui->dlg));
}

/* --------------------------------------------------------------------------------------------- */

void
ui_close_grid (ui_t *ui)
{
    if (ui->grid == NULL)
        return;
    group_remove_widget (ui->grid);
    widget_destroy (WIDGET (ui->grid));
    ui->grid = NULL;
    ui->grid_node = NULL;
    ui->grid_cache_row = -1;
    widget_set_visibility (WIDGET (ui->tree), TRUE);
    widget_select (WIDGET (ui->tree));
    ui_update_heads (ui);
    widget_draw (WIDGET (ui->dlg));
}

/* --------------------------------------------------------------------------------------------- */

void
ui_grid_scroll (ui_t *ui, int dir)
{
    int first = ui->grid_first_col + dir;

    if (ui->grid == NULL || first < 0 || first >= (int) ui->grid_cols->len)
        return;
    ui->grid_first_col = first;
    ui_grid_build (ui);
    widget_draw (WIDGET (ui->dlg));
}

/* --------------------------------------------------------------------------------------------- */

void
ui_sync_from_grid (ui_t *ui)
{
    slv_node_t *n;
    int row;

    if (ui->grid == NULL || ui->syncing)
        return;
    row = table_get_current (ui->grid);
    n = ui_grid_cell_node (ui, row, ui->grid->current_col);
    if (n == NULL && ui->grid_node->children != NULL && row >= 0
        && (guint) row < ui->grid_node->children->len)
        n = g_ptr_array_index (ui->grid_node->children, row);
    if (n == NULL)
        return;
    ui->syncing = TRUE;
    hexstrip_set_mark (ui->hex, n->offset, n->size);
    if (n->line > 0 && ui->file != NULL && (guint) n->line <= ui->file->lines->len)
    {
        table_set_current (ui->def, n->line - 1);
        widget_draw (WIDGET (ui->def));
    }
    ui_update_heads (ui);
    ui->syncing = FALSE;
}

/* --------------------------------------------------------------------------------------------- */
