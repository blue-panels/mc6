/*
   Skin editor plugin - the Color dialog and the 256-color picker.

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
#include "lib/tty/color-internal.h"  // convert_256color_to_truecolor
#include "lib/tty/key.h"
#include "lib/tty/tty.h"
#include "lib/util.h"  // MC_PTR_FREE
#include "lib/widget.h"

#include "skineditor_color.h"
#include "skineditor_grid.h"
#include "skineditor_ui.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define B_RESET   (B_USER + 1)
#define B_FG_256  (B_USER + 2)
#define B_FG_RGB  (B_USER + 3)
#define B_BG_256  (B_USER + 4)
#define B_BG_RGB  (B_USER + 5)

#define DLG_COLS  70
#define DLG_LINES 19

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const char *name; /* as in the skin */
    const char *label;
} attr_def_t;

/* the widgets of one color part: fg or bg */
typedef struct
{
    skinedit_part_t part;
    const char *inherit_label; /* "&Inherit", with the hotkey of this block */
    WColorGrid *grid;
    WCheck *inherit;
    WCheck *terminal;
    WLabel *value;
} part_widgets_t;

typedef struct
{
    skinedit_model_t *model;
    skinedit_entry_t *entry;
    char *orig[SKINEDIT_PARTS]; /* the raw values the dialog opened with */
    skineditor_change_fn on_change;
    void *data;

    WDialog *dlg;
    part_widgets_t fg, bg;
    WCheck *attr_inherit;
    WCheck *attrs[8];
    int nattrs;
    Widget *sample;
    gboolean updating; /* setting widget states, ignore their notifications */
} color_dlg_t;

/* the 256-color picker */
typedef struct
{
    WDialog *dlg;
    WColorGrid *grid;
    WLabel *info;
} pick_dlg_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static const attr_def_t attr_defs[] = {
    { "bold", N_ ("&Bold") },
#ifdef A_ITALIC
    { "italic", N_ ("Ita&lic") },
#endif
    { "underline", N_ ("&Underline") }, { "reverse", N_ ("Re&verse") }, { "blink", N_ ("Blin&k") },
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
notify_change (color_dlg_t *c)
{
    if (c->on_change != NULL)
        c->on_change (c->data);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_part (color_dlg_t *c, skinedit_part_t part, const char *value)
{
    skinedit_model_set (c->model, c->entry, part, value);
    notify_change (c);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
attr_present (const char *attrs, const char *name)
{
    gchar **list;
    int i;
    gboolean ret = FALSE;

    if (attrs == NULL)
        return FALSE;
    list = g_strsplit (attrs, "+", -1);
    for (i = 0; list[i] != NULL && !ret; i++)
        ret = strcmp (list[i], name) == 0;
    g_strfreev (list);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

/* the attrs string from the checkboxes; NULL when none is on */

static char *
attrs_from_checks (const color_dlg_t *c)
{
    GString *s = g_string_new (NULL);
    int i;

    for (i = 0; i < c->nattrs; i++)
        if (c->attrs[i]->state)
        {
            if (s->len > 0)
                g_string_append_c (s, '+');
            g_string_append (s, attr_defs[i].name);
        }
    if (s->len == 0)
    {
        g_string_free (s, TRUE);
        return NULL;
    }
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static char *
inherit_text (const skinedit_entry_t *e, const part_widgets_t *p)
{
    // what the part inherits is what the model resolved while the part was empty
    if (e->src[p->part] == SKINEDIT_SRC_SET)
        return g_strdup (p->inherit_label);
    return g_strdup_printf ("%s (%s)", p->inherit_label,
                            e->effective[p->part] != NULL ? e->effective[p->part] : _ ("terminal"));
}

/* --------------------------------------------------------------------------------------------- */

static void
update_part (color_dlg_t *c, part_widgets_t *p)
{
    const skinedit_entry_t *e = c->entry;
    const char *raw = e->raw[p->part];
    const char *eff = e->effective[p->part];
    char *text;

    p->inherit->state = (raw == NULL);
    p->terminal->state = (raw != NULL && strcmp (raw, "default") == 0);
    colorgrid_set_selected (p->grid, raw != NULL ? raw : eff);
    text = inherit_text (e, p);
    check_set_text (p->inherit, text);
    g_free (text);
    label_set_text (p->value, eff != NULL ? eff : _ ("terminal default"));
    widget_draw (WIDGET (p->inherit));
    widget_draw (WIDGET (p->terminal));
}

/* --------------------------------------------------------------------------------------------- */

static void
update_view (color_dlg_t *c)
{
    const skinedit_entry_t *e = c->entry;
    int i;

    c->updating = TRUE;
    update_part (c, &c->fg);
    update_part (c, &c->bg);
    c->attr_inherit->state = (e->raw[SKINEDIT_PART_ATTRS] == NULL);
    // with no attribute anywhere to inherit, "inherit" and "none" are the same thing
    widget_disable (WIDGET (c->attr_inherit),
                    e->raw[SKINEDIT_PART_ATTRS] == NULL
                        && e->effective[SKINEDIT_PART_ATTRS] == NULL);
    for (i = 0; i < c->nattrs; i++)
        c->attrs[i]->state = attr_present (e->effective[SKINEDIT_PART_ATTRS], attr_defs[i].name);
    c->updating = FALSE;
    widget_draw (WIDGET (c->dlg));
}

/* --------------------------------------------------------------------------------------------- */

static void
part_from_check (color_dlg_t *c, part_widgets_t *p, Widget *sender)
{
    if (sender == WIDGET (p->inherit))
    {
        const char *eff = c->entry->effective[p->part];

        // nothing to inherit means the terminal color: say so explicitly
        set_part (c, p->part, p->inherit->state ? NULL : eff != NULL ? eff : "default");
    }
    else if (sender == WIDGET (p->terminal))
        set_part (c, p->part, p->terminal->state ? "default" : NULL);
    else
        return;
    update_view (c);
}

/* --------------------------------------------------------------------------------------------- */

static void
attrs_from_check (color_dlg_t *c, Widget *sender)
{
    int i;

    if (sender == WIDGET (c->attr_inherit))
    {
        set_part (c, SKINEDIT_PART_ATTRS,
                  c->attr_inherit->state ? NULL : c->entry->effective[SKINEDIT_PART_ATTRS]);
        update_view (c);
        return;
    }
    for (i = 0; i < c->nattrs; i++)
        if (sender == WIDGET (c->attrs[i]))
        {
            char *attrs = attrs_from_checks (c);

            set_part (c, SKINEDIT_PART_ATTRS, attrs);
            g_free (attrs);
            update_view (c);
            return;
        }
}

/* --------------------------------------------------------------------------------------------- */

static void
grid_picked (WColorGrid *g, const char *value, void *data)
{
    color_dlg_t *c = (color_dlg_t *) data;
    part_widgets_t *p = (g == c->fg.grid) ? &c->fg : &c->bg;

    set_part (c, p->part, value);
    update_view (c);
}

/* --------------------------------------------------------------------------------------------- */

static void
pick_256_for (color_dlg_t *c, part_widgets_t *p)
{
    char *value;

    value = skineditor_pick_256 (c->entry->effective[p->part]);
    if (value == NULL)
        return;
    set_part (c, p->part, value);
    g_free (value);
    update_view (c);
}

/* --------------------------------------------------------------------------------------------- */

static void
pick_rgb_for (color_dlg_t *c, part_widgets_t *p)
{
    const char *cur = c->entry->effective[p->part];
    char *value;

    value = input_dialog (_ ("True color"), _ ("Color as #rrggbb or #rgb:"), "skineditor-rgb",
                          cur != NULL && cur[0] == '#' ? cur : "#", INPUT_COMPLETE_NONE);
    if (value == NULL)
        return;
    if (skinedit_color_classify (value) != SKINEDIT_COLOR_TRUECOLOR)
    {
        message (D_ERROR, _ ("True color"), _ ("\"%s\" is not a #rrggbb color"), value);
        g_free (value);
        return;
    }
    set_part (c, p->part, value);
    g_free (value);
    update_view (c);
}

/* --------------------------------------------------------------------------------------------- */

static void
reset_values (color_dlg_t *c)
{
    int i;

    for (i = 0; i < SKINEDIT_PARTS; i++)
        skinedit_model_set (c->model, c->entry, i, c->orig[i]);
    notify_change (c);
}

/* --------------------------------------------------------------------------------------------- */

static int
color_button_cb (WButton *button, int action)
{
    color_dlg_t *c = (color_dlg_t *) DIALOG (WIDGET (button)->owner)->data.p;

    switch (action)
    {
    case B_FG_256:
        pick_256_for (c, &c->fg);
        return 0;
    case B_FG_RGB:
        pick_rgb_for (c, &c->fg);
        return 0;
    case B_BG_256:
        pick_256_for (c, &c->bg);
        return 0;
    case B_BG_RGB:
        pick_rgb_for (c, &c->bg);
        return 0;
    case B_RESET:
        // back to what the entry had on disk
        skinedit_model_reset (c->model, c->entry);
        notify_change (c);
        update_view (c);
        return 0;
    default:
        return 1;
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
sample_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_DRAW:
    {
        color_dlg_t *c = (color_dlg_t *) DIALOG (w->owner)->data.p;
        int y;

        tty_setcolor (skineditor_entry_color (c->entry));
        tty_fill_region (w->rect.y, w->rect.x, w->rect.lines, w->rect.cols, ' ');
        for (y = 0; y < w->rect.lines; y++)
        {
            widget_gotoyx (w, y, 1);
            tty_print_string (
                str_fit_to_term ("Sample text  Sample text  Sample text  Sample text  Sample text",
                                 w->rect.cols - 2, J_LEFT));
        }
        return MSG_HANDLED;
    }

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
color_dlg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    color_dlg_t *c = (color_dlg_t *) DIALOG (w)->data.p;

    switch (msg)
    {
    case MSG_NOTIFY:
        if (c->updating || sender == NULL)
            return MSG_HANDLED;
        part_from_check (c, &c->fg, sender);
        part_from_check (c, &c->bg, sender);
        attrs_from_check (c, sender);
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* the widgets of one part in a block starting at (@y, @x) */

static void
add_part (color_dlg_t *c, part_widgets_t *p, skinedit_part_t part, int y, int x, const char *title,
          const char *inherit_label, const char *terminal_label, int id_256, int id_rgb)
{
    WGroup *g = GROUP (c->dlg);
    int grid_cols, grid_lines;

    WButton *b256, *brgb;

    p->part = part;
    p->inherit_label = inherit_label;
    colorgrid_size (COLORGRID_16, &grid_lines, &grid_cols);

    group_add_widget (g, label_new (y, x, title));
    p->inherit = check_new (y, x + 14, FALSE, inherit_label);
    group_add_widget (g, p->inherit);

    p->grid = colorgrid_new (y + 1, x + 1, COLORGRID_16);
    p->grid->on_pick = grid_picked;
    p->grid->data = c;
    group_add_widget (g, p->grid);

    p->value = label_new (y + 1, x + grid_cols + 4, "");
    group_add_widget (g, p->value);
    b256 = button_new (y + 2, x + grid_cols + 4, id_256, NORMAL_BUTTON, "256..", color_button_cb);
    group_add_widget (g, b256);
    brgb = button_new (y + 2, x + grid_cols + 4 + button_get_width (b256) + 1, id_rgb,
                       NORMAL_BUTTON, "RGB..", color_button_cb);
    group_add_widget (g, brgb);
    if (!tty_use_256colors (NULL))
        widget_disable (WIDGET (b256), TRUE);
    if (!tty_use_truecolors (NULL))
        widget_disable (WIDGET (brgb), TRUE);

    p->terminal = check_new (y + 3, x + 1, FALSE, terminal_label);
    group_add_widget (g, p->terminal);
}

/* --------------------------------------------------------------------------------------------- */

static void
pick_update_info (WColorGrid *g, const char *value, void *data)
{
    pick_dlg_t *p = (pick_dlg_t *) data;
    int idx = tty_color_get_index_by_name (value);
    int rgb = convert_256color_to_truecolor (idx) & 0xFFFFFF;
    char *text;

    (void) g;
    if (idx >= 16 && idx < 232)
    {
        int n = idx - 16;

        text = g_strdup_printf ("%s = rgb%d%d%d = #%06x", value, n / 36, n / 6 % 6, n % 6,
                                (unsigned int) rgb);
    }
    else if (idx >= 232)
        text = g_strdup_printf ("%s = gray%d = #%06x", value, idx - 232, (unsigned int) rgb);
    else
        text = g_strdup_printf ("%s = %s", value, tty_color_get_name_by_index (idx));
    label_set_text (p->info, text);
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

static void
pick_chosen (WColorGrid *g, const char *value, void *data)
{
    pick_dlg_t *p = (pick_dlg_t *) data;

    (void) g;
    (void) value;
    p->dlg->ret_value = B_ENTER;
    dlg_close (p->dlg);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
skineditor_pick_256 (const char *current)
{
    pick_dlg_t p;
    WGroup *g;
    int grid_lines, grid_cols, cols, lines;
    char *ret = NULL;
    WButton *ok, *cancel;

    colorgrid_size (COLORGRID_256, &grid_lines, &grid_cols);
    // 77 columns of cells: the frame has no margin so that the picker fits in 80
    cols = grid_cols + 2;
    lines = grid_lines + 6;

    p.dlg = dlg_create (TRUE, 0, 0, lines, cols, WPOS_CENTER, TRUE, dialog_colors, NULL, NULL, NULL,
                        _ ("256 colors"));
    g = GROUP (p.dlg);
    p.grid = colorgrid_new (1, 1, COLORGRID_256);
    p.grid->on_move = pick_update_info;
    p.grid->on_pick = pick_chosen;
    p.grid->data = &p;
    group_add_widget (g, p.grid);
    p.info = label_new (grid_lines + 2, 1, "");
    group_add_widget (g, p.info);
    ok = button_new (grid_lines + 3, 1, B_ENTER, DEFPUSH_BUTTON, _ ("&OK"), NULL);
    cancel = button_new (grid_lines + 3, 1 + button_get_width (ok) + 2, B_CANCEL, NORMAL_BUTTON,
                         _ ("&Cancel"), NULL);
    group_add_widget (g, ok);
    group_add_widget (g, cancel);

    if (current != NULL && skinedit_color_classify (current) == SKINEDIT_COLOR_256)
    {
        // the grid names its cells colorN; map grayN and rgbRGB to that
        int idx = tty_color_get_index_by_name (current);
        char *name = g_strdup_printf ("color%d", idx);

        colorgrid_set_selected (p.grid, name);
        g_free (name);
    }
    pick_update_info (p.grid, colorgrid_current (p.grid), &p);
    widget_select (WIDGET (p.grid));

    if (dlg_run (p.dlg) == B_ENTER)
        ret = g_strdup (colorgrid_current (p.grid));
    widget_destroy (WIDGET (p.dlg));
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
skineditor_color_dialog (skinedit_model_t *m, skinedit_entry_t *e, skineditor_change_fn on_change,
                         void *data)
{
    color_dlg_t c;
    WGroup *g;
    int i, x_style, y;
    int rc;
    char *title;
    WButton *ok, *reset, *cancel;
    WRect r;

    memset (&c, 0, sizeof (c));
    c.model = m;
    c.entry = e;
    c.on_change = on_change;
    c.data = data;
    for (i = 0; i < SKINEDIT_PARTS; i++)
        c.orig[i] = g_strdup (e->raw[i]);

    title = g_strdup_printf ("%s / %s", e->group, e->label);
    // over the left half of the screen: the sample pane on the right stays visible
    c.dlg = dlg_create (TRUE, (LINES - DLG_LINES) / 2, 1, DLG_LINES, DLG_COLS, WPOS_KEEP_DEFAULT,
                        FALSE, dialog_colors, color_dlg_callback, NULL, NULL, title);
    g_free (title);
    c.dlg->data.p = &c;
    g = GROUP (c.dlg);

    // every hotkey in the dialog is distinct: i t / e d / h, b l u v k, o r c
    add_part (&c, &c.fg, SKINEDIT_PART_FG, 2, 2, _ ("Foreground"), _ ("&Inherit"),
              _ ("&Terminal default"), B_FG_256, B_FG_RGB);
    add_part (&c, &c.bg, SKINEDIT_PART_BG, 7, 2, _ ("Background"), _ ("Inh&erit"),
              _ ("Terminal &default"), B_BG_256, B_BG_RGB);

    x_style = DLG_COLS - 18;
    group_add_widget (g, label_new (2, x_style, _ ("Style")));
    c.attr_inherit = check_new (3, x_style, FALSE, _ ("In&herit"));
    group_add_widget (g, c.attr_inherit);
    c.nattrs = G_N_ELEMENTS (attr_defs);
    for (i = 0; i < c.nattrs; i++)
    {
        c.attrs[i] = check_new (4 + i, x_style, FALSE, _ (attr_defs[i].label));
        group_add_widget (g, c.attrs[i]);
    }

    y = 12;
    group_add_widget (g, groupbox_new (y, 2, 4, DLG_COLS - 4, _ ("Sample")));
    c.sample = g_new0 (Widget, 1);
    r.y = y + 1;
    r.x = 3;
    r.lines = 2;
    r.cols = DLG_COLS - 6;
    widget_init (c.sample, &r, sample_callback, NULL);
    group_add_widget (g, c.sample);

    y = DLG_LINES - 3;
    ok = button_new (y, 0, B_ENTER, DEFPUSH_BUTTON, _ ("&OK"), NULL);
    reset = button_new (y, 0, B_RESET, NORMAL_BUTTON, _ ("&Reset"), color_button_cb);
    cancel = button_new (y, 0, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), NULL);
    {
        int total =
            button_get_width (ok) + button_get_width (reset) + button_get_width (cancel) + 4;
        int x = (DLG_COLS - total) / 2;

        widget_set_size (WIDGET (ok), y, x, 1, button_get_width (ok));
        x += button_get_width (ok) + 2;
        widget_set_size (WIDGET (reset), y, x, 1, button_get_width (reset));
        x += button_get_width (reset) + 2;
        widget_set_size (WIDGET (cancel), y, x, 1, button_get_width (cancel));
    }
    group_add_widget (g, ok);
    group_add_widget (g, reset);
    group_add_widget (g, cancel);

    update_view (&c);
    widget_select (WIDGET (c.fg.grid));

    rc = dlg_run (c.dlg);
    widget_destroy (WIDGET (c.dlg));

    if (rc != B_ENTER)
        reset_values (&c);
    for (i = 0; i < SKINEDIT_PARTS; i++)
        g_free (c.orig[i]);
    return rc == B_ENTER;
}

/* --------------------------------------------------------------------------------------------- */
