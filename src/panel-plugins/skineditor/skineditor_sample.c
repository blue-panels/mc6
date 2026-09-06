/*
   Skin editor plugin - the sample pane.

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

#include <string.h>

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/tty/color.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "skineditor_sample.h"
#include "skineditor_sample_priv.h"
#include "skineditor_ui.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define SPIN_PERIOD_US 150000

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
region_add (WSkinSample *s, int y, int x, int lines, int cols, skinedit_entry_t *e)
{
    skinsample_region_t r;

    if (e == NULL || lines <= 0 || cols <= 0)
        return;
    r.rect.y = y;
    r.rect.x = x;
    r.rect.lines = lines;
    r.rect.cols = cols;
    r.entry = e;
    g_array_append_val (s->regions, r);
}

/* --------------------------------------------------------------------------------------------- */

static skinedit_entry_t *
region_at (const WSkinSample *s, int y, int x)
{
    int i;

    for (i = (int) s->regions->len - 1; i >= 0; i--)
    {
        const skinsample_region_t *r = &g_array_index (s->regions, skinsample_region_t, i);

        if (y >= r->rect.y && y < r->rect.y + r->rect.lines && x >= r->rect.x
            && x < r->rect.x + r->rect.cols)
            return r->entry;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* the pair of @e; in isolate mode dimmed unless @e or @owner, the key of the area, is current */

static int
entry_color (WSkinSample *s, const skinedit_entry_t *e, const skinedit_entry_t *owner)
{
    const int *colors = widget_get_colors (WIDGET (s));

    if (s->isolate && s->current != NULL && e != s->current && owner != s->current)
    {
        tty_color_pair_t pair;

        pair.fg = (char *) "gray";
        pair.bg = (char *) tty_color_pair_background (colors[DLG_COLOR_NORMAL]);
        pair.attrs = NULL;
        pair.pair_index = 0;
        return tty_try_alloc_color_pair (&pair, TRUE);
    }
    if (e == NULL || e->kind != SKINEDIT_ENTRY_COLOR)
        return colors[DLG_COLOR_NORMAL];
    return skineditor_entry_color (e);
}

/* --------------------------------------------------------------------------------------------- */

/* Until a section has a mock-up of its own: every entry as a line of text in its colors. */

static void
draw_generic (WSkinSample *s, const WRect *r)
{
    int y;
    guint ei;
    const skinedit_entry_t *first;

    (void) r;
    first = s->section->entries->len > 0 ? g_ptr_array_index (s->section->entries, 0) : NULL;
    if (first != NULL)
        sample_fill (s, 0, 0, r->lines, r->cols, first->group, "_default_");

    for (ei = 0, y = 0; ei < s->section->entries->len && y < r->lines; ei++, y++)
    {
        skinedit_entry_t *e = g_ptr_array_index (s->section->entries, ei);

        if (e->kind == SKINEDIT_ENTRY_COLOR)
            sample_text (s, y, 2, e->group, e->key, e->label);
        else
        {
            char *text;

            text = g_strdup_printf ("%s  %s", sample_char (s, e->group, e->key), e->label);
            sample_text (s, y, 2, e->group, e->key, text);
            g_free (text);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
sample_draw (WSkinSample *s)
{
    Widget *w = WIDGET (s);
    const int *colors = widget_get_colors (w);
    WRect r = { 0, 0, w->rect.lines, w->rect.cols };
    skinsample_draw_fn fn = NULL;

    g_array_set_size (s->regions, 0);
    tty_setcolor (colors[DLG_COLOR_NORMAL]);
    tty_fill_region (w->rect.y, w->rect.x, w->rect.lines, w->rect.cols, ' ');

    if (s->section == NULL || s->section->entries->len == 0)
        return;

    {
        const skinedit_entry_t *first = g_ptr_array_index (s->section->entries, 0);

        fn = skinsample_lookup (first->group, first->key);
    }
    if (fn != NULL)
        fn (s, &r);
    else
        draw_generic (s, &r);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
sample_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WSkinSample *s = SKINSAMPLE (w);

    switch (msg)
    {
    case MSG_DRAW:
        sample_draw (s);
        return MSG_HANDLED;

    case MSG_DESTROY:
        g_array_free (s->regions, TRUE);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
sample_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WSkinSample *s = SKINSAMPLE (w);
    skinedit_entry_t *e;

    switch (msg)
    {
    case MSG_MOUSE_DOWN:
        e = region_at (s, event->y, event->x);
        if (e != NULL && s->on_pick != NULL)
            s->on_pick (s, e, s->data);
        break;

    case MSG_MOUSE_CLICK:
        e = region_at (s, event->y, event->x);
        if (e != NULL && event->count == GPM_DOUBLE && s->on_edit != NULL)
            s->on_edit (s, e, s->data);
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

skinedit_entry_t *
sample_entry (WSkinSample *s, const char *group, const char *key)
{
    return skinedit_model_find (s->model, group, key);
}

/* --------------------------------------------------------------------------------------------- */

int
sample_color (WSkinSample *s, const char *group, const char *key)
{
    return entry_color (s, sample_entry (s, group, key), NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
sample_text (WSkinSample *s, int y, int x, const char *group, const char *key, const char *text)
{
    Widget *w = WIDGET (s);
    skinedit_entry_t *e = sample_entry (s, group, key);
    int width;

    if (y < 0 || y >= w->rect.lines || x < 0 || x >= w->rect.cols)
        return;
    width = str_term_width1 (text);
    if (x + width > w->rect.cols)
        width = w->rect.cols - x;

    tty_setcolor (entry_color (s, e, NULL));
    widget_gotoyx (w, y, x);
    tty_print_string (str_fit_to_term (text, width, J_LEFT));
    region_add (s, y, x, 1, width, e);
}

/* --------------------------------------------------------------------------------------------- */

void
sample_hot (WSkinSample *s, int y, int x, const char *group, const char *key, const char *hot_key,
            const char *text)
{
    const char *amp = strchr (text, '&');
    char *before;
    int w1;

    if (amp == NULL)
    {
        sample_text (s, y, x, group, key, text);
        return;
    }

    before = g_strndup (text, amp - text);
    w1 = str_term_width1 (before);
    if (*before != '\0')
        sample_text (s, y, x, group, key, before);
    g_free (before);

    {
        char hot[8];
        const char *next = g_utf8_next_char (amp + 1);

        g_strlcpy (hot, amp + 1, MIN ((size_t) (next - (amp + 1)) + 1, sizeof (hot)));
        sample_text (s, y, x + w1, group, hot_key, hot);
        if (*next != '\0')
            sample_text (s, y, x + w1 + 1, group, key, next);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
sample_piece (WSkinSample *s, int y, int x, const char *group, const char *key,
              const char *char_group, const char *char_key, mc_tty_char_t ch)
{
    Widget *w = WIDGET (s);

    skinedit_entry_t *owner = sample_entry (s, char_group, char_key);

    if (y < 0 || y >= w->rect.lines || x < 0 || x >= w->rect.cols)
        return;
    tty_setcolor (entry_color (s, sample_entry (s, group, key), owner));
    widget_gotoyx (w, y, x);
    tty_print_char (ch);
    region_add (s, y, x, 1, 1, owner);
}

/* --------------------------------------------------------------------------------------------- */

void
sample_fill (WSkinSample *s, int y, int x, int lines, int cols, const char *group, const char *key)
{
    Widget *w = WIDGET (s);

    if (y < 0)
    {
        lines += y;
        y = 0;
    }
    if (x < 0)
    {
        cols += x;
        x = 0;
    }
    if (x + cols > w->rect.cols)
        cols = w->rect.cols - x;
    if (y + lines > w->rect.lines)
        lines = w->rect.lines - y;
    if (lines <= 0 || cols <= 0)
        return;
    tty_setcolor (sample_color (s, group, key));
    tty_fill_region (w->rect.y + y, w->rect.x + x, lines, cols, ' ');
    region_add (s, y, x, lines, cols, sample_entry (s, group, key));
}

/* --------------------------------------------------------------------------------------------- */

void
sample_hline (WSkinSample *s, int y, int x, int cols, const char *group, const char *key)
{
    Widget *w = WIDGET (s);

    if (y < 0 || y >= w->rect.lines || x < 0)
        return;
    if (x + cols > w->rect.cols)
        cols = w->rect.cols - x;
    if (cols <= 0)
        return;
    tty_setcolor (sample_color (s, group, key));
    tty_draw_hline (w->rect.y + y, w->rect.x + x, mc_tty_frm[MC_TTY_FRM_HORIZ], cols);
    region_add (s, y, x, 1, cols, sample_entry (s, group, key));
}

/* --------------------------------------------------------------------------------------------- */

void
sample_box (WSkinSample *s, int y, int x, int lines, int cols, const char *group, const char *key,
            gboolean single)
{
    Widget *w = WIDGET (s);

    // a box cannot be drawn in part: skip it when the pane is too small
    if (y < 0 || x < 0)
        return;
    if (x + cols > w->rect.cols)
        cols = w->rect.cols - x;
    if (y + lines > w->rect.lines)
        lines = w->rect.lines - y;
    if (lines < 2 || cols < 2)
        return;
    tty_setcolor (sample_color (s, group, key));
    tty_draw_box (w->rect.y + y, w->rect.x + x, lines, cols, single);
    region_add (s, y, x, 1, cols, sample_entry (s, group, key));
    region_add (s, y + lines - 1, x, 1, cols, sample_entry (s, group, key));
    region_add (s, y, x, lines, 1, sample_entry (s, group, key));
    region_add (s, y, x + cols - 1, lines, 1, sample_entry (s, group, key));
}

/* --------------------------------------------------------------------------------------------- */

const char *
sample_char (WSkinSample *s, const char *group, const char *key)
{
    const skinedit_entry_t *e = sample_entry (s, group, key);

    if (e == NULL)
        return "?";
    return e->shown != NULL ? e->shown : "";
}

/* --------------------------------------------------------------------------------------------- */

WSkinSample *
skinsample_new (int y, int x, int lines, int cols, skinedit_model_t *model)
{
    WSkinSample *s;
    WRect r = { y, x, lines, cols };

    s = g_new0 (WSkinSample, 1);
    widget_init (WIDGET (s), &r, sample_callback, sample_mouse_callback);
    s->model = model;
    s->regions = g_array_new (FALSE, FALSE, sizeof (skinsample_region_t));
    s->tick_at = g_get_monotonic_time ();
    return s;
}

/* --------------------------------------------------------------------------------------------- */

void
skinsample_set (WSkinSample *s, skinedit_section_t *section, skinedit_entry_t *current)
{
    s->section = section;
    s->current = current;
    widget_draw (WIDGET (s));
}

/* --------------------------------------------------------------------------------------------- */

void
skinsample_tick (WSkinSample *s)
{
    gint64 now = g_get_monotonic_time ();
    const skinedit_entry_t *first;

    if (s->section == NULL || s->section->entries->len == 0 || now - s->tick_at < SPIN_PERIOD_US)
        return;
    first = g_ptr_array_index (s->section->entries, 0);
    if (first->kind != SKINEDIT_ENTRY_STRING)
        return;
    s->tick++;
    s->tick_at = now;
    widget_draw (WIDGET (s));
}

/* --------------------------------------------------------------------------------------------- */
