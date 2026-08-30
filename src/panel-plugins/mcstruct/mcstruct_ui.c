/*
   mcstruct - the screen: structure tree, hex strip, def-file

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

/** \file mcstruct_ui.c
 *  \brief Source: the screen: structure tree, hex strip, def-file
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "mcstruct_ui_priv.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static gboolean node_has_children (const slv_node_t *n);
static gboolean node_visible (const ui_t *ui, const slv_node_t *n);
static void add_rows (ui_t *ui, slv_node_t *node, int depth);
static void ui_rebuild_rows (ui_t *ui);
static void expand_default (slv_node_t *node, int depth);
static const char *tree_key_text (ui_t *ui, const row_t *r);
static int tree_get_nrows (const void *data);
static const char *tree_get_text (const void *data, int row, int col);
static int def_get_nrows (const void *data);
static const char *def_get_text (const void *data, int row, int col);
static int tree_get_color (void *data, int row, int col);
static int def_get_color (void *data, int row, int col);
static void hex_on_cursor (WHexStrip *h, void *data);
static void ui_set_root (ui_t *ui, slv_node_t *root, int current);
static void ui_clear_jumps (ui_t *ui);
static slv_node_t *find_by_path (slv_node_t *root, const char *path);
static void collect_expanded (const slv_node_t *node, GString *path, GHashTable *set);
static void apply_expanded (const slv_eval_t *ev, slv_node_t *node, GString *path, GHashTable *set);
static void ui_toggle_node (ui_t *ui, gboolean expand);
static void ui_go_parent (ui_t *ui);
static void ui_expand_to_depth (ui_t *ui, int depth);
static void ui_follow_jump (ui_t *ui);
static void ui_follow_jump_node (ui_t *ui, slv_node_t *n);
static void ui_jump_back (ui_t *ui);
static void ui_activate_row (ui_t *ui);
static gboolean hex_on_edit (WHexStrip *h, off_t offset, unsigned char value, void *data);
static gboolean hex_is_changed (void *ctx, off_t offset);
static cb_ret_t ui_execute (ui_t *ui, long command);
static cb_ret_t ui_key (ui_t *ui, int key);
static cb_ret_t ui_dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm,
                                    void *data);
static void ui_create_widgets (ui_t *ui);

/*** file scope variables ************************************************************************/

static const global_keymap_t bb_keymap[] = { { KEY_F (1), CMD_HELP, "F1" },
                                             { KEY_F (2), CMD_SAVE, "F2" },
                                             { KEY_F (3), CMD_STRUCT, "F3" },
                                             { KEY_F (4), CMD_EDIT, "F4" },
                                             { KEY_F (5), CMD_GOTO, "F5" },
                                             { KEY_F (6), CMD_DEF, "F6" },
                                             { KEY_F (7), CMD_SEARCH, "F7" },
                                             { KEY_F (8), CMD_HEX, "F8" },
                                             { KEY_F (9), CMD_ERRORS, "F9" },
                                             { KEY_F (10), CMD_QUIT, "F10" },
                                             { KEY_F (16), CMD_EDIT_DEF, "S-F6" },
                                             { KEY_F (17), CMD_SEARCH_NEXT, "S-F7" },
                                             { KEY_F (14), CMD_BACK, "S-F4" },
                                             { KEY_F (12), CMD_PUT_BYTES, "S-F2" },
                                             { KEY_M_CTRL | KEY_F (2), CMD_GET_BYTES, "C-F2" },
                                             { 0, 0, "" } };

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
ui_hex_focused (const ui_t *ui)
{
    return GROUP (ui->dlg)->current != NULL
        && WIDGET (GROUP (ui->dlg)->current->data) == WIDGET (ui->hex);
}

/* --------------------------------------------------------------------------------------------- */

