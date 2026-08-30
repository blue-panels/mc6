/*
   mcstruct - the commands and dialogs of the screen

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

/** \file mcstruct_cmd.c
 *  \brief Source: the commands and dialogs of the screen
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "mcstruct_ui_priv.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static void ui_edit_node (ui_t *ui, slv_node_t *n);
static gboolean row_matches (ui_t *ui, const row_t *r, const char *needle);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_edit_field (ui_t *ui)
{
    if (ui->grid != NULL)
        ui_edit_node (ui,
                      ui_grid_cell_node (ui, table_get_current (ui->grid), ui->grid->current_col));
    else
    {
        row_t *r = ui_current_row (ui);

        ui_edit_node (ui, r != NULL ? r->node : NULL);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_edit_node (ui_t *ui, slv_node_t *n)
{
    char *text, *title, *err = NULL;
    unsigned char *out, *orig;
    off_t i;

    if (n == NULL || !slv_node_editable (n))
    {
        message (D_NORMAL, _ ("Edit"), _ ("This field cannot be edited"));
        return;
    }
    orig = g_malloc (n->size);
    if (ui->fr->reader.read (ui->fr->reader.ctx, n->offset, orig, n->size) != (gssize) n->size)
    {
        g_free (orig);
        return;
    }
    text = slv_node_edit_text (n, orig);
    g_free (orig);
    if (text == NULL)
    {
        message (D_NORMAL, _ ("Edit"),
                 _ ("The bytes of this field are not text, edit them in the hex zone"));
        return;
    }
    title = g_strdup_printf ("%s  (%s, %lld bytes at %08llX)", n->key != NULL ? n->key : "",
                             n->hint != NULL ? n->hint : "", (long long) n->size,
                             (unsigned long long) n->offset);
    for (;;)
    {
        char *edited;

        edited = input_dialog (title, _ ("New value:"), "mcstruct-edit", text, INPUT_COMPLETE_NONE);
        g_free (text);
        text = edited;
        if (text == NULL)
            break;
        out = g_malloc (n->size);
        if (slv_node_encode (n, text, out, &err))
        {
            orig = g_malloc (n->size);
            if (ui->fr->reader.read (ui->fr->reader.ctx, n->offset, orig, n->size)
                == (gssize) n->size)
            {
                ui_note_change (ui);
                for (i = 0; i < n->size; i++)
                    if (out[i] != orig[i])
                        slv_file_reader_set_byte (ui->fr, n->offset + i, out[i]);
            }
            g_free (orig);
            g_free (out);
            ui_refresh (ui);
            widget_draw (WIDGET (ui->hex));
            break;
        }
        g_free (out);
        message (D_ERROR, _ ("Edit"), "%s", err);
        g_free (err);
        err = NULL;
    }
    g_free (text);
    g_free (title);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
node_is_text (const slv_node_t *n)
{
    if (n->kind != SLV_NODE_FIELD || n->item == NULL)
        return FALSE;
    switch (n->item->type)
    {
    case SLV_TYPE_CHAR:
    case SLV_TYPE_CSTRING:
    case SLV_TYPE_PSTRING:
    case SLV_TYPE_STR8:
        return TRUE;
    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* the bytes of a string field in a window, one row per line of the data */
void
ui_view_text (ui_t *ui, const slv_node_t *n)
{
    Listbox *lb;
    unsigned char *buf;
    gsize len = (gsize) n->size, i, start;
    int lines = LINES - 8, cols = COLS - 8, width;
    GString *row;
    GPtrArray *rows;
    char *title;

    if (len == 0)
        return;
    buf = g_malloc (len);
    if (ui->fr->reader.read (ui->fr->reader.ctx, n->offset, buf, len) != (gssize) len)
    {
        g_free (buf);
        return;
    }
    if (n->item->type == SLV_TYPE_PSTRING)
        start = 1;
    else
        start = 0;
    if (n->item->type == SLV_TYPE_CSTRING || n->item->type == SLV_TYPE_STR8)
        while (len > start && buf[len - 1] == '\0')
            len--;

    width = cols - 4;
    rows = g_ptr_array_new_with_free_func (g_free);
    row = g_string_new ("");
    for (i = start; i <= len; i++)
    {
        unsigned char c = i < len ? buf[i] : '\n';

        if (c == '\n' || (int) row->len >= width)
        {
            g_ptr_array_add (rows, g_strdup (row->str));
            g_string_truncate (row, 0);
            if (c == '\n')
                continue;
        }
        if (c == '\r')
            continue;
        if (c == '\t')
            g_string_append_c (row, ' ');
        else if (c >= 0x20 && c != 0x7F)
            g_string_append_c (row, (char) c);
        else
            g_string_append_c (row, '.');
    }
    g_string_free (row, TRUE);
    g_free (buf);

    if ((int) rows->len < lines)
        lines = MAX ((int) rows->len, 1);
    title = g_strdup_printf ("%s: %d bytes at %08llX", n->item->name != NULL ? n->item->name : "",
                             (int) n->size, (unsigned long long) n->offset);
    lb = listbox_window_new (lines, cols, title, NULL);
    g_free (title);
    for (i = 0; i < rows->len; i++)
        listbox_add_item (lb->list, LISTBOX_APPEND_AT_END, 0, g_ptr_array_index (rows, i), NULL,
                          FALSE);
    g_ptr_array_free (rows, TRUE);
    listbox_run (lb);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
ui_input_offset (ui_t *ui, const char *title, off_t *offset)
{
    char *def, *text, *err = NULL;
    gint64 v = 0;
    const row_t *r = ui_current_row (ui);

    def = g_strdup_printf ("0x%llX", (unsigned long long) hexstrip_get_cursor (ui->hex));
    text = input_dialog (title, _ ("Offset (expression, labels of the current structure):"),
                         "mcstruct-offset", def, INPUT_COMPLETE_NONE);
    g_free (def);
    if (text == NULL)
        return FALSE;
    if (!slv_eval_calc (&ui->ev, r != NULL ? r->node : NULL, text, &v, &err))
    {
        message (D_ERROR, title, "%s", err);
        g_free (err);
        g_free (text);
        return FALSE;
    }
    g_free (text);
    if (v < 0 || v >= ui->fr->size)
    {
        message (D_ERROR, title, _ ("Offset 0x%llX is outside the file"), (unsigned long long) v);
        return FALSE;
    }
    *offset = (off_t) v;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_select_struct (ui_t *ui)
{
    Listbox *lb;
    guint i;
    const slv_def_t *chosen;
    off_t offset;

    if (ui->file == NULL)
        return;
    lb = listbox_window_new (16, 56, _ ("Select structure"), NULL);
    for (i = 0; i < ui->file->defs->len; i++)
    {
        const slv_def_t *def = g_ptr_array_index (ui->file->defs, i);

        if (def->kind != SLV_DEF_STRUCT && def->kind != SLV_DEF_TABLE)
            continue;
        if (def->hidden && !ui->settings.show_hidden)
            continue;
        {
            gssize size = slv_def_fixed_size (def);
            char *text;

            if (size >= 0)
                text = g_strdup_printf ("%s%-28s %6ld bytes", def->kind == SLV_DEF_TABLE ? ":" : "",
                                        def->name, (long) size);
            else
                text = g_strdup_printf ("%s%-28s variable", def->kind == SLV_DEF_TABLE ? ":" : "",
                                        def->name);
            listbox_add_item_take (lb->list, LISTBOX_APPEND_AT_END, 0, text, (void *) def, FALSE);
        }
    }
    chosen = listbox_run_with_data (lb, ui->root != NULL ? ui->root->def : NULL);
    if (chosen == NULL)
        return;
    {
        gssize size = slv_def_fixed_size (chosen);
        char *title;

        if (size >= 0)
            title = g_strdup_printf (_ ("%s: %ld bytes, file %lld bytes"), chosen->name,
                                     (long) size, (long long) ui->fr->size);
        else
            title = g_strdup_printf (_ ("%s: variable size, file %lld bytes"), chosen->name,
                                     (long long) ui->fr->size);
        if (!ui_input_offset (ui, title, &offset))
        {
            g_free (title);
            return;
        }
        g_free (title);
        if (size >= 0 && offset + size > ui->fr->size)
            message (D_NORMAL, _ ("Select structure"),
                     _ ("%s needs %ld bytes, only %lld left after 0x%llX"), chosen->name,
                     (long) size, (long long) (ui->fr->size - offset), (unsigned long long) offset);
        if (chosen->kind == SLV_DEF_TABLE)
        {
            /* a table needs a row count; by default as many rows as fit until the end */
            char *def_rows, *text, *end;
            gint64 rows;

            def_rows = g_strdup_printf (
                "%lld", (long long) (size > 0 ? MAX ((ui->fr->size - offset) / size, 1) : 1));
            text = input_dialog (_ ("Select structure"), _ ("Rows:"), "mcstruct-rows", def_rows,
                                 INPUT_COMPLETE_NONE);
            g_free (def_rows);
            if (text == NULL)
                return;
            rows = g_ascii_strtoll (text, &end, 0);
            g_free (text);
            if (rows <= 0)
                rows = 1;
            ui_build_rows (ui, chosen, offset, rows);
            return;
        }
    }
    ui_build (ui, chosen, offset);
}

/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_select_def (ui_t *ui)
{
    GPtrArray *defs = slv_load_list_defs ();
    Listbox *lb;
    guint i;
    const char *chosen;
    const slv_def_t *def;

    if (defs->len == 0)
    {
        message (D_ERROR, _ ("Struct look"), _ ("No def-files found on the search path"));
        g_ptr_array_free (defs, TRUE);
        return;
    }
    lb = listbox_window_new (16, 50, _ ("Select def-file"), NULL);
    for (i = 0; i < defs->len; i++)
    {
        const char *path = g_ptr_array_index (defs, i);

        listbox_add_item (lb->list, LISTBOX_APPEND_AT_END, 0, x_basename (path), (void *) path,
                          FALSE);
    }
    chosen = listbox_run_with_data (lb, NULL);
    if (chosen != NULL && ui_load_def (ui, chosen))
    {
        def = slv_file_first_struct (ui->file);
        ui_build (ui, def, 0);
    }
    g_ptr_array_free (defs, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_edit_def (ui_t *ui, int line)
{
    edit_arg_t *arg;
    const slv_def_t *def = ui->root != NULL ? ui->root->def : NULL;
    char *def_name = def != NULL ? g_strdup (def->name) : NULL;
    off_t offset = ui->root != NULL ? ui->root->offset : 0;

    if (ui->def_path == NULL)
        return;
    arg = edit_arg_new (ui->def_path, line > 0 ? line : 1);
    edit_file (arg);
    edit_arg_free (arg);

    if (ui_load_def (ui, ui->def_path))
    {
        def = def_name != NULL ? slv_file_lookup (ui->file, def_name) : NULL;
        if (def == NULL)
            def = slv_file_first_struct (ui->file);
        ui_build (ui, def, offset);
    }
    g_free (def_name);
    widget_draw (WIDGET (ui->dlg));
}

/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_errors (ui_t *ui)
{
    Listbox *lb;
    guint i;
    const slv_error_t *chosen;

    if (ui->file == NULL || ui->file->errors->len == 0)
    {
        message (D_NORMAL, _ ("Struct look"), _ ("No errors in %s"),
                 ui->def_path != NULL ? x_basename (ui->def_path) : "-");
        return;
    }
    lb = listbox_window_new (16, 70, _ ("Def-file errors"), NULL);
    for (i = 0; i < ui->file->errors->len; i++)
    {
        const slv_error_t *err = g_ptr_array_index (ui->file->errors, i);

        listbox_add_item_take (lb->list, LISTBOX_APPEND_AT_END, 0,
                               g_strdup_printf ("%5d: %s", err->line, err->message), (void *) err,
                               FALSE);
    }
    chosen = listbox_run_with_data (lb, NULL);
    if (chosen != NULL)
        ui_cmd_edit_def (ui, chosen->line);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
row_matches (ui_t *ui, const row_t *r, const char *needle)
{
    const slv_node_t *n = r->node;
    const char *fields[4];
    int i;

    fields[0] = n->key;
    fields[1] = n->text;
    fields[2] = n->legend;
    fields[3] = n->comment;
    for (i = 0; i < 4; i++)
    {
        char *lower;
        gboolean hit;

        if (fields[i] == NULL)
            continue;
        lower = g_utf8_strdown (fields[i], -1);
        hit = strstr (lower, needle) != NULL;
        g_free (lower);
        if (hit)
            return TRUE;
    }
    (void) ui;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* "DE AD BE EF", "dead" or text into bytes; NULL when not hex */
static unsigned char *
parse_hex_bytes (const char *text, gsize *len)
{
    GByteArray *out = g_byte_array_new ();
    const char *p = text;

    while (*p != '\0')
    {
        int hi, lo;

        while (*p == ' ' || *p == ',' || *p == ':')
            p++;
        if (*p == '\0')
            break;
        hi = g_ascii_xdigit_value (p[0]);
        lo = p[1] != '\0' ? g_ascii_xdigit_value (p[1]) : -1;
        if (hi < 0 || lo < 0)
        {
            g_byte_array_free (out, TRUE);
            return NULL;
        }
        {
            guint8 b = (guint8) (hi * 16 + lo);

            g_byte_array_append (out, &b, 1);
        }
        p += 2;
    }
    *len = out->len;
    if (out->len == 0)
    {
        g_byte_array_free (out, TRUE);
        return NULL;
    }
    return g_byte_array_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* F7 in the hex zone: bytes or text forward from the cursor; the hit becomes the block */
static void
ui_hex_search (ui_t *ui, gboolean next)
{
    off_t from, size = ui->fr->size, pos;
    unsigned char *buf;
    const gsize chunk = 65536;

    if (!next || ui->hex_pat == NULL)
    {
        char *text = NULL;
        int is_text = ui->hex_pat_text ? 1 : 0;
        const char *kind_items[] = { N_ ("&Hex bytes"), N_ ("&Text") };
        unsigned char *pat;
        gsize len;

        {
            /* clang-format off */
            quick_widget_t quick_widgets[] = {
                QUICK_LABELED_INPUT (N_ ("Search for:"), input_label_above, "",
                                     "mcstruct-hex-search", &text, NULL, FALSE, FALSE,
                                     INPUT_COMPLETE_NONE),
                QUICK_RADIO (2, kind_items, &is_text, NULL),
                QUICK_BUTTONS_OK_CANCEL,
                QUICK_END,
            };
            /* clang-format on */

            WRect r = { -1, -1, 0, 50 };

            quick_dialog_t qdlg = {
                .rect = r,
                .title = N_ ("Search bytes"),
                .help = NULL,
                .widgets = quick_widgets,
                .callback = NULL,
                .mouse_callback = NULL,
            };

            if (quick_dialog (&qdlg) != B_ENTER)
            {
                g_free (text);
                return;
            }
        }
        if (text == NULL || text[0] == '\0')
        {
            g_free (text);
            return;
        }
        if (is_text != 0)
        {
            len = strlen (text);
            pat = (unsigned char *) g_strdup (text);
        }
        else
        {
            pat = parse_hex_bytes (text, &len);
            if (pat == NULL)
            {
                message (D_ERROR, _ ("Search bytes"), _ ("Not a hex byte string: %s"), text);
                g_free (text);
                return;
            }
        }
        g_free (text);
        g_free (ui->hex_pat);
        ui->hex_pat = pat;
        ui->hex_pat_len = len;
        ui->hex_pat_text = is_text != 0;
    }

    from = hexstrip_get_cursor (ui->hex) + 1;
    if (ui->hex->block_len > 0 && ui->hex->block_start == hexstrip_get_cursor (ui->hex))
        from = ui->hex->block_start + 1; /* after the previous hit */
    buf = g_malloc (chunk + ui->hex_pat_len);
    for (pos = from; pos + (off_t) ui->hex_pat_len <= size; pos += chunk)
    {
        gsize want = MIN (chunk + ui->hex_pat_len, (gsize) (size - pos)), i;
        gssize got = ui->fr->reader.read (ui->fr->reader.ctx, pos, buf, want);

        if (got < (gssize) ui->hex_pat_len)
            break;
        for (i = 0; i + ui->hex_pat_len <= (gsize) got && i < chunk; i++)
            if (buf[i] == ui->hex_pat[0] && memcmp (buf + i, ui->hex_pat, ui->hex_pat_len) == 0)
            {
                g_free (buf);
                hexstrip_set_block (ui->hex, pos + (off_t) i, (off_t) ui->hex_pat_len);
                hexstrip_set_cursor (ui->hex, pos + (off_t) i);
                ui_sync_from_hex (ui);
                ui_update_heads (ui);
                widget_draw (WIDGET (ui->dlg));
                return;
            }
    }
    g_free (buf);
    message (D_NORMAL, _ ("Search bytes"), _ ("Not found"));
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-PgDn / Ctrl-PgUp: the same structure right after / before the current one */
void
ui_step_struct (ui_t *ui, int dir)
{
    off_t offset;

    if (ui->root == NULL || ui->root->def == NULL || ui->root->size <= 0)
        return;
    offset = dir > 0 ? ui->root->offset + ui->root->size : ui->root->offset - ui->root->size;
    if (offset < 0 || offset >= ui->fr->size)
    {
        message (D_NORMAL, _ ("Struct look"),
                 dir > 0 ? _ ("No room for another one after this")
                         : _ ("Already at the first one"));
        return;
    }
    ui_build_rows (ui, ui->root->def, offset, ui->root->rows);
    widget_draw (WIDGET (ui->dlg));
}

/* --------------------------------------------------------------------------------------------- */

/* Enter in the def-file zone: go to the structure the current line refers to */
void
ui_def_go_reference (ui_t *ui)
{
    int cur = table_get_current (ui->def);
    const char *line;
    char **tok;
    int i;

    if (ui->file == NULL || cur < 0 || (guint) cur >= ui->file->lines->len)
        return;
    line = g_ptr_array_index (ui->file->lines, cur);
    tok = g_strsplit_set (line, " \t", -1);
    for (i = 0; tok[i] != NULL; i++)
    {
        char *name = tok[i], *eq;
        const slv_def_t *def;

        if (name[0] == '\0' || name[0] == ';')
            continue;
        if (name[0] == ';')
            break;
        if (name[0] == ':')
            name++;
        eq = strchr (name, '=');
        if (eq != NULL)
            *eq = '\0';
        if (name[0] == '\0' || name[0] == '/')
            continue;
        def = slv_file_lookup (ui->file, name);
        if (def != NULL && def->line > 0 && (guint) def->line <= ui->file->lines->len
            && def->line - 1 != cur)
        {
            table_set_current (ui->def, def->line - 1);
            widget_draw (WIDGET (ui->def));
            ui_update_heads (ui);
            break;
        }
    }
    g_strfreev (tok);
}

/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_search (ui_t *ui, gboolean next)
{
    guint start, i, n = ui->rows->len;

    if (ui_hex_focused (ui))
    {
        ui_hex_search (ui, next);
        return;
    }
    if (!next || ui->search == NULL)
    {
        char *text;

        text = input_dialog (_ ("Search"), _ ("Field name or value:"), "mcstruct-search",
                             ui->search != NULL ? ui->search : "", INPUT_COMPLETE_NONE);
        if (text == NULL || text[0] == '\0')
        {
            g_free (text);
            return;
        }
        g_free (ui->search);
        ui->search = g_utf8_strdown (text, -1);
        g_free (text);
    }
    if (n == 0)
        return;
    start = (guint) table_get_current (ui->tree);
    for (i = 1; i <= n; i++)
    {
        guint idx = (start + i) % n;

        if (row_matches (ui, &g_array_index (ui->rows, row_t, idx), ui->search))
        {
            table_set_current (ui->tree, (int) idx);
            ui_sync_from_tree (ui);
            widget_draw (WIDGET (ui->tree));
            return;
        }
    }
    message (D_NORMAL, _ ("Search"), _ ("Not found"));
}

/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_calc (ui_t *ui)
{
    char *text, *err = NULL;
    gint64 v = 0;
    const row_t *r = ui_current_row (ui);

    text = input_dialog (_ ("Calculator"), _ ("Expression:"), "mcstruct-calc", "",
                         INPUT_COMPLETE_NONE);
    if (text == NULL || text[0] == '\0')
    {
        g_free (text);
        return;
    }
    if (!slv_eval_calc (&ui->ev, r != NULL ? r->node : NULL, text, &v, &err))
    {
        message (D_ERROR, _ ("Calculator"), "%s", err);
        g_free (err);
    }
    else
        message (D_NORMAL, _ ("Calculator"), "%s = %lld (0x%llX)", text, (long long) v,
                 (unsigned long long) v);
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

/* what Shift-F2 / Ctrl-F2 work on: the block in the hex zone when there is one and the zone is
   focused, else the current tree row or grid cell; @sname is the structure the bytes belong to */
gboolean
ui_bytes_range (ui_t *ui, const char *what, off_t *offset, off_t *size, char **name, char **sname)
{
    const slv_node_t *n = NULL;

    if (ui_hex_focused (ui) && ui->hex->block_len > 0)
    {
        *offset = ui->hex->block_start;
        *size = ui->hex->block_len;
        *name = g_strdup (_ ("block"));
        *sname = g_strdup (ui->root != NULL && ui->root->def != NULL ? ui->root->def->name : "");
        return TRUE;
    }
    if (ui->grid != NULL)
        n = ui_grid_cell_node (ui, table_get_current (ui->grid), ui->grid->current_col);
    else
    {
        const row_t *r = ui_current_row (ui);

        n = r != NULL ? r->node : NULL;
    }
    if (n == NULL || n->size <= 0 || n->kind == SLV_NODE_ERROR || n->kind == SLV_NODE_REMARK)
    {
        message (D_NORMAL, what, _ ("The current row has no bytes; in the hex zone mark a block"));
        return FALSE;
    }
    *offset = n->offset;
    *size = n->size;
    *name = g_strdup (n->key != NULL ? n->key : "");
    if (n->def != NULL)
        *sname = g_strdup (n->def->name);
    else
    {
        const slv_node_t *p = n->parent;

        while (p != NULL && p->def == NULL)
            p = p->parent;
        *sname = g_strdup (p != NULL ? p->def->name : "");
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
ui_cmd_save (ui_t *ui)
{
    GError *error = NULL;

    if (slv_file_reader_change_count (ui->fr) == 0)
        return TRUE;
    if (!slv_file_reader_save (ui->fr, &error))
    {
        message (D_ERROR, _ ("Save"), "%s", error->message);
        g_error_free (error);
        return FALSE;
    }
    ui_update_heads (ui);
    widget_draw (WIDGET (ui->hex));
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_help (void)
{
    ev_help_t event_data = { MC_PLUGIN_DIR "/mcstruct_panel.hlp", "[mcstruct]", "[main]" };

    mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
}

/* --------------------------------------------------------------------------------------------- */

void
ui_cmd_quit (ui_t *ui)
{
    if (slv_file_reader_change_count (ui->fr) > 0)
    {
        int rc;

        rc = query_dialog (_ ("Quit"), _ ("File was modified. Save changes?"), D_NORMAL, 3,
                           _ ("&Yes"), _ ("&No"), _ ("&Cancel"));
        if (rc == 0 && !ui_cmd_save (ui))
            return;
        if (rc == 2 || rc == -1)
            return;
        if (rc == 1)
            slv_file_reader_discard (ui->fr);
    }
    ui->quit = TRUE;
    dlg_close (ui->dlg);
}

/* --------------------------------------------------------------------------------------------- */
