/*
   Full-screen widget groups filled and driven by a runtime package.

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

/** \file runtime-screen.c
 *  \brief Source: a screen is a full-screen dialog whose widgets a runtime package
 *  describes, whose table rows it serves on request, and whose keys it hears about.
 *
 *  The status line is at the top, the button bar at the bottom.  Between them
 *  a grid: rows of cells, one control per cell.  A row is so many lines or a
 *  share of the lines left, a cell so many columns or a share of the columns
 *  left; the grid is laid out again when the terminal changes size.  Every
 *  table pulls its rows from the runtime page by page and keeps the pages it
 *  saw recently.  A text cell is a viewer widget, so it scrolls and wraps.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/event.h"
#include "lib/keybind.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/tty/color.h"
#include "lib/tty/key.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "viewer/mcviewer.h"

#include "runtime-screen.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* Commands of the button bar: one per key of the keys list. */
#define SCREEN_ACTION_BASE 32000L
#define SCREEN_ACTION(i)   (SCREEN_ACTION_BASE + (long) (i))

#define SCREEN_PAGE_SIZE   256
#define SCREEN_PAGES_KEPT  64
#define SCREEN_MAX_ROWS    64
#define SCREEN_MAX_CELLS   16

/*** file scope type declarations ****************************************************************/

typedef struct runtime_screen runtime_screen_t;

typedef struct
{
    guint rows;
    char **texts;  /* rows * columns */
    char **colors; /* rows * columns, NULL entries for the row's color */
} screen_page_t;

/* A table cell: the widget and the rows it pulled. */
typedef struct
{
    runtime_screen_t *scr;
    char *id;
    WTable *table;
    guint ncols;
    gint64 row_count; /* -1: unknown */
    gint64 known_rows;
    gboolean at_end;
    guint page_size;
    GHashTable *pages; /* page index -> screen_page_t * */
    GQueue lru;        /* page indexes, most recent last */
    gint64 last_row;   /* the row reported as current */
} screen_table_t;

typedef struct
{
    char *id;
    mc_runtime_dialog_control_type_t type;
    guint width, weight;
    Widget *widget;
    screen_table_t *table; /* for a table cell */
} screen_cell_t;

typedef struct
{
    guint height, weight;
    GPtrArray *cells; /* screen_cell_t * */
    int y, lines;     /* from the last layout */
} screen_row_t;

/* The status line: the text in the viewer's status color, the whole width. */
typedef struct
{
    Widget widget;
    char *text;
} screen_status_t;

struct runtime_screen
{
    guint64 id;
    mc_runtime_plugin_context_t *context;
    const mc_runtime_screen_descriptor_t *descriptor;
    WDialog *dlg;
    screen_status_t *status;
    WButtonBar *bar;
    GPtrArray *rows;       /* screen_row_t * */
    GHashTable *by_id;     /* control id -> screen_cell_t * */
    GHashTable *by_widget; /* Widget * -> screen_cell_t * */
    GHashTable *color_pairs;

    /* the keys list, by index */
    int *key_codes;
    global_keymap_t *bar_keymap;
    gboolean closing;
};

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GHashTable *runtime_screens = NULL; /* id -> runtime_screen_t * */

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
screen_error (const char **error, const char *text)
{
    if (error != NULL)
        *error = text;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static runtime_screen_t *
screen_lookup (guint64 id)
{
    if (runtime_screens == NULL)
        return NULL;
    return g_hash_table_lookup (runtime_screens, &id);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
screen_dispatch (runtime_screen_t *scr, mc_runtime_screen_operation_t operation,
                 mc_runtime_screen_request_t *request, mc_runtime_screen_response_t *response)
{
    const char *error = NULL;
    gboolean ok;

    if (scr->descriptor->dispatch == NULL)
        return FALSE;
    request->struct_size = sizeof (*request);
    request->operation_version = 1;
    memset (response, 0, sizeof (*response));
    response->struct_size = sizeof (*response);
    ok = scr->descriptor->dispatch (scr->context, scr->id, operation, request, response, &error);
    if (ok && response->close && !scr->closing)
    {
        scr->closing = TRUE;
        if (scr->dlg != NULL)
            dlg_close (scr->dlg);
    }
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_response_clear (runtime_screen_t *scr, mc_runtime_screen_response_t *response)
{
    if (scr->descriptor->response_free != NULL)
        scr->descriptor->response_free (scr->context, response);
    memset (response, 0, sizeof (*response));
}

/* --------------------------------------------------------------------------------------------- */
/*** the status line *****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
screen_status_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    screen_status_t *status = (screen_status_t *) w;

    switch (msg)
    {
    case MSG_DRAW:
        tty_setcolor (STATUSBAR_COLOR);
        widget_gotoyx (w, 0, 0);
        tty_print_string (
            str_fit_to_term (status->text != NULL ? status->text : "", w->rect.cols, J_LEFT));
        return MSG_HANDLED;

    case MSG_DESTROY:
        g_free (status->text);
        status->text = NULL;
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static screen_status_t *
screen_status_new (const char *text)
{
    screen_status_t *status = g_new0 (screen_status_t, 1);
    WRect r = { 0, 0, 1, 1 };

    widget_init (WIDGET (status), &r, screen_status_callback, NULL);
    status->text = g_strdup (text != NULL ? text : "");
    return status;
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_status_set (screen_status_t *status, const char *text)
{
    g_free (status->text);
    status->text = g_strdup (text != NULL ? text : "");
    widget_draw (WIDGET (status));
}

/* --------------------------------------------------------------------------------------------- */
/*** tables: pages pulled from the runtime *******************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
screen_page_free (screen_page_t *page, guint ncols)
{
    guint i;

    if (page == NULL)
        return;
    for (i = 0; i < page->rows * ncols; i++)
    {
        g_free (page->texts[i]);
        g_free (page->colors[i]);
    }
    g_free (page->texts);
    g_free (page->colors);
    g_free (page);
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_table_clear_pages (screen_table_t *st)
{
    GHashTableIter iter;
    gpointer value;

    if (st->pages == NULL)
        return;
    g_hash_table_iter_init (&iter, st->pages);
    while (g_hash_table_iter_next (&iter, NULL, &value))
        screen_page_free (value, st->ncols);
    g_hash_table_remove_all (st->pages);
    g_queue_clear (&st->lru);
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_table_free (screen_table_t *st)
{
    if (st == NULL)
        return;
    screen_table_clear_pages (st);
    if (st->pages != NULL)
        g_hash_table_destroy (st->pages);
    g_free (st->id);
    g_free (st);
}

/* --------------------------------------------------------------------------------------------- */

/* The page with this index, asked from the runtime when it is not kept. */
static screen_page_t *
screen_fetch_page (screen_table_t *st, gint64 page_index)
{
    runtime_screen_t *scr = st->scr;
    screen_page_t *page;
    mc_runtime_screen_request_t request = { 0 };
    mc_runtime_screen_response_t response;
    guint i, count;

    page = g_hash_table_lookup (st->pages, &page_index);
    if (page != NULL)
        return page;
    if (st->at_end && page_index * (gint64) st->page_size >= st->known_rows)
        return NULL;

    request.control_id = st->id;
    request.first = page_index * (gint64) st->page_size;
    request.count = st->page_size;
    if (!screen_dispatch (scr, MC_RUNTIME_SCREEN_ROWS, &request, &response))
        return NULL;

    page = g_new0 (screen_page_t, 1);
    count = response.cells != NULL ? response.rows_count : 0;
    page->rows = count;
    page->texts = g_new0 (char *, (gsize) count * st->ncols + 1);
    page->colors = g_new0 (char *, (gsize) count * st->ncols + 1);
    for (i = 0; i < count * st->ncols; i++)
    {
        guint row = i / st->ncols, col = i % st->ncols;

        if (col < response.columns_count)
        {
            const mc_runtime_screen_cell_t *cell =
                &response.cells[row * response.columns_count + col];

            page->texts[i] = g_strdup (cell->text != NULL ? cell->text : "");
            page->colors[i] = g_strdup (cell->color);
        }
        else
            page->texts[i] = g_strdup ("");
    }
    screen_response_clear (scr, &response);

    if (st->row_count < 0)
    {
        if (count < st->page_size)
            st->at_end = TRUE;
        st->known_rows = MAX (st->known_rows, request.first + (gint64) count);
    }

    {
        gint64 *key = g_new (gint64, 1);

        *key = page_index;
        g_hash_table_insert (st->pages, key, page);
        g_queue_push_tail (&st->lru, key);
    }
    while (g_queue_get_length (&st->lru) > SCREEN_PAGES_KEPT)
    {
        gint64 *old = g_queue_pop_head (&st->lru);
        screen_page_t *old_page = g_hash_table_lookup (st->pages, old);

        screen_page_free (old_page, st->ncols);
        g_hash_table_remove (st->pages, old);
    }
    return page;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
screen_cell_text (screen_table_t *st, gint64 row, int col, const char **color)
{
    gint64 page_index = row / (gint64) st->page_size;
    screen_page_t *page = screen_fetch_page (st, page_index);
    guint offset;

    if (color != NULL)
        *color = NULL;
    if (page == NULL || col < 0 || (guint) col >= st->ncols)
        return "";
    offset = (guint) (row - page_index * (gint64) st->page_size);
    if (offset >= page->rows)
        return "";
    if (color != NULL)
        *color = page->colors[offset * st->ncols + (guint) col];
    return page->texts[offset * st->ncols + (guint) col];
}

/* --------------------------------------------------------------------------------------------- */

static int
screen_table_get_nrows (const void *data)
{
    const screen_table_t *st = data;
    gint64 rows = st->row_count >= 0 ? st->row_count : st->known_rows;

    return (int) MIN (rows, G_MAXINT);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
screen_table_get_text (const void *data, int row, int col)
{
    return screen_cell_text ((screen_table_t *) data, row, col, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
screen_text_is_checked (const char *text)
{
    return text != NULL
        && (strcmp (text, "1") == 0 || g_ascii_strcasecmp (text, "x") == 0
            || g_ascii_strcasecmp (text, "true") == 0 || g_ascii_strcasecmp (text, "yes") == 0);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
screen_table_get_checked (const void *data, int row, int col)
{
    return screen_text_is_checked (screen_cell_text ((screen_table_t *) data, row, col, NULL));
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_table_set_checked (void *data, int row, int col, gboolean val)
{
    screen_table_t *st = data;
    mc_runtime_screen_request_t request = { 0 };
    mc_runtime_screen_response_t response;
    gint64 page_index = row / (gint64) st->page_size;
    screen_page_t *page;

    request.control_id = st->id;
    request.row = row;
    request.column = col;
    request.value = val;
    if (!screen_dispatch (st->scr, MC_RUNTIME_SCREEN_SET_CHECKED, &request, &response))
        return;
    screen_response_clear (st->scr, &response);

    /* the page shows the new state without a round trip */
    page = g_hash_table_lookup (st->pages, &page_index);
    if (page != NULL)
    {
        guint offset = (guint) (row - page_index * (gint64) st->page_size);

        if (offset < page->rows)
        {
            g_free (page->texts[offset * st->ncols + (guint) col]);
            page->texts[offset * st->ncols + (guint) col] = g_strdup (val ? "1" : "0");
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* "fg;bg;attrs" or "fg" as a skin color pair, allocated once per spelling; a
   color without a background keeps the background of the cells around it. */
static int
screen_table_cell_color (void *data, int row, int col)
{
    screen_table_t *st = data;
    runtime_screen_t *scr = st->scr;
    const char *spec = NULL;
    gpointer cached;

    screen_cell_text (st, row, col, &spec);
    if (spec == NULL || spec[0] == '\0')
        return -1;
    if (g_hash_table_lookup_extended (scr->color_pairs, spec, NULL, &cached))
        return GPOINTER_TO_INT (cached);

    {
        char **parts = g_strsplit (spec, ";", 3);
        tty_color_pair_t pair = { 0 };
        int index;

        pair.fg = parts[0] != NULL && parts[0][0] != '\0' ? parts[0] : NULL;
        pair.bg = parts[0] != NULL && parts[1] != NULL && parts[1][0] != '\0' ? parts[1] : NULL;
        pair.attrs = parts[0] != NULL && parts[1] != NULL && parts[2] != NULL ? parts[2] : NULL;
        if (pair.bg == NULL)
        {
            /* the background of the cells around it; the name is a constant of
               the color table, and tty_try_alloc_color_pair() only reads it */
            const int *colors = widget_get_colors (WIDGET (st->table));

            pair.bg = (char *) tty_color_pair_background (colors[DLG_COLOR_NORMAL]);
        }
        index = tty_try_alloc_color_pair (&pair, FALSE);
        g_strfreev (parts);
        g_hash_table_insert (scr->color_pairs, g_strdup (spec), GINT_TO_POINTER (index));
        return index;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The pages under the rows about to be drawn, and the one after them. */
static void
screen_table_prefetch (void *data, int first, int count)
{
    screen_table_t *st = data;
    gint64 page_index = first / (gint64) st->page_size;
    gint64 last_page = (first + count) / (gint64) st->page_size + 1;

    for (; page_index <= last_page; page_index++)
    {
        if (st->row_count >= 0 && page_index * (gint64) st->page_size >= st->row_count)
            break;
        if (screen_fetch_page (st, page_index) == NULL)
            break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** events **************************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static screen_cell_t *
screen_focused_cell (const runtime_screen_t *scr)
{
    WGroup *g = GROUP (scr->dlg);

    if (g->current == NULL)
        return NULL;
    return g_hash_table_lookup (scr->by_widget, g->current->data);
}

/* --------------------------------------------------------------------------------------------- */

/* Where the user is: the focused control, and its row and column for a table. */
static void
screen_fill_position (const runtime_screen_t *scr, const screen_cell_t *cell,
                      mc_runtime_screen_request_t *request)
{
    if (cell == NULL)
        cell = screen_focused_cell (scr);
    request->control_id = cell != NULL ? cell->id : NULL;
    request->row = -1;
    request->column = -1;
    if (cell != NULL && cell->table != NULL)
    {
        request->row = table_get_current (cell->table->table);
        request->column = cell->table->table->current_col;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_show_help (runtime_screen_t *scr)
{
    const mc_runtime_screen_t *spec = scr->descriptor->spec;
    ev_help_t event_data = { spec->help_file, spec->help_node, NULL };

    if (spec->help_node == NULL)
        return;
    mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
screen_run_action (runtime_screen_t *scr, guint index)
{
    const mc_runtime_screen_t *spec = scr->descriptor->spec;
    mc_runtime_screen_request_t request = { 0 };
    mc_runtime_screen_response_t response;
    const char *action;

    if (index >= spec->keys_count)
        return MSG_NOT_HANDLED;
    action = spec->keys[index].action;
    if (action == NULL || strcmp (action, "close") == 0)
    {
        dlg_close (scr->dlg);
        return MSG_HANDLED;
    }
    if (strcmp (action, "help") == 0)
    {
        screen_show_help (scr);
        return MSG_HANDLED;
    }
    screen_fill_position (scr, NULL, &request);
    request.action = action;
    if (screen_dispatch (scr, MC_RUNTIME_SCREEN_ACTION, &request, &response))
        screen_response_clear (scr, &response);
    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

/* A key of the keys list, before any widget sees it. */
static cb_ret_t
screen_handle_listed_key (runtime_screen_t *scr, int key)
{
    const mc_runtime_screen_t *spec = scr->descriptor->spec;
    guint i;

    for (i = 0; i < spec->keys_count; i++)
        if (scr->key_codes[i] != 0 && scr->key_codes[i] == key)
            return screen_run_action (scr, i);
    if (key == KEY_F (1))
    {
        screen_show_help (scr);
        return MSG_HANDLED;
    }
    return MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

/* A key no widget took: the runtime's. */
static cb_ret_t
screen_handle_key (runtime_screen_t *scr, int key)
{
    mc_runtime_screen_request_t request = { 0 };
    mc_runtime_screen_response_t response;
    gboolean handled = FALSE;

    screen_fill_position (scr, NULL, &request);
    request.key = key;
    request.key_name = tty_keycode_to_keyname (key);
    if (screen_dispatch (scr, MC_RUNTIME_SCREEN_KEY, &request, &response))
    {
        handled = response.handled;
        screen_response_clear (scr, &response);
    }
    g_free ((char *) request.key_name);
    return handled ? MSG_HANDLED : MSG_NOT_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_notify (runtime_screen_t *scr, Widget *sender, int parm)
{
    screen_cell_t *cell = g_hash_table_lookup (scr->by_widget, sender);
    mc_runtime_screen_request_t request = { 0 };
    mc_runtime_screen_response_t response;

    if (cell == NULL || cell->table == NULL)
        return;
    screen_fill_position (scr, cell, &request);
    if (parm == CK_Enter)
    {
        if (screen_dispatch (scr, MC_RUNTIME_SCREEN_ENTER, &request, &response))
            screen_response_clear (scr, &response);
    }
    else if (request.row != cell->table->last_row)
    {
        cell->table->last_row = request.row;
        if (screen_dispatch (scr, MC_RUNTIME_SCREEN_ROW_CHANGED, &request, &response))
            screen_response_clear (scr, &response);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** layout **************************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Places every widget: the status line on top, the button bar at the bottom,
   the rows between them by height or weight, the cells by width or weight
   with a column between them for a line. */
static void
screen_layout (runtime_screen_t *scr)
{
    const WRect *r = &WIDGET (scr->dlg)->rect;
    int cols = r->cols, lines = r->lines;
    int body_top = 1, body_lines = MAX (lines - 2, 0);
    int fixed = 0, spare, y;
    guint wsum = 0, i;

    widget_set_size (WIDGET (scr->status), 0, 0, 1, cols);

    for (i = 0; i < scr->rows->len; i++)
    {
        screen_row_t *row = g_ptr_array_index (scr->rows, i);

        if (row->height != 0)
            fixed += (int) row->height;
        else
            wsum += MAX (row->weight, 1);
    }
    spare = MAX (body_lines - fixed, 0);
    y = body_top;
    for (i = 0; i < scr->rows->len; i++)
    {
        screen_row_t *row = g_ptr_array_index (scr->rows, i);
        int cfixed = 0, cspare, x = 0;
        guint cwsum = 0, j;

        if (row->height != 0)
            row->lines = (int) row->height;
        else
        {
            guint weight = MAX (row->weight, 1);

            row->lines = spare * (int) weight / (int) wsum;
            spare -= row->lines;
            wsum -= weight;
        }
        row->y = y;
        y += row->lines;

        for (j = 0; j < row->cells->len; j++)
        {
            screen_cell_t *cell = g_ptr_array_index (row->cells, j);

            if (cell->width != 0)
                cfixed += (int) cell->width;
            else
                cwsum += MAX (cell->weight, 1);
        }
        /* a line between the cells */
        cspare = MAX (cols - cfixed - (int) (row->cells->len - 1), 0);
        for (j = 0; j < row->cells->len; j++)
        {
            screen_cell_t *cell = g_ptr_array_index (row->cells, j);
            int width;

            if (cell->width != 0)
                width = (int) cell->width;
            else
            {
                guint weight = MAX (cell->weight, 1);

                width = cspare * (int) weight / (int) cwsum;
                cspare -= width;
                cwsum -= weight;
            }
            widget_set_size (cell->widget, row->y, x, MAX (row->lines, 1), MAX (width, 1));
            x += width + 1;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The lines between the cells of a row. */
static void
screen_draw_lines (runtime_screen_t *scr)
{
    const int *colors = widget_get_colors (WIDGET (scr->dlg));
    guint i;

    tty_setcolor (colors[DLG_COLOR_FRAME]);
    for (i = 0; i < scr->rows->len; i++)
    {
        screen_row_t *row = g_ptr_array_index (scr->rows, i);
        guint j;

        for (j = 0; j + 1 < row->cells->len; j++)
        {
            screen_cell_t *cell = g_ptr_array_index (row->cells, j);
            const WRect *cr = &cell->widget->rect;

            tty_draw_vline (row->y, cr->x + cr->cols, mc_tty_frm[MC_TTY_FRM_VERT], row->lines);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
screen_dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);
    runtime_screen_t *scr = h->data.p;

    switch (msg)
    {
    case MSG_ACTION:
        if (parm >= SCREEN_ACTION_BASE && parm < SCREEN_ACTION_BASE + BUTTONBAR_LABELS_NUM)
            return screen_run_action (scr, (guint) (parm - SCREEN_ACTION_BASE));
        if (parm == CK_Help)
        {
            screen_show_help (scr);
            return MSG_HANDLED;
        }
        return dlg_default_callback (w, sender, msg, parm, data);

    case MSG_KEY:
        return screen_handle_listed_key (scr, parm);

    case MSG_UNHANDLED_KEY:
        if (screen_handle_key (scr, parm) == MSG_HANDLED)
            return MSG_HANDLED;
        return dlg_default_callback (w, sender, msg, parm, data);

    case MSG_NOTIFY:
        screen_notify (scr, sender, parm);
        return MSG_HANDLED;

    case MSG_DRAW:
    {
        cb_ret_t ret = dlg_default_callback (w, sender, msg, parm, data);

        screen_draw_lines (scr);
        return ret;
    }

    case MSG_RESIZE:
    {
        cb_ret_t ret = dlg_default_callback (w, sender, msg, parm, data);
        mc_runtime_screen_request_t request = { 0 };
        mc_runtime_screen_response_t response;

        screen_layout (scr);
        request.columns = (guint) w->rect.cols;
        request.lines = (guint) w->rect.lines;
        if (screen_dispatch (scr, MC_RUNTIME_SCREEN_RESIZE, &request, &response))
            screen_response_clear (scr, &response);
        return ret;
    }

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
screen_get_title (const WDialog *h, const ssize_t width)
{
    const runtime_screen_t *scr = h->data.p;
    const char *title = scr->descriptor->spec->title != NULL ? scr->descriptor->spec->title : "";

    return g_strndup (title, width > 0 ? (gsize) width : strlen (title));
}

/* --------------------------------------------------------------------------------------------- */
/*** building ************************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static screen_table_t *
screen_table_new (runtime_screen_t *scr, const mc_runtime_dialog_control_t *control,
                  const char **error)
{
    const mc_runtime_screen_table_t *table = control->table;
    screen_table_t *st;
    table_column_def_t *defs;
    const char **titles;
    table_datasource_t ds = { 0 };
    guint i;

    if (table == NULL || table->columns_count == 0 || table->columns_count > 256)
    {
        screen_error (error, "invalid_table");
        return NULL;
    }
    st = g_new0 (screen_table_t, 1);
    st->scr = scr;
    st->id = g_strdup (control->id);
    st->ncols = table->columns_count;
    defs = g_new0 (table_column_def_t, st->ncols);
    titles = g_new0 (const char *, st->ncols + 1);
    for (i = 0; i < st->ncols; i++)
    {
        const mc_runtime_screen_column_t *column = &table->columns[i];

        defs[i].width = 0;
        defs[i].align = column->align == MC_RUNTIME_PANEL_ALIGN_RIGHT ? J_RIGHT
            : column->align == MC_RUNTIME_PANEL_ALIGN_CENTER          ? J_CENTER
                                                                      : J_LEFT;
        defs[i].type =
            column->type == MC_RUNTIME_SCREEN_COLUMN_CHECK ? TABLE_COL_CHECK : TABLE_COL_TEXT;
        titles[i] = column->title != NULL ? column->title : column->id;
    }
    st->table = table_new (0, 0, 1, 1, (int) st->ncols, defs);
    g_free (defs);
    for (i = 0; i < st->ncols; i++)
        table_set_column_sizing (st->table, (int) i, (int) MAX (table->columns[i].min_width, 1),
                                 table->columns[i].expands);
    table_set_header (st->table, titles);
    g_free (titles);
    table_set_scroll_columns (st->table, TRUE);
    table_set_prefetch (st->table, screen_table_prefetch);
    table_set_cell_color (st->table, screen_table_cell_color);
    st->table->scrollbar = TRUE;

    st->row_count = table->row_count < 0 ? -1 : table->row_count;
    st->known_rows = st->row_count >= 0 ? st->row_count : 0;
    st->page_size = table->page_size != 0 ? table->page_size : SCREEN_PAGE_SIZE;
    st->pages = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, NULL);
    g_queue_init (&st->lru);
    st->last_row = -1;

    ds.get_nrows = screen_table_get_nrows;
    ds.get_text = screen_table_get_text;
    ds.get_checked = screen_table_get_checked;
    ds.set_checked = screen_table_set_checked;
    ds.data = st;
    table_set_datasource (st->table, ds);
    return st;
}

/* --------------------------------------------------------------------------------------------- */

/* A text cell is a viewer in its panel shape: it scrolls, wraps and draws
   its own frame, and never touches the button bar. */
static Widget *
screen_text_new (const char *text)
{
    WRect r = { 0, 0, 1, 1 };
    WView *view = mcview_new (&r, TRUE);

    mcview_set_embedded (view, TRUE);
    mcview_load_text (view, text != NULL ? text : "");
    return WIDGET (view);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
screen_add_cell (runtime_screen_t *scr, screen_row_t *row,
                 const mc_runtime_screen_cell_spec_t *spec, const char **error)
{
    const mc_runtime_dialog_control_t *control = spec->control;
    screen_cell_t *cell;
    Widget *w = NULL;
    screen_table_t *st = NULL;

    if (control == NULL || control->id == NULL || control->id[0] == '\0')
        return screen_error (error, "invalid_control");
    if (g_hash_table_contains (scr->by_id, control->id))
        return screen_error (error, "duplicate_control");

    switch (control->type)
    {
    case MC_RUNTIME_DIALOG_LABEL:
    case MC_RUNTIME_DIALOG_STATUS:
        w = WIDGET (label_new (0, 0, control->text != NULL ? control->text : ""));
        break;
    case MC_RUNTIME_DIALOG_SEPARATOR:
        w = WIDGET (hline_new (0, 0, 1));
        break;
    case MC_RUNTIME_DIALOG_CHECKBOX:
        w = WIDGET (
            check_new (0, 0, control->checked, control->label != NULL ? control->label : ""));
        break;
    case MC_RUNTIME_DIALOG_INPUT:
        w = WIDGET (input_new (0, 0, input_colors, 1, control->value != NULL ? control->value : "",
                               NULL, INPUT_COMPLETE_NONE));
        break;
    case MC_RUNTIME_DIALOG_TEXT:
        w = screen_text_new (control->text);
        break;
    case MC_RUNTIME_DIALOG_TABLE:
        st = screen_table_new (scr, control, error);
        if (st == NULL)
            return FALSE;
        w = WIDGET (st->table);
        break;
    default:
        return screen_error (error, "unsupported_control");
    }

    cell = g_new0 (screen_cell_t, 1);
    cell->id = g_strdup (control->id);
    cell->type = control->type;
    cell->width = spec->width;
    cell->weight = spec->weight;
    cell->widget = w;
    cell->table = st;
    g_ptr_array_add (row->cells, cell);
    g_hash_table_insert (scr->by_id, cell->id, cell);
    g_hash_table_insert (scr->by_widget, w, cell);
    group_add_widget_autopos (GROUP (scr->dlg), w, WPOS_KEEP_DEFAULT, NULL);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
screen_build (runtime_screen_t *scr, const char **error)
{
    const mc_runtime_screen_t *spec = scr->descriptor->spec;
    guint i;

    scr->dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, dialog_colors,
                           screen_dialog_callback, NULL, NULL, spec->title);
    scr->dlg->data.p = scr;
    scr->dlg->get_title = screen_get_title;

    scr->status = screen_status_new (spec->status);
    group_add_widget_autopos (GROUP (scr->dlg), scr->status, WPOS_KEEP_DEFAULT, NULL);

    for (i = 0; i < spec->rows_count; i++)
    {
        const mc_runtime_screen_row_t *row_spec = &spec->rows[i];
        screen_row_t *row;
        guint j;

        if (row_spec->cells_count == 0 || row_spec->cells_count > SCREEN_MAX_CELLS)
            return screen_error (error, "invalid_row");
        row = g_new0 (screen_row_t, 1);
        row->height = row_spec->height;
        row->weight = row_spec->weight;
        row->cells = g_ptr_array_new ();
        g_ptr_array_add (scr->rows, row);
        for (j = 0; j < row_spec->cells_count; j++)
            if (!screen_add_cell (scr, row, &row_spec->cells[j], error))
                return FALSE;
    }

    /* the keys: F1..F10 on the button bar, every one by its code */
    scr->key_codes = g_new0 (int, spec->keys_count + 1);
    scr->bar_keymap = g_new0 (global_keymap_t, spec->keys_count + 1);
    scr->bar = buttonbar_new ();
    for (i = 0; i < spec->keys_count; i++)
    {
        const mc_runtime_screen_key_t *key = &spec->keys[i];
        int code = key->key != NULL ? tty_keyname_to_keycode (key->key, NULL) : 0;

        scr->key_codes[i] = code;
        scr->bar_keymap[i].key = code;
        scr->bar_keymap[i].command = SCREEN_ACTION (i);
    }
    for (i = 0; i < spec->keys_count; i++)
    {
        int n;

        for (n = 1; n <= BUTTONBAR_LABELS_NUM; n++)
            if (scr->key_codes[i] == KEY_F (n))
                buttonbar_set_label (scr->bar, n,
                                     spec->keys[i].label != NULL ? spec->keys[i].label
                                                                 : spec->keys[i].action,
                                     scr->bar_keymap, WIDGET (scr->dlg));
    }
    group_add_widget_autopos (GROUP (scr->dlg), scr->bar, WIDGET (scr->bar)->pos_flags, NULL);

    screen_layout (scr);

    if (spec->focus != NULL)
    {
        screen_cell_t *cell = g_hash_table_lookup (scr->by_id, spec->focus);

        if (cell != NULL)
            widget_select (cell->widget);
    }
    else
    {
        /* the first table, else the first cell */
        screen_cell_t *first = NULL;

        for (i = 0; i < scr->rows->len && first == NULL; i++)
        {
            screen_row_t *row = g_ptr_array_index (scr->rows, i);
            guint j;

            for (j = 0; j < row->cells->len; j++)
            {
                screen_cell_t *cell = g_ptr_array_index (row->cells, j);

                if (cell->table != NULL)
                {
                    first = cell;
                    break;
                }
            }
        }
        if (first != NULL)
            widget_select (first->widget);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
screen_free (runtime_screen_t *scr)
{
    guint i;

    if (scr == NULL)
        return;
    for (i = 0; scr->rows != NULL && i < scr->rows->len; i++)
    {
        screen_row_t *row = g_ptr_array_index (scr->rows, i);
        guint j;

        for (j = 0; j < row->cells->len; j++)
        {
            screen_cell_t *cell = g_ptr_array_index (row->cells, j);

            screen_table_free (cell->table);
            g_free (cell->id);
            g_free (cell);
        }
        g_ptr_array_free (row->cells, TRUE);
        g_free (row);
    }
    if (scr->rows != NULL)
        g_ptr_array_free (scr->rows, TRUE);
    if (scr->by_id != NULL)
        g_hash_table_destroy (scr->by_id);
    if (scr->by_widget != NULL)
        g_hash_table_destroy (scr->by_widget);
    if (scr->color_pairs != NULL)
        g_hash_table_destroy (scr->color_pairs);
    g_free (scr->key_codes);
    g_free (scr->bar_keymap);
    g_free (scr);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
runtime_screen_run (mc_runtime_plugin_context_t *context,
                    const mc_runtime_screen_descriptor_t *descriptor, const char **error)
{
    runtime_screen_t *scr;
    mc_runtime_screen_request_t request = { 0 };
    mc_runtime_screen_response_t response;
    guint64 *key;

    if (error != NULL)
        *error = NULL;
    if (descriptor == NULL || descriptor->struct_size < sizeof (*descriptor)
        || descriptor->spec == NULL || descriptor->dispatch == NULL)
        return screen_error (error, "invalid_argument");
    if (descriptor->spec->rows_count == 0 || descriptor->spec->rows_count > SCREEN_MAX_ROWS
        || descriptor->spec->keys_count > 64)
        return screen_error (error, "invalid_argument");
    if (screen_lookup (descriptor->screen_id) != NULL)
        return screen_error (error, "already_running");
    if (runtime_screens == NULL)
        runtime_screens = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, NULL);

    scr = g_new0 (runtime_screen_t, 1);
    scr->id = descriptor->screen_id;
    scr->context = context;
    scr->descriptor = descriptor;
    scr->rows = g_ptr_array_new ();
    scr->by_id = g_hash_table_new (g_str_hash, g_str_equal);
    scr->by_widget = g_hash_table_new (g_direct_hash, g_direct_equal);
    scr->color_pairs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    key = g_new (guint64, 1);
    *key = scr->id;
    g_hash_table_insert (runtime_screens, key, scr);

    if (!screen_build (scr, error))
    {
        if (scr->dlg != NULL)
            widget_destroy (WIDGET (scr->dlg));
        g_hash_table_remove (runtime_screens, &scr->id);
        screen_free (scr);
        return FALSE;
    }

    dlg_run (scr->dlg);

    scr->closing = TRUE;
    if (screen_dispatch (scr, MC_RUNTIME_SCREEN_CLOSE, &request, &response))
        screen_response_clear (scr, &response);

    widget_destroy (WIDGET (scr->dlg));
    scr->dlg = NULL;
    g_hash_table_remove (runtime_screens, &scr->id);
    screen_free (scr);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
runtime_screen_update (mc_runtime_plugin_context_t *context, guint64 screen_id,
                       const mc_runtime_screen_patch_t *patch, const char **error)
{
    runtime_screen_t *scr = screen_lookup (screen_id);
    screen_cell_t *cell;

    (void) context;
    if (error != NULL)
        *error = NULL;
    if (scr == NULL || scr->dlg == NULL)
        return screen_error (error, "closed");
    if (patch == NULL || patch->struct_size < sizeof (*patch))
        return screen_error (error, "invalid_argument");

    /* no control: the status line */
    if (patch->control_id == NULL || patch->control_id[0] == '\0')
    {
        if (patch->text != NULL)
            screen_status_set (scr->status, patch->text);
        return TRUE;
    }

    cell = g_hash_table_lookup (scr->by_id, patch->control_id);
    if (cell == NULL)
        return screen_error (error, "no_such_control");

    switch (cell->type)
    {
    case MC_RUNTIME_DIALOG_TABLE:
    {
        screen_table_t *st = cell->table;

        if (patch->invalidate)
        {
            screen_table_clear_pages (st);
            st->at_end = FALSE;
            if (st->row_count < 0)
                st->known_rows = 0;
        }
        if (patch->has_row_count)
        {
            st->row_count = patch->row_count < 0 ? -1 : patch->row_count;
            st->known_rows = st->row_count >= 0 ? st->row_count : 0;
            st->at_end = FALSE;
        }
        table_set_current (st->table,
                           patch->has_row ? (int) MIN (patch->row, G_MAXINT)
                                          : table_get_current (st->table));
        break;
    }
    case MC_RUNTIME_DIALOG_LABEL:
    case MC_RUNTIME_DIALOG_STATUS:
        if (patch->text != NULL)
        {
            const WRect *r = &cell->widget->rect;

            label_set_text (LABEL (cell->widget), patch->text);
            widget_set_size (cell->widget, r->y, r->x, r->lines, r->cols);
        }
        break;
    case MC_RUNTIME_DIALOG_TEXT:
        if (patch->text != NULL)
            mcview_load_text ((WView *) cell->widget, patch->text);
        break;
    case MC_RUNTIME_DIALOG_INPUT:
        if (patch->value != NULL)
            input_assign_text (INPUT (cell->widget), patch->value);
        break;
    case MC_RUNTIME_DIALOG_CHECKBOX:
        if (patch->value != NULL)
            CHECK (cell->widget)->state =
                strcmp (patch->value, "true") == 0 || strcmp (patch->value, "1") == 0;
        break;
    default:
        break;
    }
    widget_draw (cell->widget);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
runtime_screen_close (mc_runtime_plugin_context_t *context, guint64 screen_id, const char **error)
{
    runtime_screen_t *scr = screen_lookup (screen_id);

    (void) context;
    if (error != NULL)
        *error = NULL;
    if (scr == NULL || scr->dlg == NULL)
        return screen_error (error, "closed");
    if (!scr->closing)
    {
        scr->closing = TRUE;
        dlg_close (scr->dlg);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