row_t *
ui_current_row (ui_t *ui)
{
    int cur = table_get_current (ui->tree);

    if (ui->rows->len == 0 || cur < 0 || (guint) cur >= ui->rows->len)
        return NULL;
    return &g_array_index (ui->rows, row_t, cur);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
node_has_children (const slv_node_t *n)
{
    return n->lazy || (n->children != NULL && n->children->len > 0);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
node_visible (const ui_t *ui, const slv_node_t *n)
{
    if (n->kind == SLV_NODE_FIELD && n->item != NULL && n->item->hidden)
        return ui->settings.show_hidden;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_rows (ui_t *ui, slv_node_t *node, int depth)
{
    guint i;

    if (node->children == NULL)
        return;
    for (i = 0; i < node->children->len; i++)
    {
        slv_node_t *c = g_ptr_array_index (node->children, i);
        row_t r;

        if (!node_visible (ui, c))
            continue;
        r.node = c;
        r.depth = depth;
        r.index = (int) i;
        g_array_append_val (ui->rows, r);
        if (c->expanded && !c->lazy)
            add_rows (ui, c, depth + 1);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_rebuild_rows (ui_t *ui)
{
    row_t r;

    g_array_set_size (ui->rows, 0);
    if (ui->root == NULL)
        return;
    r.node = ui->root;
    r.depth = 0;
    r.index = 0;
    g_array_append_val (ui->rows, r);
    if (ui->root->expanded)
        add_rows (ui, ui->root, 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
expand_default (slv_node_t *node, int depth)
{
    guint i;

    node->expanded = depth < 2 && !node->lazy;
    if (node->children == NULL)
        return;
    for (i = 0; i < node->children->len; i++)
        expand_default (g_ptr_array_index (node->children, i), depth + 1);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
tree_key_text (ui_t *ui, const row_t *r)
{
    const slv_node_t *n = r->node;
    const char *marker = node_has_children (n) ? (n->expanded ? "- " : "+ ") : "  ";
    int i;

    g_string_truncate (ui->cell, 0);
    for (i = 0; i < r->depth; i++)
        g_string_append (ui->cell, "  ");
    g_string_append (ui->cell, marker);
    switch (n->kind)
    {
    case SLV_NODE_STRUCT:
        if (r->depth == 0)
            g_string_append_printf (ui->cell, "/%s", n->key != NULL ? n->key : "");
        else
            g_string_append_printf (ui->cell, "[%d] %s", r->index, n->key != NULL ? n->key : "");
        break;
    case SLV_NODE_REPEAT:
        g_string_append (ui->cell, "#repeat");
        break;
    case SLV_NODE_REMARK:
        g_string_append_printf (ui->cell, ": %s", n->key != NULL ? n->key : "");
        break;
    case SLV_NODE_ERROR:
        g_string_append (ui->cell, "ERROR");
        break;
    default:
        g_string_append (ui->cell, n->key != NULL ? n->key : "");
        break;
    }
    return ui->cell->str;
}

/* --------------------------------------------------------------------------------------------- */

static int
tree_get_nrows (const void *data)
{
    const ui_t *ui = data;

    return (int) ui->rows->len;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
tree_get_text (const void *data, int row, int col)
{
    ui_t *ui = (ui_t *) data;
    const row_t *r;
    const slv_node_t *n;

    if (row < 0 || (guint) row >= ui->rows->len)
        return "";
    r = &g_array_index (ui->rows, row_t, row);
    n = r->node;

    switch (col)
    {
    case COL_OFFSET:
        if (n->kind == SLV_NODE_ERROR)
            return "";
        g_string_printf (ui->cell, "%08llX",
                         (unsigned long long) (ui->settings.offset_column == 1 && ui->root != NULL
                                                   ? n->offset - ui->root->offset
                                                   : n->offset));
        return ui->cell->str;
    case COL_KEY:
        return tree_key_text (ui, r);
    case COL_HINT:
        if (n->kind == SLV_NODE_STRUCT)
        {
            g_string_printf (ui->cell, "%lld", (long long) n->size);
            return ui->cell->str;
        }
        return n->hint != NULL ? n->hint : "";
    case COL_VALUE:
        if (n->kind == SLV_NODE_ERROR)
            return n->text != NULL ? n->text : "";
        if (n->kind == SLV_NODE_STRUCT || n->kind == SLV_NODE_REMARK)
            return "";
        g_string_truncate (ui->cell, 0);
        if (n->text != NULL)
            g_string_append (ui->cell, n->text);
        if (n->legend != NULL)
            g_string_append_printf (ui->cell, " (%s)", n->legend);
        if (n->kind == SLV_NODE_JUMP)
            g_string_append_printf (ui->cell, " -> %08llX", (unsigned long long) n->jump_target);
        if (n->kind == SLV_NODE_BUFFER)
            g_string_append (ui->cell, " -> Enter");
        if (n->lazy)
            g_string_append (ui->cell, " ...");
        return ui->cell->str;
    case COL_COMMENT:
        return n->comment != NULL ? n->comment : "";
    default:
        return "";
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
def_get_nrows (const void *data)
{
    const ui_t *ui = data;

    return ui->file != NULL ? (int) ui->file->lines->len : 0;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
def_get_text (const void *data, int row, int col)
{
    ui_t *ui = (ui_t *) data;

    if (ui->file == NULL || row < 0 || (guint) row >= ui->file->lines->len)
        return "";
    if (col == 0)
    {
        g_string_printf (ui->cell, "%5d", row + 1);
        return ui->cell->str;
    }
    return g_ptr_array_index (ui->file->lines, row);
}

/* --------------------------------------------------------------------------------------------- */

static int
tree_get_color (void *data, int row, int col)
{
    const ui_t *ui = data;
    const slv_node_t *n;

    if (row < 0 || (guint) row >= ui->rows->len)
        return -1;
    n = g_array_index (ui->rows, row_t, row).node;
    switch (n->kind)
    {
    case SLV_NODE_ERROR:
        return ui->colors.tree_error;
    case SLV_NODE_REMARK:
        return ui->colors.tree_remark;
    case SLV_NODE_STRUCT:
    case SLV_NODE_NESTED:
    case SLV_NODE_TABLE:
    case SLV_NODE_REPEAT:
        return col == COL_OFFSET ? ui->colors.tree_offset : ui->colors.tree_struct;
    case SLV_NODE_JUMP:
    case SLV_NODE_BUFFER:
        return col == COL_OFFSET ? ui->colors.tree_offset : ui->colors.tree_jump;
    default:
        break;
    }
    if (n->legend != NULL && col == COL_VALUE && g_str_has_prefix (n->legend, "MISMATCH"))
        return ui->colors.tree_error;
    switch (col)
    {
    case COL_OFFSET:
        return ui->colors.tree_offset;
    case COL_KEY:
        return ui->colors.tree_name;
    case COL_HINT:
        return ui->colors.tree_type;
    default:
        return ui->colors.tree_value;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
def_get_color (void *data, int row, int col)
{
    const ui_t *ui = data;
    const char *line;
    if (ui->file == NULL || row < 0 || (guint) row >= ui->file->lines->len)
        return -1;
    if (col == 0)
        return ui->colors.def_lineno;
    line = g_ptr_array_index (ui->file->lines, row);
    while (*line == ' ' || *line == '\t')
        line++;
    if (*line == '#' || *line == '/')
        return ui->colors.def_directive;
    if (*line == ';')
        return ui->colors.def_comment;
    {
        const char *p = line;

        while (g_ascii_isalnum (*p) || *p == '_')
            p++;
        if (p > line && *p == ':' && !g_ascii_isdigit (*line))
            return ui->colors.def_label;
    }
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

void
ui_update_heads (ui_t *ui)
{
    const row_t *r = ui_current_row (ui);
    const slv_node_t *n = r != NULL ? r->node : NULL;
    off_t cur = hexstrip_get_cursor (ui->hex);
    int cols = WIDGET (ui->dlg)->rect.cols;
    char *s, *padded;

    if (ui->grid != NULL)
        s = g_strdup_printf (" mcstruct  %s  %s  %s  row %d/%u  col %d/%u  %s", ui->display_name,
                             ui->def_path != NULL ? x_basename (ui->def_path) : "-",
                             ui->grid_node->def != NULL       ? ui->grid_node->def->name
                                 : ui->grid_node->key != NULL ? ui->grid_node->key
                                                              : "",
                             table_get_current (ui->grid) + 1, ui->grid_node->children->len,
                             ui->grid_first_col + ui->grid->current_col + 1 - ui->grid_lead,
                             ui->grid_cols->len,
                             slv_file_reader_change_count (ui->fr) > 0 ? "[modified]" : "");
    else
        s = g_strdup_printf (" mcstruct  %s  %s  %s  %s", ui->display_name,
                             ui->def_path != NULL ? x_basename (ui->def_path) : "-",
                             ui->root != NULL && ui->root->def != NULL ? ui->root->def->name : "",
                             slv_file_reader_change_count (ui->fr) > 0 ? "[modified]" : "");
    padded = g_strdup_printf ("%-*s", cols, s);
    status_set_text (ui->title, s);
    g_free (padded);
    g_free (s);

    if (ui->grid != NULL)
    {
        ui_grid_heads (ui);
        n = ui_grid_cell_node (ui, table_get_current (ui->grid), ui->grid->current_col);
    }
    else
    {
        s = g_strdup_printf (" %s: %lld bytes at %08llX%*s%d/%u   %s",
                             ui->root != NULL && ui->root->def != NULL ? ui->root->def->name : "-",
                             ui->root != NULL ? (long long) ui->root->size : 0LL,
                             ui->root != NULL ? (unsigned long long) ui->root->offset : 0ULL, 4, "",
                             table_get_current (ui->tree) + 1, ui->rows->len,
                             n != NULL && n->key != NULL ? n->key : "");
        padded = g_strdup_printf ("%-*s", WIDGET (ui->tree)->rect.cols, s);
        status_set_text (ui->tree_head, padded);
        g_free (padded);
        g_free (s);
    }

    if (ui->hex->block_len > 0)
        s = g_strdup_printf (
            " offset %08llX (%lld)  local +%llX  block %08llX-%08llX (%lld bytes)  file %lld bytes",
            (unsigned long long) cur, (long long) cur,
            (unsigned long long) (ui->root != NULL ? cur - ui->root->offset : cur),
            (unsigned long long) ui->hex->block_start,
            (unsigned long long) (ui->hex->block_start + ui->hex->block_len - 1),
            (long long) ui->hex->block_len, (long long) ui->fr->size);
    else
        s = g_strdup_printf (
            " offset %08llX (%lld)  local +%llX  field %lld bytes  file %lld bytes",
            (unsigned long long) cur, (long long) cur,
            (unsigned long long) (ui->root != NULL ? cur - ui->root->offset : cur),
            n != NULL ? (long long) n->size : 0LL, (long long) ui->fr->size);
    padded = g_strdup_printf ("%-*s", cols, s);
    status_set_text (ui->hex_head, s);
    g_free (padded);
    g_free (s);

    s = g_strdup_printf (
        " %s  line %d/%u%s%u errors", ui->def_path != NULL ? x_basename (ui->def_path) : "-",
        table_get_current (ui->def) + 1, ui->file != NULL ? ui->file->lines->len : 0, "  ",
        ui->file != NULL ? ui->file->errors->len : 0);
    padded = g_strdup_printf ("%-*s", WIDGET (ui->def)->rect.cols, s);
    status_set_text (ui->def_head, padded);
    g_free (padded);
    g_free (s);
}

/* --------------------------------------------------------------------------------------------- */

void
ui_sync_from_tree (ui_t *ui)
{
    const row_t *r = ui_current_row (ui);

    if (r == NULL || ui->syncing)
        return;
    ui->syncing = TRUE;
    hexstrip_set_mark (ui->hex, r->node->offset, r->node->size);
    if (r->node->line > 0 && ui->file != NULL && (guint) r->node->line <= ui->file->lines->len)
    {
        table_set_current (ui->def, r->node->line - 1);
        widget_draw (WIDGET (ui->def));
    }
    ui_update_heads (ui);
    ui->syncing = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* the deepest visible row covering the hex cursor */
void
ui_sync_from_hex (ui_t *ui)
{
    off_t cur = hexstrip_get_cursor (ui->hex);
    int best = -1;
    off_t best_size = -1;
    guint i;

    if (ui->syncing)
        return;
    for (i = 0; i < ui->rows->len; i++)
    {
        const row_t *r = &g_array_index (ui->rows, row_t, i);
        const slv_node_t *n = r->node;

        if (n->size <= 0 || cur < n->offset || cur >= n->offset + n->size)
            continue;
        if (best < 0 || n->size <= best_size)
        {
            best = (int) i;
            best_size = n->size;
        }
    }
    ui->syncing = TRUE;
    if (best >= 0 && best != table_get_current (ui->tree))
    {
        const row_t *r = &g_array_index (ui->rows, row_t, best);

        table_set_current (ui->tree, best);
        widget_draw (WIDGET (ui->tree));
        if (r->node->line > 0 && ui->file != NULL)
        {
            table_set_current (ui->def, r->node->line - 1);
            widget_draw (WIDGET (ui->def));
        }
        widget_draw (WIDGET (ui->def));
    }
    ui_update_heads (ui);
    ui->syncing = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
hex_on_cursor (WHexStrip *h, void *data)
{
    (void) h;
    ui_sync_from_hex (data);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_set_root (ui_t *ui, slv_node_t *root, int current)
{
    ui->root = root;
    ui_rebuild_rows (ui);
    if (ui->rows->len == 0)
        current = 0;
    else if (current >= (int) ui->rows->len)
        current = (int) ui->rows->len - 1;
    table_set_current (ui->tree, current);
    if (ui->rows->len == 0)
    {
        /* table_set_current does nothing on an empty table */
        ui->tree->current = 0;
        ui->tree->top = 0;
    }
    widget_draw (WIDGET (ui->tree));
    ui_sync_from_tree (ui);
}

/* --------------------------------------------------------------------------------------------- */

/* the labels of the structure a jump or buffer row belongs to: readable in the target */
static GHashTable *
ui_source_labels (const slv_node_t *n)
{
    const slv_node_t *p;

    for (p = n->parent; p != NULL; p = p->parent)
        if (p->labels != NULL)
            return p->labels;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* back from a buffer: the reader and the hex source of the level above */
static void
ui_leave_buffer (ui_t *ui, jump_t *j)
{
    if (j->mem_reader == NULL)
        return;
    ui->ev.reader = j->outer_reader;
    hexstrip_set_source (ui->hex, &j->outer_src);
    slv_reader_free (j->mem_reader);
    j->mem_reader = NULL;
    ui->buffer_depth--;
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_clear_jumps (ui_t *ui)
{
    guint i;

    for (i = ui->jumps->len; i > 0; i--)
    {
        jump_t *j = g_ptr_array_index (ui->jumps, i - 1);

        ui_leave_buffer (ui, j);
        slv_node_free (j->root);
        g_free (j);
    }
    g_ptr_array_set_size (ui->jumps, 0);
}

/* --------------------------------------------------------------------------------------------- */

void
ui_build (ui_t *ui, const slv_def_t *def, off_t offset)
{
    ui_build_rows (ui, def, offset, 1);
}

/* --------------------------------------------------------------------------------------------- */

void
ui_build_rows (ui_t *ui, const slv_def_t *def, off_t offset, gint64 rows)
{
    slv_node_t *root;

    ui_close_grid (ui);
    ui_clear_jumps (ui);
    if (ui->root != NULL)
        slv_node_free (ui->root);
    ui->root = NULL;
    if (def == NULL)
    {
        ui_set_root (ui, NULL, 0);
        return;
    }
    if (def->kind == SLV_DEF_TABLE)
        root = slv_eval_table (&ui->ev, def, offset, rows > 0 ? rows : 1);
    else
        root = slv_eval_struct (&ui->ev, def, offset);
    expand_default (root, 0);
    ui_set_root (ui, root, 0);
}

/* --------------------------------------------------------------------------------------------- */

char *
node_path (const slv_node_t *node)
{
    GString *p = g_string_new ("");

    while (node->parent != NULL)
    {
        guint i;
        char *s;

        for (i = 0; i < node->parent->children->len; i++)
            if (g_ptr_array_index (node->parent->children, i) == node)
                break;
        s = g_strdup_printf ("%u/%s", i, p->str);
        g_string_assign (p, s);
        g_free (s);
        node = node->parent;
    }
    return g_string_free (p, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static slv_node_t *
find_by_path (slv_node_t *root, const char *path)
{
    slv_node_t *n = root;
    const char *p = path;

    while (n != NULL && *p != '\0')
    {
        guint i = (guint) strtoul (p, (char **) &p, 10);

        if (*p == '/')
            p++;
        if (n->children == NULL || i >= n->children->len)
            return NULL;
        n = g_ptr_array_index (n->children, i);
    }
    return n;
}

/* --------------------------------------------------------------------------------------------- */

static void
collect_expanded (const slv_node_t *node, GString *path, GHashTable *set)
{
    guint i;
    gsize len = path->len;

    if (node->expanded && !node->lazy)
        g_hash_table_add (set, g_strdup (path->str));
    if (node->children == NULL)
        return;
    for (i = 0; i < node->children->len; i++)
    {
        g_string_append_printf (path, "%u/", i);
        collect_expanded (g_ptr_array_index (node->children, i), path, set);
        g_string_truncate (path, len);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_expanded (const slv_eval_t *ev, slv_node_t *node, GString *path, GHashTable *set)
{
    guint i;
    gsize len = path->len;

    node->expanded = g_hash_table_contains (set, path->str);
    if (node->expanded && node->lazy)
        slv_node_expand (ev, node);
    if (node->children == NULL)
        return;
    for (i = 0; i < node->children->len; i++)
    {
        g_string_append_printf (path, "%u/", i);
        apply_expanded (ev, g_ptr_array_index (node->children, i), path, set);
        g_string_truncate (path, len);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* re-evaluate the current root in place, keep the cursor row and the expansion by index path */
void
ui_refresh (ui_t *ui)
{
    slv_node_t *root;
    int current = table_get_current (ui->tree);
    const slv_def_t *def;
    off_t offset;
    GHashTable *expanded;
    GString *path;

    if (ui->root == NULL || ui->root->def == NULL)
        return;
    def = ui->root->def;
    offset = ui->root->offset;
    if (ui->root->kind == SLV_NODE_TABLE)
        root = slv_eval_table (&ui->ev, def, offset, ui->root->rows);
    else
        root = slv_eval_struct (&ui->ev, def, offset);

    expanded = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    path = g_string_new ("/");
    collect_expanded (ui->root, path, expanded);
    g_string_assign (path, "/");
    apply_expanded (&ui->ev, root, path, expanded);
    g_string_free (path, TRUE);
    g_hash_table_destroy (expanded);

    if (ui->grid != NULL)
    {
        /* the grid points into the old tree: close it, reopen on the new one */
        char *saved_path = g_strdup (ui->grid_path);
        int grid_row = table_get_current (ui->grid);
        int grid_col = ui->grid->current_col;
        int first = ui->grid_first_col;
        slv_node_t *again;

        ui_close_grid (ui);
        slv_node_free (ui->root);
        ui_set_root (ui, root, current);
        again = find_by_path (root, saved_path);
        g_free (saved_path);
        if (node_is_gridable (again))
        {
            ui_open_grid (ui, again);
            if (ui->grid != NULL)
            {
                ui->grid_first_col = first;
                ui_grid_build (ui);
                table_set_current (ui->grid, grid_row);
                ui->grid->current_col = grid_col;
                ui_update_heads (ui);
                widget_draw (WIDGET (ui->dlg));
            }
        }
        return;
    }

    slv_node_free (ui->root);
    ui_set_root (ui, root, current);
}

/* --------------------------------------------------------------------------------------------- */

/* before the first edit: take the file lock as mcedit does */
void
ui_note_change (ui_t *ui)
{
    if (!ui->locked && slv_file_reader_change_count (ui->fr) == 0)
        ui->locked = lock_file (ui->vpath) != 0;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
ui_load_def (ui_t *ui, const char *path)
{
    GError *error = NULL;
    slv_file_t *file;

    file = slv_file_load (path, &error);
    if (file == NULL)
    {
        message (D_ERROR, _ ("Struct look"), "%s", error->message);
        g_error_free (error);
        return FALSE;
    }
    if (ui->file != NULL)
        slv_file_free (ui->file);
    ui->file = file;
    ui->ev.file = file;
    g_free (ui->def_path);
    ui->def_path = g_strdup (path);
    /* exec: providers only for def-files the user keeps in the config directory */
    {
        char *user_dir = g_build_filename (mc_config_get_path (), "mcstruct", (char *) NULL);

        ui->ev.trust = g_str_has_prefix (path, user_dir) ? 1 : 0;
        g_free (user_dir);
    }
    widget_draw (WIDGET (ui->def));
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_toggle_node (ui_t *ui, gboolean expand)
{
    row_t *r = ui_current_row (ui);
    slv_node_t *n;
    int current = table_get_current (ui->tree);

    if (r == NULL)
        return;
    n = r->node;
    if (!node_has_children (n))
        return;
    if (expand && n->lazy)
        slv_node_expand (&ui->ev, n);
    n->expanded = expand;
    ui_rebuild_rows (ui);
    table_set_current (ui->tree, current);
    widget_draw (WIDGET (ui->tree));
    ui_update_heads (ui);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_go_parent (ui_t *ui)
{
    row_t *r = ui_current_row (ui);
    int i;

    if (r == NULL || r->node->parent == NULL)
        return;
    for (i = table_get_current (ui->tree) - 1; i >= 0; i--)
        if (g_array_index (ui->rows, row_t, i).node == r->node->parent)
        {
            table_set_current (ui->tree, i);
            ui_sync_from_tree (ui);
            return;
        }
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_expand_to_depth (ui_t *ui, int depth)
{
    guint i;
    int current = table_get_current (ui->tree);

    if (ui->root == NULL)
        return;
    /* only over the rows already built; lazy nodes stay collapsed */
    for (i = 0; i < ui->rows->len; i++)
    {
        row_t *r = &g_array_index (ui->rows, row_t, i);

        if (node_has_children (r->node) && !r->node->lazy)
            r->node->expanded = r->depth < depth;
    }
    ui_rebuild_rows (ui);
    table_set_current (ui->tree, current);
    widget_draw (WIDGET (ui->tree));
    ui_update_heads (ui);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_follow_jump (ui_t *ui)
{
    row_t *r = ui_current_row (ui);

    if (r != NULL)
        ui_follow_jump_node (ui, r->node);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_follow_jump_node (ui_t *ui, slv_node_t *n)
{
    slv_node_t *root;
    jump_t *j;

    if (n == NULL || (n->kind != SLV_NODE_JUMP && n->kind != SLV_NODE_BUFFER) || n->def == NULL)
        return;
    if (ui->jumps->len >= 256)
        return;
    if (n->kind == SLV_NODE_BUFFER)
    {
        gsize len = 0;
        const void *data = g_bytes_get_data (n->buffer, &len);
        hexstrip_source_t src;

        j = g_new0 (jump_t, 1);
        j->root = ui->root;
        j->current = table_get_current (ui->tree);
        j->outer_reader = ui->ev.reader;
        j->outer_src = ui->hex->source;
        j->mem_reader = slv_reader_new_memory (data, len);
        ui->ev.reader = j->mem_reader;
        src.get_size = j->mem_reader->size;
        src.read = j->mem_reader->read;
        src.is_changed = NULL;
        src.ctx = j->mem_reader->ctx;
        hexstrip_set_source (ui->hex, &src);
        ui->buffer_depth++;
        if (n->def->kind == SLV_DEF_TABLE)
            root = slv_eval_table_in (&ui->ev, n->def, 0, n->rows > 0 ? n->rows : 1,
                                      ui_source_labels (n));
        else
            root = slv_eval_struct_in (&ui->ev, n->def, 0, ui_source_labels (n));
        expand_default (root, 0);
        g_ptr_array_add (ui->jumps, j);
        ui_set_root (ui, root, 0);
        return;
    }
    if (n->jump_target < 0 || n->jump_target >= ui->fr->size)
    {
        message (D_ERROR, _ ("Struct look"), _ ("Jump target 0x%llX is outside the file"),
                 (unsigned long long) n->jump_target);
        return;
    }
    if (ui->jumps->len >= 256)
        return;

    if (n->def->kind == SLV_DEF_TABLE)
        root = slv_eval_table_in (&ui->ev, n->def, n->jump_target, n->rows > 0 ? n->rows : 1,
                                  ui_source_labels (n));
    else
        root = slv_eval_struct_in (&ui->ev, n->def, n->jump_target, ui_source_labels (n));
    expand_default (root, 0);

    j = g_new0 (jump_t, 1);
    j->root = ui->root;
    j->current = table_get_current (ui->tree);
    g_ptr_array_add (ui->jumps, j);
    ui_set_root (ui, root, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_jump_back (ui_t *ui)
{
    jump_t *j;

    if (ui->jumps->len == 0)
        return;
    j = g_ptr_array_index (ui->jumps, ui->jumps->len - 1);
    g_ptr_array_remove_index (ui->jumps, ui->jumps->len - 1);
    slv_node_free (ui->root);
    ui_leave_buffer (ui, j);
    ui_set_root (ui, j->root, j->current);
    g_free (j);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_activate_row (ui_t *ui)
{
    row_t *r = ui_current_row (ui);

    if (r == NULL)
        return;
    if (r->node->kind == SLV_NODE_JUMP || r->node->kind == SLV_NODE_BUFFER)
        ui_follow_jump (ui);
    else if (node_is_text (r->node))
        ui_view_text (ui, r->node);
    else if (r->node->kind == SLV_NODE_TABLE || (node_is_gridable (r->node) && r->node->rows > 1))
        ui_open_grid (ui, r->node);
    else if (node_has_children (r->node))
        ui_toggle_node (ui, !r->node->expanded);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
hex_on_edit (WHexStrip *h, off_t offset, unsigned char value, void *data)
{
    ui_t *ui = data;
    off_t cursor = h->cursor, top = h->top;
    gboolean in_text = h->in_text;

    if (ui->buffer_depth > 0)
    {
        message (D_NORMAL, _ ("Struct look"), _ ("A buffer is read-only"));
        return;
    }
    ui_note_change (ui);
    slv_file_reader_set_byte (ui->fr, offset, value);
    /* the refresh re-syncs the mark from the tree, which moves the cursor to the
       start of the field; the user is typing here, so put it back */
    ui_refresh (ui);
    h->cursor = cursor;
    h->top = top;
    h->in_text = in_text;
    ui_update_heads (ui);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
hex_is_changed (void *ctx, off_t offset)
{
    return slv_file_reader_is_changed (ctx, offset);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
ui_execute (ui_t *ui, long command)
{
    switch (command)
    {
    case CMD_HELP:
        ui_cmd_help ();
        return MSG_HANDLED;
    case CMD_SAVE:
        ui_cmd_save (ui);
        return MSG_HANDLED;
    case CMD_PUT_BYTES:
        ui_cmd_put_bytes (ui);
        return MSG_HANDLED;
    case CMD_GET_BYTES:
        ui_cmd_get_bytes (ui);
        return MSG_HANDLED;
    case CMD_STRUCT:
        ui_cmd_select_struct (ui);
        return MSG_HANDLED;
    case CMD_EDIT:
        ui_cmd_edit_field (ui);
        return MSG_HANDLED;
    case CMD_GOTO:
    {
        off_t offset;

        if (ui_input_offset (ui, _ ("Goto"), &offset))
            hexstrip_set_cursor (ui->hex, offset);
        return MSG_HANDLED;
    }
    case CMD_DEF:
        ui_cmd_select_def (ui);
        return MSG_HANDLED;
    case CMD_EDIT_DEF:
    {
        const row_t *r = ui_current_row (ui);

        ui_cmd_edit_def (ui, r != NULL ? r->node->line : 1);
        return MSG_HANDLED;
    }
    case CMD_SEARCH:
        ui_cmd_search (ui, FALSE);
        return MSG_HANDLED;
    case CMD_SEARCH_NEXT:
        ui_cmd_search (ui, TRUE);
        return MSG_HANDLED;
    case CMD_HEX:
        ui->hex_hidden = !ui->hex_hidden;
        ui_layout (ui);
        widget_draw (WIDGET (ui->dlg));
        return MSG_HANDLED;
    case CMD_ERRORS:
        ui_cmd_errors (ui);
        return MSG_HANDLED;
    case CMD_CALC:
        ui_cmd_calc (ui);
        return MSG_HANDLED;
    case CMD_QUIT:
        ui_cmd_quit (ui);
        return MSG_HANDLED;
    case CMD_BACK:
        ui_cmd_quit (ui);
        ui->back = ui->quit;
        return MSG_HANDLED;
    default:
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
ui_key (ui_t *ui, int key)
{
    long command;
    gboolean tree_focused = GROUP (ui->dlg)->current != NULL
        && WIDGET (GROUP (ui->dlg)->current->data) == WIDGET (ui->tree);

    command = keybind_lookup_keymap_command (bb_keymap, key);
    if (command != CK_IgnoreKey)
        return ui_execute (ui, command);

    if (key == ALT ('='))
        return ui_execute (ui, CMD_CALC);
    if (key == ui->zoom_key || key == KEY_F (19))
    {
        Widget *cur =
            GROUP (ui->dlg)->current != NULL ? WIDGET (GROUP (ui->dlg)->current->data) : NULL;
        int zone = cur == WIDGET (ui->hex) ? 2 : cur == WIDGET (ui->def) ? 3 : 1;

        ui->zoom = ui->zoom == zone ? 0 : zone;
        ui_layout (ui);
        ui_update_heads (ui);
        widget_draw (WIDGET (ui->dlg));
        return MSG_HANDLED;
    }
    if (key == '\t' && ui->grid == NULL)
    {
        group_select_next_widget (GROUP (ui->dlg));
        return MSG_HANDLED;
    }
    if (ui->grid != NULL)
    {
        switch (key)
        {
        case ESC_CHAR:
        case KEY_BACKSPACE:
        case 127:
        case 8:
            ui_close_grid (ui);
            table_set_current (ui->tree, ui->tree_current);
            ui_sync_from_tree (ui);
            return MSG_HANDLED;
        case '\t':
        {
            int before = ui->grid->current_col;

            send_message (ui->grid, NULL, MSG_KEY, KEY_RIGHT, NULL);
            if (ui->grid->current_col == before)
            {
                ui->grid->current_col = ui->grid_lead;
                widget_draw (WIDGET (ui->grid));
            }
            ui_sync_from_grid (ui);
            return MSG_HANDLED;
        }
        case '<':
            ui_grid_scroll (ui, -1);
            return MSG_HANDLED;
        case '>':
            ui_grid_scroll (ui, 1);
            return MSG_HANDLED;
        case KEY_ENTER:
        case '\n':
        {
            slv_node_t *n =
                ui_grid_cell_node (ui, table_get_current (ui->grid), ui->grid->current_col);

            if (n != NULL && (n->kind == SLV_NODE_JUMP || n->kind == SLV_NODE_BUFFER))
            {
                ui_close_grid (ui);
                ui_follow_jump_node (ui, n);
            }
            return MSG_HANDLED;
        }
        default:
            return MSG_NOT_HANDLED;
        }
    }

    if (key == ESC_CHAR)
        return MSG_NOT_HANDLED;

    if (ui_hex_focused (ui))
    {
        /* a block in the hex zone: [ start, ] end, * clear */
        off_t cur = hexstrip_get_cursor (ui->hex);
        WHexStrip *h = ui->hex;

        switch (key)
        {
        case '[':
            if (h->block_len > 0 && cur < h->block_start + h->block_len)
                hexstrip_set_block (h, cur, h->block_start + h->block_len - cur);
            else
                hexstrip_set_block (h, cur, 1);
            break;
        case ']':
            if (h->block_len > 0 && cur >= h->block_start)
                hexstrip_set_block (h, h->block_start, cur - h->block_start + 1);
            else
                hexstrip_set_block (h, cur, 1);
            break;
        case '*':
            hexstrip_set_block (h, 0, 0);
            break;
        default:
            return MSG_NOT_HANDLED;
        }
        ui_update_heads (ui);
        return MSG_HANDLED;
    }

    if (widget_get_state (WIDGET (ui->def), WST_FOCUSED) && (key == KEY_ENTER || key == '\n'))
    {
        ui_def_go_reference (ui);
        return MSG_HANDLED;
    }

    if (!tree_focused)
        return MSG_NOT_HANDLED;

    if (key == (KEY_M_CTRL | KEY_NPAGE) || key == (KEY_M_CTRL | KEY_PPAGE))
    {
        ui_step_struct (ui, key == (KEY_M_CTRL | KEY_NPAGE) ? 1 : -1);
        return MSG_HANDLED;
    }

    switch (key)
    {
    case KEY_RIGHT:
    case '+':
    {
        row_t *r = ui_current_row (ui);

        if (r != NULL && (r->node->kind == SLV_NODE_JUMP || r->node->kind == SLV_NODE_BUFFER))
            ui_follow_jump (ui);
        else
            ui_toggle_node (ui, TRUE);
    }
        return MSG_HANDLED;
    case KEY_LEFT:
    case '-':
    {
        row_t *r = ui_current_row (ui);

        if (r != NULL && node_has_children (r->node) && r->node->expanded)
            ui_toggle_node (ui, FALSE);
        else
            ui_go_parent (ui);
    }
        return MSG_HANDLED;
    case KEY_ENTER:
    case '\n':
        ui_activate_row (ui);
        return MSG_HANDLED;
    case KEY_BACKSPACE:
    case 127:
    case 8:
        ui_jump_back (ui);
        return MSG_HANDLED;
    case '*':
        ui_expand_to_depth (ui, 99);
        return MSG_HANDLED;
    default:
        if (key >= '1' && key <= '9')
        {
            ui_expand_to_depth (ui, key - '0');
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
ui_dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *dlg = DIALOG (w);
    ui_t *ui = dlg->data.p;

    switch (msg)
    {
    case MSG_RESIZE:
        dlg_default_callback (w, sender, msg, parm, data);
        ui_layout (ui);
        ui_update_heads (ui);
        return MSG_HANDLED;

    case MSG_DRAW:
    {
        const int *colors = widget_get_colors (w);
        int i;

        dlg_default_callback (w, sender, msg, parm, data);
        for (i = 0; i < 3; i++)
            if (ui->box[i].lines > 0)
            {
                /* by widget state: MSG_CHANGED_FOCUS arrives before the group's current moves */
                gboolean tree_active = widget_get_state (WIDGET (ui->tree), WST_FOCUSED)
                    || (ui->grid != NULL && widget_get_state (WIDGET (ui->grid), WST_FOCUSED));
                gboolean def_active = widget_get_state (WIDGET (ui->def), WST_FOCUSED);
                int fc = i == 0
                    ? (tree_active ? ui->colors.tree_frame_active : ui->colors.tree_frame)
                    : i == 1 ? (def_active ? ui->colors.def_frame_active : ui->colors.def_frame)
                             : -1;

                tty_setcolor (fc >= 0 ? fc : colors[DLG_COLOR_FRAME]);
                tty_draw_box (ui->box[i].y, ui->box[i].x, ui->box[i].lines, ui->box[i].cols, FALSE);
            }
        /* the tables paint their scrollbar column on the frame line, in its color */
        {
            gboolean tree_active = widget_get_state (WIDGET (ui->tree), WST_FOCUSED)
                || (ui->grid != NULL && widget_get_state (WIDGET (ui->grid), WST_FOCUSED));
            int tree_fc = tree_active ? ui->colors.tree_frame_active : ui->colors.tree_frame;

            ui->tree->scrollbar_color = tree_fc;
            if (ui->grid != NULL)
                ui->grid->scrollbar_color = tree_fc;
            ui->def->scrollbar_color = widget_get_state (WIDGET (ui->def), WST_FOCUSED)
                ? ui->colors.def_frame_active
                : ui->colors.def_frame;
        }
        if (ui->grid != NULL)
            widget_draw (WIDGET (ui->grid));
        else
            widget_draw (WIDGET (ui->tree));
        widget_draw (WIDGET (ui->def));
        return MSG_HANDLED;
    }

    case MSG_KEY:
        return ui_key (ui, parm);

    case MSG_ACTION:
        return ui_execute (ui, parm);

    case MSG_NOTIFY:
        if (sender == WIDGET (ui->tree))
        {
            if (parm == CK_Enter)
                ui_activate_row (ui);
            else
                ui_sync_from_tree (ui);
            return MSG_HANDLED;
        }
        if (ui->grid != NULL && sender == WIDGET (ui->grid))
        {
            ui_sync_from_grid (ui);
            return MSG_HANDLED;
        }
        if (sender == WIDGET (ui->def))
        {
            ui_update_heads (ui);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_CHANGED_FOCUS:
        /* the frame of the focused zone changes color */
        widget_draw (w);
        return MSG_HANDLED;

    case MSG_VALIDATE:
        /* Esc: same question as F10 */
        if (!ui->quit)
        {
            ui_cmd_quit (ui);
            if (!ui->quit)
                widget_set_state (w, WST_ACTIVE, TRUE);
        }
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_create_widgets (ui_t *ui)
{
    WDialog *dlg;
    WGroup *g;
    Widget *wd;
    table_column_def_t tree_cols[TREE_COLS] = { { 9, J_LEFT, TABLE_COL_TEXT },
                                                { 20, J_LEFT_FIT, TABLE_COL_TEXT },
                                                { 8, J_LEFT, TABLE_COL_TEXT },
                                                { 30, J_LEFT_FIT, TABLE_COL_TEXT },
                                                { 0, J_LEFT_FIT, TABLE_COL_TEXT } };
    table_column_def_t def_cols[DEF_COLS] = { { 6, J_RIGHT, TABLE_COL_TEXT },
                                              { 70, J_LEFT_FIT, TABLE_COL_TEXT } };
    table_datasource_t tree_ds = { tree_get_nrows, tree_get_text, NULL, NULL, ui, NULL };
    table_datasource_t def_ds = { def_get_nrows, def_get_text, NULL, NULL, ui, NULL };
    hexstrip_source_t hex_src;

    dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, listbox_colors, ui_dialog_callback,
                      NULL, NULL, NULL);
    dlg->data.p = ui;
    ui->dlg = dlg;
    wd = WIDGET (dlg);
    widget_want_tab (wd, TRUE);
    g = GROUP (dlg);

    ui->title = status_new (0, 0, 80);
    group_add_widget (g, ui->title);
    ui->tree_head = status_new (1, 0, 80);
    group_add_widget (g, ui->tree_head);

    ui->tree = table_new (2, 0, 5, 80, TREE_COLS, tree_cols);
    table_set_datasource (ui->tree, tree_ds);
    table_set_cell_color (ui->tree, tree_get_color);
    ui->tree->scrollbar = TRUE;
    ui->tree->scrollbar_on_frame = TRUE;
    group_add_widget (g, ui->tree);

    ui->hex_head = status_new (8, 0, 80);
    group_add_widget (g, ui->hex_head);
    ui->hex = hexstrip_new (9, 0, 4, 80);
    hex_src.get_size = ui->fr->reader.size;
    hex_src.read = ui->fr->reader.read;
    hex_src.is_changed = hex_is_changed;
    hex_src.ctx = ui->fr;
    hexstrip_set_source (ui->hex, &hex_src);
    hexstrip_set_handlers (ui->hex, hex_on_cursor, hex_on_edit, ui);
    group_add_widget (g, ui->hex);

    ui->def_head = status_new (14, 0, 80);
    group_add_widget (g, ui->def_head);
    ui->def = table_new (15, 0, 5, 80, DEF_COLS, def_cols);
    table_set_datasource (ui->def, def_ds);
    table_set_cell_color (ui->def, def_get_color);
    ui->def->scrollbar = TRUE;
    ui->def->scrollbar_on_frame = TRUE;
    group_add_widget (g, ui->def);

    ui->bb = buttonbar_new ();
    buttonbar_set_label (ui->bb, 1, Q_ ("ButtonBar|Help"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 2, Q_ ("ButtonBar|Save"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 3, Q_ ("ButtonBar|Struct"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 4, Q_ ("ButtonBar|Edit"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 5, Q_ ("ButtonBar|Goto"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 6, Q_ ("ButtonBar|DefFil"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 7, Q_ ("ButtonBar|Search"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 8, Q_ ("ButtonBar|Hex"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 9, Q_ ("ButtonBar|Errors"), bb_keymap, wd);
    buttonbar_set_label (ui->bb, 10, Q_ ("ButtonBar|Quit"), bb_keymap, wd);
    group_add_widget_autopos (g, ui->bb, WIDGET (ui->bb)->pos_flags, NULL);

    widget_select (WIDGET (ui->tree));
    ui->zoom_key = tty_keyname_to_keycode ("alt-f9", NULL);
    ui_apply_colors (ui);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcstruct_run (const char *path, const char *display_name, const char *hint, off_t *last_offset)
{
    ui_t *ui;
    GError *error = NULL;
    char *def_path;
    gboolean ok = TRUE;
    char *def_hint = NULL;
    off_t start_at = -1;

    ui = g_new0 (ui_t, 1);
    ui->display_name = g_strdup (display_name != NULL ? display_name : x_basename (path));
    ui->rows = g_array_new (FALSE, FALSE, sizeof (row_t));
    ui->jumps = g_ptr_array_new ();
    ui->cell = g_string_sized_new (128);
    slv_settings_load (&ui->settings);

    ui->fr = slv_file_reader_open (path, &error);
    if (ui->fr == NULL)
    {
        message (D_ERROR, _ ("Struct look"), "%s", error->message);
        g_error_free (error);
        ok = FALSE;
        goto out;
    }

    ui->vpath = vfs_path_from_str (path);
    ui->ev.reader = &ui->fr->reader;
    ui->ev.lazy_rows = ui->settings.lazy_rows;
    ui->ev.float_format = ui->settings.float_format;

    ui_create_widgets (ui);
    ui_layout (ui);

    /* hint: "name", "name@0x1c" or "@0x1c" (the offset to open on) */
    {
        const char *at = hint != NULL ? strchr (hint, '@') : NULL;

        if (at != NULL)
        {
            def_hint = g_strndup (hint, at - hint);
            start_at = (off_t) g_ascii_strtoll (at + 1, NULL, 0);
            hint = def_hint[0] != '\0' ? def_hint : NULL;
        }
    }
    def_path = slv_load_find_def (path, hint);
    if (def_path != NULL)
    {
        if (ui_load_def (ui, def_path))
            ui_build (ui, slv_file_first_struct (ui->file), 0);
        g_free (def_path);
    }
    ui_update_heads (ui);

    if (ui->file == NULL)
    {
        /* nothing matched: ask */
        ui_cmd_select_def (ui);
    }
    if (start_at >= 0 && start_at < ui->fr->size)
        hexstrip_set_cursor (ui->hex, start_at);

    dlg_run (ui->dlg);
    if (last_offset != NULL)
        *last_offset = ui->back ? hexstrip_get_cursor (ui->hex) : -1;
    if (widget_get_state (WIDGET (ui->dlg), WST_CLOSED))
        widget_destroy (WIDGET (ui->dlg));

out:
    if (ui->grid_cols != NULL)
        g_ptr_array_free (ui->grid_cols, TRUE);
    if (ui->grid_cache != NULL)
        g_ptr_array_free (ui->grid_cache, TRUE);
    g_free (ui->grid_path);
    if (ui->locked)
        unlock_file (ui->vpath);
    vfs_path_free (ui->vpath, TRUE);
    ui_clear_jumps (ui);
    g_ptr_array_free (ui->jumps, TRUE);
    if (ui->root != NULL)
        slv_node_free (ui->root);
    if (ui->file != NULL)
        slv_file_free (ui->file);
    slv_file_reader_free (ui->fr);
    g_array_free (ui->rows, TRUE);
    g_string_free (ui->cell, TRUE);
    slv_settings_free (&ui->settings);
    g_free (ui->def_path);
    g_free (ui->search);
    g_free (ui->hex_pat);
    g_free (ui->display_name);
    g_free (ui);
    g_free (def_hint);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */
