/*
   Skin editor plugin - the main dialog.

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
#include "lib/event.h"
#include "lib/mcconfig.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/tty/color.h"
#include "lib/tty/key.h"
#include "lib/tty/tty.h"
#include "lib/util.h"  // MC_PTR_FREE
#include "lib/widget.h"

#include "src/editor/edit.h"        // edit_file
#include "src/filemanager/boxes.h"  // skin_apply_current

#include "skineditor_color.h"
#include "skineditor_keylist.h"
#include "skineditor_sample.h"
#include "skineditor_ui.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define B_EDIT        (B_USER + 1)
#define B_SAVE        (B_USER + 2)
#define B_SAVE_AS     (B_USER + 3)
#define B_SKIN        (B_USER + 4)
#define B_EDIT_FILE   (B_USER + 5)
#define B_CLASS       (B_USER + 6)

#define MIN_LIST_COLS 30
#define MIN_COLS      66
#define MIN_LINES     16

/*** file scope type declarations ****************************************************************/

typedef struct
{
    skinedit_model_t *model;
    char *running_name;         /* the skin mc had when the editor opened */
    gboolean can_preview;       /* the terminal can show the colors of the skin */
    gboolean previewed;         /* the running skin has been replaced by a preview */
    char *clip[SKINEDIT_PARTS]; /* Ctrl-Ins: the parts of a key, NULL = inherited */
    gboolean has_clip;
    GArray *undo; /* undo_step_t: what a key held before each change, newest last */

    WDialog *dlg;
    WLabel *skin_label;
    WButton *skin_button;
    WButton *class_button; /* 16 colors, 256 colors or true color: opens a list of the three */
    WSkinKeyList *list;
    WSkinSample *sample;
    WHLine *info_line;
    WLabel *value_label;
    WLabel *desc_label;
    WHLine *button_line;
    WButton *edit_button;
    WButton *save_button;
    WButton *save_as_button;
    WButton *edit_file_button;
    WButton *cancel_button;
} skineditor_ui_t;

typedef struct
{
    skinedit_entry_t *entry;
    char *before[SKINEDIT_PARTS];
} undo_step_t;

/*** forward declarations (file scope functions) *************************************************/

static void undo_push (skineditor_ui_t *ui, skinedit_entry_t *e);
static skinedit_entry_t *undo_pop (skineditor_ui_t *ui, char *before[SKINEDIT_PARTS]);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static skineditor_ui_t *
ui_of (const Widget *w)
{
    return (skineditor_ui_t *) DIALOG (w->owner)->data.p;
}

/* the three classes a skin can be declared for, in the order of the list */
static const skinedit_color_class_t class_order[] = { SKINEDIT_COLOR_BASIC, SKINEDIT_COLOR_256,
                                                      SKINEDIT_COLOR_TRUECOLOR };

static const char *
color_class_name (skinedit_color_class_t cls)
{
    switch (cls)
    {
    case SKINEDIT_COLOR_TRUECOLOR:
        return _ ("True color");
    case SKINEDIT_COLOR_256:
        return _ ("256 colors");
    default:
        return _ ("16 colors");
    }
}

/* --------------------------------------------------------------------------------------------- */

/* the widest of the class names, as a button */

static int
class_button_cols (void)
{
    int w = 0;
    guint i;

    for (i = 0; i < G_N_ELEMENTS (class_order); i++)
        w = MAX (w, str_term_width1 (color_class_name (class_order[i])));
    return w + 4;
}

/* --------------------------------------------------------------------------------------------- */

/* the button row, with its gaps; translated labels make it wider */

static int
ui_buttons_width (const skineditor_ui_t *ui)
{
    return button_get_width (ui->edit_button) + button_get_width (ui->save_button)
        + button_get_width (ui->save_as_button) + button_get_width (ui->edit_file_button)
        + button_get_width (ui->cancel_button) + 8;
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_layout (skineditor_ui_t *ui)
{
    const WRect *r = &WIDGET (ui->dlg)->rect;
    int top = 2, left = 2;
    int width = r->cols - 4;
    int list_cols, sample_x, sample_cols;
    int list_lines = r->lines - 9 - top;
    int y, x, bw;

    list_cols = width * 2 / 5;
    if (list_cols < MIN_LIST_COLS)
        list_cols = MIN_LIST_COLS;
    if (list_cols > width - 10)
        list_cols = width - 10;
    if (list_cols < 8)
        list_cols = 8;
    sample_x = left + list_cols + 1;
    sample_cols = MAX (r->cols - 2 - sample_x, 1);
    if (list_lines < 1)
        list_lines = 1;
    if (width < 1)
        width = 1;

    widget_set_size (WIDGET (ui->skin_label), top, left, 1, 5);
    widget_set_size (WIDGET (ui->skin_button), top, left + 6, 1, list_cols - 6);
    widget_set_size (WIDGET (ui->list), top + 1, left, list_lines, list_cols);
    widget_set_size (WIDGET (ui->class_button), top, sample_x, 1, class_button_cols ());
    widget_set_size (WIDGET (ui->sample), top + 1, sample_x, list_lines, sample_cols);

    y = top + list_lines + 1;
    widget_set_size (WIDGET (ui->info_line), y + 1, 0, 1, r->cols);
    widget_set_size (WIDGET (ui->value_label), y + 2, left, 1, width);
    widget_set_size (WIDGET (ui->desc_label), y + 3, left, 1, width);
    widget_set_size (WIDGET (ui->button_line), y + 4, 0, 1, r->cols);

    y += 5;
    bw = ui_buttons_width (ui);
    x = MAX ((r->cols - bw) / 2, 1);
    widget_set_size (WIDGET (ui->edit_button), y, x, 1, button_get_width (ui->edit_button));
    x += button_get_width (ui->edit_button) + 2;
    widget_set_size (WIDGET (ui->save_button), y, x, 1, button_get_width (ui->save_button));
    x += button_get_width (ui->save_button) + 2;
    widget_set_size (WIDGET (ui->save_as_button), y, x, 1, button_get_width (ui->save_as_button));
    x += button_get_width (ui->save_as_button) + 2;
    widget_set_size (WIDGET (ui->edit_file_button), y, x, 1,
                     button_get_width (ui->edit_file_button));
    x += button_get_width (ui->edit_file_button) + 2;
    widget_set_size (WIDGET (ui->cancel_button), y, x, 1, button_get_width (ui->cancel_button));
}

/* --------------------------------------------------------------------------------------------- */

static const char *
src_name (const skinedit_entry_t *e, int i)
{
    switch (e->src[i])
    {
    case SKINEDIT_SRC_GROUP_DEFAULT:
        return e->group;
    case SKINEDIT_SRC_CORE_DEFAULT:
        return "core";
    case SKINEDIT_SRC_FALLBACK:
        return "";
    default:
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
format_value (const skinedit_entry_t *e)
{
    GString *s = g_string_new (NULL);

    if (e->kind == SKINEDIT_ENTRY_COLOR)
    {
        static const char *const part_names[SKINEDIT_PARTS] = { "fg", "bg", "attrs" };
        const char *fg = e->effective[SKINEDIT_PART_FG];
        const char *bg = e->effective[SKINEDIT_PART_BG];
        int i;
        gboolean first = TRUE;

        g_string_append_printf (s, "%s: %s on %s", e->label, fg != NULL ? fg : _ ("terminal"),
                                bg != NULL ? bg : _ ("terminal"));
        if (e->effective[SKINEDIT_PART_ATTRS] != NULL)
            g_string_append_printf (s, ", %s", e->effective[SKINEDIT_PART_ATTRS]);

        for (i = 0; i < SKINEDIT_PARTS; i++)
        {
            const char *from = src_name (e, i);

            if (from == NULL)
                continue;
            g_string_append (s, first ? "   (" : ", ");
            if (*from == '\0')
                g_string_append_printf (s, _ ("%s: what mc fills in"), part_names[i]);
            else
                g_string_append_printf (s, "%s from [%s] _default_", part_names[i], from);
            first = FALSE;
        }
        if (!first)
            g_string_append_c (s, ')');
    }
    else
    {
        const char *v = e->raw[0] != NULL ? e->raw[0] : e->builtin;

        g_string_append_printf (s, "%s: %s", e->label, e->shown != NULL ? e->shown : "");
        if (e->kind == SKINEDIT_ENTRY_CHAR && v != NULL && *v != '\0')
            g_string_append_printf (s, "  U+%04X", (unsigned int) g_utf8_get_char (v));
        if (e->raw[0] == NULL)
            g_string_append_printf (s, "  (%s)", _ ("built-in"));
    }
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* label_set_text lets a label shrink to its text and never grow again: put the width back */

static void
set_info_line (WLabel *label, const char *text)
{
    WRect r = WIDGET (label)->rect;

    label_set_text (label, str_fit_to_term (text, r.cols, J_LEFT_FIT));
    widget_set_size_rect (WIDGET (label), &r);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_update_info (skineditor_ui_t *ui)
{
    skinedit_entry_t *e = skinkeylist_current (ui->list);
    skinedit_section_t *s = skinkeylist_current_section (ui->list);
    char *text;

    text = e != NULL ? format_value (e) : g_strdup ("");
    set_info_line (ui->value_label, text);
    g_free (text);
    set_info_line (ui->desc_label, e != NULL && e->description != NULL ? e->description : "");

    {
        WRect br = WIDGET (ui->skin_button)->rect;

        text = g_strdup (str_fit_to_term (ui->model->name, br.cols - 4, J_LEFT_FIT));
        // button_set_text shrinks the button to the text; keep the column
        button_set_text (ui->skin_button, text);
        widget_set_size_rect (WIDGET (ui->skin_button), &br);
        g_free (text);
    }
    {
        WRect br = WIDGET (ui->class_button)->rect;

        button_set_text (ui->class_button, color_class_name (ui->model->colors));
        widget_set_size_rect (WIDGET (ui->class_button), &br);
    }

    // the idle timer only runs the spinner
    widget_idle (WIDGET (ui->dlg),
                 s != NULL && s->entries->len > 0
                     && ((skinedit_entry_t *) g_ptr_array_index (s->entries, 0))->kind
                         == SKINEDIT_ENTRY_STRING);

    skinsample_set (ui->sample, s, e);
}

/* --------------------------------------------------------------------------------------------- */

/* TRUE when the terminal can show what the skin uses */

static gboolean
ui_terminal_ok (const skineditor_ui_t *ui)
{
    gboolean needs_256, needs_true;

    skinedit_model_needs (ui->model, &needs_256, &needs_true);
    return !(needs_true && !tty_use_truecolors (NULL)) && !(needs_256 && !tty_use_256colors (NULL));
}

/* --------------------------------------------------------------------------------------------- */

/* ui_terminal_ok, saying why once when it is not */

static gboolean
ui_check_terminal (skineditor_ui_t *ui)
{
    if (ui_terminal_ok (ui))
        return TRUE;
    message (D_NORMAL, _ ("Skin editor"),
             _ ("This terminal cannot show the colors of \"%s\".\n"
                "The skin can be edited and saved, but the screen will not follow."),
             ui->model->name);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_preview (skineditor_ui_t *ui)
{
    mc_config_t *cfg;
    GError *error = NULL;

    // a skin the terminal could not show may have been edited down to what it can
    if (!ui->can_preview)
    {
        if (!ui_terminal_ok (ui))
            return;
        ui->can_preview = TRUE;
    }
    cfg = skinedit_model_config_copy (ui->model);
    if (cfg == NULL)
        return;
    ui->previewed = TRUE;
    if (!mc_skin_load_from_config (cfg, ui->model->name, &error))
        ui->can_preview = FALSE;
    skin_apply_current ();
    mc_error_message (&error, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_reload_running (skineditor_ui_t *ui)
{
    GError *error = NULL;

    mc_skin_deinit ();
    mc_skin_init (ui->running_name, &error);
    skin_apply_current ();
    mc_error_message (&error, NULL);
    ui->previewed = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_changed (skineditor_ui_t *ui)
{
    ui_preview (ui);
    ui_update_info (ui);
    widget_draw (WIDGET (ui->dlg));
}

/* --------------------------------------------------------------------------------------------- */

/* "Skin: name  ~/.local/share/mc6/skins/name.ini": the file, so that the user can find it */

static void
ui_set_title (skineditor_ui_t *ui)
{
    const char *home = g_get_home_dir ();
    const char *path = ui->model->path != NULL ? ui->model->path : "";
    char *shown, *title;

    if (home != NULL && g_str_has_prefix (path, home) && path[strlen (home)] == PATH_SEP)
        shown = g_strdup_printf ("~%s", path + strlen (home));
    else
        shown = g_strdup (path);
    title = g_strdup_printf (_ ("Skin: %s  %s"), ui->model->name, shown);
    frame_set_title (FRAME (ui->dlg->bg),
                     str_fit_to_term (title, WIDGET (ui->dlg)->rect.cols - 8, J_LEFT_FIT));
    g_free (title);
    g_free (shown);
}

/* --------------------------------------------------------------------------------------------- */

/* every change in the Color dialog: the screen behind follows */

static void
ui_live_change (void *data)
{
    skineditor_ui_t *ui = (skineditor_ui_t *) data;

    ui_update_info (ui);
    ui_preview (ui);
}

/* --------------------------------------------------------------------------------------------- */

/* the three parts as text: for a screen the Color dialog does not fit on */

static void
edit_color_text (skineditor_ui_t *ui, skinedit_entry_t *e)
{
    char *fg = NULL, *bg = NULL, *attrs = NULL;
    int rc;

    {
        quick_widget_t widgets[] = {
            // clang-format off
            QUICK_LABELED_INPUT (_ ("Foreground:"), input_label_left,
                                 e->raw[SKINEDIT_PART_FG] != NULL ? e->raw[SKINEDIT_PART_FG] : "",
                                 "skineditor-fg", &fg, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (_ ("Background:"), input_label_left,
                                 e->raw[SKINEDIT_PART_BG] != NULL ? e->raw[SKINEDIT_PART_BG] : "",
                                 "skineditor-bg", &bg, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (_ ("Attributes:"), input_label_left,
                                 e->raw[SKINEDIT_PART_ATTRS] != NULL ? e->raw[SKINEDIT_PART_ATTRS] : "",
                                 "skineditor-attrs", &attrs, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABEL (_ ("Empty = inherited"), NULL),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
            // clang-format on
        };

        WRect r = { -1, -1, 0, MIN (50, COLS - 2) };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = e->label,
            .help = NULL,
            .widgets = widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        rc = quick_dialog (&qdlg);
    }

    if (rc == B_ENTER)
    {
        undo_push (ui, e);
        skinedit_model_set (ui->model, e, SKINEDIT_PART_FG, fg);
        skinedit_model_set (ui->model, e, SKINEDIT_PART_BG, bg);
        skinedit_model_set (ui->model, e, SKINEDIT_PART_ATTRS, attrs);
        ui_changed (ui);
    }
    g_free (fg);
    g_free (bg);
    g_free (attrs);
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_color_entry (skineditor_ui_t *ui, skinedit_entry_t *e)
{
    if (!skineditor_color_dialog_fits ())
    {
        edit_color_text (ui, e);
        return;
    }
    undo_push (ui, e);
    // the dialog previews every change itself, Cancel included
    if (!skineditor_color_dialog (ui->model, e, ui_live_change, ui))
        undo_pop (ui, NULL);
    ui_update_info (ui);
    widget_draw (WIDGET (ui->dlg));
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_text_entry (skineditor_ui_t *ui, skinedit_entry_t *e)
{
    char *value, *utf8;
    const char *current = e->shown;

    value = input_dialog (e->label,
                          e->kind == SKINEDIT_ENTRY_CHAR ? _ ("One character (empty = built-in):")
                                                         : _ ("Value (empty = built-in):"),
                          "skineditor-text", current != NULL ? current : "", INPUT_COMPLETE_NONE);
    if (value == NULL)
        return;
    // the check is made on what goes into the file: UTF-8, one character for a CHAR key
    utf8 = skinedit_text_to_skin (value);
    if (*utf8 != '\0'
        && (!g_utf8_validate (utf8, -1, NULL)
            || (e->kind == SKINEDIT_ENTRY_CHAR && g_utf8_strlen (utf8, -1) != 1)))
    {
        message (D_ERROR, e->label, "%s", _ ("One character is expected here"));
        g_free (utf8);
        g_free (value);
        return;
    }
    g_free (utf8);
    undo_push (ui, e);
    skinedit_model_set_text (ui->model, e, value);
    g_free (value);
    ui_changed (ui);
}

/* --------------------------------------------------------------------------------------------- */

/* remember what @e holds now; call before a change made through the dialog */

static void
undo_push (skineditor_ui_t *ui, skinedit_entry_t *e)
{
    undo_step_t step;
    int i;

    step.entry = e;
    for (i = 0; i < SKINEDIT_PARTS; i++)
        step.before[i] = g_strdup (e->raw[i]);
    g_array_append_val (ui->undo, step);
}

/* --------------------------------------------------------------------------------------------- */

static void
undo_clear (skineditor_ui_t *ui)
{
    while (undo_pop (ui, NULL) != NULL)
        ;
}

/* --------------------------------------------------------------------------------------------- */

/* the newest step off the stack; its entry, the before values freed */

static skinedit_entry_t *
undo_pop (skineditor_ui_t *ui, char *before[SKINEDIT_PARTS])
{
    undo_step_t *step;
    skinedit_entry_t *e;
    int i;

    if (ui->undo->len == 0)
        return NULL;
    step = &g_array_index (ui->undo, undo_step_t, ui->undo->len - 1);
    e = step->entry;
    for (i = 0; i < SKINEDIT_PARTS; i++)
        if (before != NULL)
            before[i] = step->before[i];
        else
            g_free (step->before[i]);
    g_array_set_size (ui->undo, ui->undo->len - 1);
    return e;
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-U: the last change taken back, the cursor on its key */

static void
ui_undo (skineditor_ui_t *ui)
{
    skinedit_entry_t *e;
    char *before[SKINEDIT_PARTS];
    int i;

    e = undo_pop (ui, before);
    if (e == NULL)
        return;
    if (e->kind == SKINEDIT_ENTRY_COLOR)
        for (i = 0; i < SKINEDIT_PARTS; i++)
            skinedit_model_set (ui->model, e, i, before[i]);
    else
        skinedit_model_set_text_raw (ui->model, e, before[0]);
    for (i = 0; i < SKINEDIT_PARTS; i++)
        g_free (before[i]);
    skinkeylist_goto_entry (ui->list, e);
    ui_changed (ui);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_edit_current (skineditor_ui_t *ui)
{
    skinedit_entry_t *e = skinkeylist_current (ui->list);

    if (e == NULL)
        return;
    if (e->kind == SKINEDIT_ENTRY_COLOR)
        edit_color_entry (ui, e);
    else
        edit_text_entry (ui, e);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_inherit_current (skineditor_ui_t *ui)
{
    skinedit_entry_t *e = skinkeylist_current (ui->list);

    if (e == NULL)
        return;
    undo_push (ui, e);
    if (e->kind == SKINEDIT_ENTRY_COLOR)
    {
        int i;

        for (i = 0; i < SKINEDIT_PARTS; i++)
            skinedit_model_set (ui->model, e, i, NULL);
    }
    else
        skinedit_model_set_text (ui->model, e, NULL);
    ui_changed (ui);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_reset_current (skineditor_ui_t *ui)
{
    skinedit_entry_t *e = skinkeylist_current (ui->list);

    if (e == NULL || !skinedit_entry_changed (e))
        return;
    undo_push (ui, e);
    skinedit_model_reset (ui->model, e);
    ui_changed (ui);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ask_name_and_description (skineditor_ui_t *ui, char **name, char **description)
{
    int rc;
    char *shown_description;

    *name = NULL;
    *description = NULL;
    shown_description = skinedit_text_from_skin (ui->model->description);
    {
        quick_widget_t widgets[] = {
            // clang-format off
            QUICK_LABELED_INPUT (_ ("Name:"), input_label_left, ui->model->name, "skineditor-name",
                                 name, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (_ ("Description:"), input_label_left, shown_description,
                                 "skineditor-description", description, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_NONE),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
            // clang-format on
        };

        WRect r = { -1, -1, 0, 60 };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = _ ("Save skin as"),
            .help = NULL,
            .widgets = widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        rc = quick_dialog (&qdlg);
    }
    g_free (shown_description);

    if (rc != B_ENTER || *name == NULL || **name == '\0')
    {
        MC_PTR_FREE (*name);
        MC_PTR_FREE (*description);
        return FALSE;
    }
    if (*description != NULL)
    {
        char *utf8 = skinedit_text_to_skin (*description);

        g_free (*description);
        *description = utf8;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* a skin keeps its color class unless the user agrees to raise it; FALSE = do not save */

static gboolean
ui_confirm_class (skineditor_ui_t *ui)
{
    skinedit_part_t part;
    skinedit_entry_t *over;
    skinedit_color_class_t needed;
    gboolean needs_256, needs_true;
    char *text;
    int rc;

    over = skinedit_model_over_class (ui->model, ui->model->colors, &part);
    if (over == NULL)
        return TRUE;

    skinedit_model_needs (ui->model, &needs_256, &needs_true);
    needed = needs_true ? SKINEDIT_COLOR_TRUECOLOR : SKINEDIT_COLOR_256;
    text = g_strdup_printf (_ ("This is a %s skin, but [%s] %s uses %s.\nSave it as a %s skin?"),
                            color_class_name (ui->model->colors), over->group, over->key,
                            over->effective[part] != NULL ? over->effective[part] : over->raw[part],
                            color_class_name (needed));
    rc = query_dialog (_ ("Save skin"), text, D_NORMAL, 2, _ ("&Yes"), _ ("&No"));
    g_free (text);
    return rc == 0;
}

/* --------------------------------------------------------------------------------------------- */

/* to the user's skins directory; a system skin or Save as asks the name; the saved skin runs */

static void
ui_save (skineditor_ui_t *ui, gboolean ask)
{
    char *name = NULL, *description = NULL;
    GError *error = NULL;

    if (ask)
    {
        if (!ask_name_and_description (ui, &name, &description))
            return;
    }
    else if (ui->model->system)
    {
        name = input_dialog (_ ("Save skin"),
                             _ ("This is a system skin. Save a copy in your skins directory as:"),
                             "skineditor-name", ui->model->name, INPUT_COMPLETE_NONE);
        if (name == NULL || *name == '\0')
        {
            g_free (name);
            return;
        }
    }
    else
        name = g_strdup (ui->model->name);

    if (!skinedit_model_name_ok (name))
    {
        message (D_ERROR, _ ("Save skin"),
                 _ ("\"%s\" is not a skin name: no directory part, not .ini"), name);
        g_free (name);
        g_free (description);
        return;
    }

    // another skin of that name in the user's directory is not written over unasked
    {
        char *target = skinedit_model_user_path (name);

        if (target != NULL && g_strcmp0 (target, ui->model->path) != 0
            && g_file_test (target, G_FILE_TEST_EXISTS)
            && query_dialog (_ ("Save skin"), _ ("A skin of this name exists. Overwrite it?"),
                             D_NORMAL, 2, _ ("&Yes"), _ ("&No"))
                != 0)
        {
            g_free (target);
            g_free (name);
            g_free (description);
            return;
        }
        g_free (target);
    }

    if (!ui_confirm_class (ui))
    {
        g_free (name);
        g_free (description);
        return;
    }

    if (!skinedit_model_save (ui->model, name, description, &error))
    {
        mc_error_message (&error, NULL);
        g_free (name);
        g_free (description);
        return;
    }

    mc_config_set_string (mc_global.main_config, CONFIG_APP_SECTION, "skin", name);
    g_free (ui->running_name);
    ui->running_name = g_strdup (name);
    ui_reload_running (ui);
    ui_set_title (ui);
    ui_update_info (ui);
    widget_draw (WIDGET (ui->dlg));

    g_free (name);
    g_free (description);
}

/* --------------------------------------------------------------------------------------------- */

/* TRUE when the editor may close: nothing to lose, or the user lets it go */

static gboolean
ui_cancel (skineditor_ui_t *ui)
{
    if (skinedit_model_dirty (ui->model)
        && query_dialog (_ ("Skin editor"), _ ("Discard the changes?"), D_NORMAL, 2, _ ("&Yes"),
                         _ ("&No"))
            != 0)
        return FALSE;
    if (ui->previewed)
        ui_reload_running (ui);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* @m replaces the model everywhere the dialog holds it */

static void
ui_take_model (skineditor_ui_t *ui, skinedit_model_t *m)
{
    undo_clear (ui);
    skinedit_model_free (ui->model);
    ui->model = m;
    ui->sample->model = m;
    skinkeylist_set_model (ui->list, m);
    ui_set_title (ui);
    ui->can_preview = ui_check_terminal (ui);
    ui_changed (ui);
}

/* --------------------------------------------------------------------------------------------- */

/* the screen shows the opened skin at once; Save makes it mc's, Cancel brings the old one back */

static void
ui_open_skin (skineditor_ui_t *ui, const char *name)
{
    skinedit_model_t *m;
    GError *error = NULL;

    if (strcmp (name, ui->model->name) == 0)
        return;
    if (skinedit_model_dirty (ui->model)
        && query_dialog (_ ("Skin editor"), _ ("Discard the changes?"), D_NORMAL, 2, _ ("&Yes"),
                         _ ("&No"))
            != 0)
        return;

    m = skinedit_model_open (name, &error);
    if (m == NULL)
    {
        mc_error_message (&error, NULL);
        return;
    }
    ui_take_model (ui, m);
}

/* --------------------------------------------------------------------------------------------- */

static void
ui_pick_skin (skineditor_ui_t *ui)
{
    WDialog *pop;
    WListbox *list;
    const WRect *br = &WIDGET (ui->skin_button)->rect;
    GPtrArray *names;
    int lines, cols, y, x, pos = 0;
    guint i;
    char *chosen = NULL;

    names = mc_skin_list ();
    lines = (int) names->len + 3;
    y = br->y + 1;
    if (y + lines > LINES)
        lines = LINES - y;
    if (lines < 4)
    {
        g_ptr_array_free (names, TRUE);
        return;
    }
    cols = br->cols;
    x = br->x;

    pop = dlg_create (TRUE, y, x, lines, cols, WPOS_KEEP_DEFAULT, TRUE, dialog_colors, NULL, NULL,
                      NULL, NULL);
    list = listbox_new (1, 1, lines - 2, cols - 2, FALSE, NULL);
    listbox_add_item (list, LISTBOX_APPEND_AT_END, 0, "default", (void *) "default", FALSE);
    if (strcmp (ui->model->name, "default") == 0)
        listbox_set_current (list, 0);
    for (i = 0; i < names->len; i++)
    {
        const char *name = g_ptr_array_index (names, i);

        if (strcmp (name, "default") == 0)
            continue;
        pos++;
        listbox_add_item (list, LISTBOX_APPEND_AT_END, 0, name, (void *) name, FALSE);
        if (strcmp (name, ui->model->name) == 0)
            listbox_set_current (list, pos);
    }
    group_add_widget_autopos (GROUP (pop), list, WPOS_KEEP_ALL, NULL);

    if (dlg_run (pop) == B_ENTER)
    {
        char *label;
        void *data;

        listbox_get_current (list, &label, &data);
        if (data != NULL)
            chosen = g_strdup ((const char *) data);
    }
    widget_destroy (WIDGET (pop));
    g_ptr_array_free (names, TRUE);

    if (chosen != NULL)
        ui_open_skin (ui, chosen);
    g_free (chosen);
    widget_select (WIDGET (ui->list));
}

/* --------------------------------------------------------------------------------------------- */

/* the class button: a list of the three under it; the skin is declared for the one picked */

static void
ui_pick_class (skineditor_ui_t *ui)
{
    WDialog *pop;
    WListbox *list;
    const WRect *br = &WIDGET (ui->class_button)->rect;
    int lines = G_N_ELEMENTS (class_order) + 2;
    guint i;

    if (br->y + 1 + lines > LINES)
        return;

    pop = dlg_create (TRUE, br->y + 1, br->x, lines, br->cols, WPOS_KEEP_DEFAULT, TRUE,
                      dialog_colors, NULL, NULL, NULL, NULL);
    list = listbox_new (1, 1, lines - 2, br->cols - 2, FALSE, NULL);
    for (i = 0; i < G_N_ELEMENTS (class_order); i++)
    {
        listbox_add_item (list, LISTBOX_APPEND_AT_END, 0, color_class_name (class_order[i]),
                          GINT_TO_POINTER ((int) class_order[i]), FALSE);
        if (class_order[i] == ui->model->colors)
            listbox_set_current (list, (int) i);
    }
    group_add_widget_autopos (GROUP (pop), list, WPOS_KEEP_ALL, NULL);

    if (dlg_run (pop) == B_ENTER)
    {
        char *label;
        void *data;

        listbox_get_current (list, &label, &data);
        ui->model->colors = (skinedit_color_class_t) GPOINTER_TO_INT (data);
    }
    widget_destroy (WIDGET (pop));

    ui_update_info (ui);
    widget_select (WIDGET (ui->list));
}

/* --------------------------------------------------------------------------------------------- */

static char *
clip_text (const skineditor_ui_t *ui)
{
    GString *s = g_string_new (NULL);
    int i;

    for (i = 0; i < SKINEDIT_PARTS; i++)
    {
        if (i > 0)
            g_string_append (s, i == SKINEDIT_PART_BG ? _ (" on ") : ", ");
        g_string_append (s, ui->clip[i] != NULL ? ui->clip[i] : _ ("inherited"));
    }
    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-Ins: the three parts of the key under the cursor, as written, inherited parts included */

static void
ui_copy_current (skineditor_ui_t *ui)
{
    const skinedit_entry_t *e = skinkeylist_current (ui->list);
    char *text;
    int i;

    if (e == NULL || e->kind != SKINEDIT_ENTRY_COLOR)
        return;
    for (i = 0; i < SKINEDIT_PARTS; i++)
    {
        g_free (ui->clip[i]);
        ui->clip[i] = g_strdup (e->raw[i]);
    }
    ui->has_clip = TRUE;
    text = clip_text (ui);
    {
        char *line = g_strdup_printf (_ ("Copied: %s"), text);

        set_info_line (ui->value_label, line);
        g_free (line);
    }
    g_free (text);
}

/* --------------------------------------------------------------------------------------------- */

/* Shift-Ins: asks what to paste into the key under the cursor */

static void
ui_paste_current (skineditor_ui_t *ui)
{
    skinedit_entry_t *e = skinkeylist_current (ui->list);
    const char *choices[] = { N_ ("&Everything"), N_ ("&Foreground only"), N_ ("&Background only"),
                              N_ ("&Attributes only") };
    int choice = 0, rc;
    char *what;

    if (e == NULL || e->kind != SKINEDIT_ENTRY_COLOR || !ui->has_clip)
        return;

    what = clip_text (ui);
    {
        quick_widget_t widgets[] = {
            // clang-format off
            QUICK_LABEL (what, NULL),
            QUICK_SEPARATOR (FALSE),
            QUICK_RADIO (4, choices, &choice, NULL),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
            // clang-format on
        };

        WRect r = { -1, -1, 0, 44 };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = e->label,
            .help = NULL,
            .widgets = widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        rc = quick_dialog (&qdlg);
    }
    g_free (what);
    if (rc != B_ENTER)
        return;

    undo_push (ui, e);
    if (choice == 0 || choice == 1)
        skinedit_model_set (ui->model, e, SKINEDIT_PART_FG, ui->clip[SKINEDIT_PART_FG]);
    if (choice == 0 || choice == 2)
        skinedit_model_set (ui->model, e, SKINEDIT_PART_BG, ui->clip[SKINEDIT_PART_BG]);
    if (choice == 0 || choice == 3)
        skinedit_model_set (ui->model, e, SKINEDIT_PART_ATTRS, ui->clip[SKINEDIT_PART_ATTRS]);
    ui_changed (ui);
}

/* --------------------------------------------------------------------------------------------- */

/* The help file next to the plugin: the user's copy first, as the loader does with the .so */

static void
ui_help (void)
{
    char *user_path;
    ev_help_t ev = { NULL, "[skineditor]", "[main]" };

    user_path = g_build_filename (g_get_home_dir (), ".local", "lib", "mc", "panel-plugins",
                                  "skineditor", "skineditor.hlp", (char *) NULL);
    ev.filename =
        g_file_test (user_path, G_FILE_TEST_EXISTS) ? user_path : MC_PLUGIN_DIR "/skineditor.hlp";
    mc_event_raise (MCEVENT_GROUP_CORE, "help", &ev);
    g_free (user_path);
}

/* --------------------------------------------------------------------------------------------- */

/* the model read again from its file, after mcedit or the like changed it */

static void
ui_reopen_file (skineditor_ui_t *ui)
{
    skinedit_model_t *m;
    GError *error = NULL;

    m = skinedit_model_open_file (ui->model->path, ui->model->name, &error);
    if (m == NULL)
    {
        mc_error_message (&error, NULL);
        return;
    }
    ui_take_model (ui, m);
}

/* --------------------------------------------------------------------------------------------- */

/* the skin file in mcedit; unsaved changes saved or dropped first, the file read again after */

static void
ui_open_in_mcedit (skineditor_ui_t *ui)
{
    edit_arg_t *arg;

    if (skinedit_model_dirty (ui->model))
    {
        int rc;

        rc = query_dialog (_ ("Skin editor"), _ ("The skin has unsaved changes."), D_NORMAL, 3,
                           _ ("&Save"), _ ("&Discard"), _ ("&Cancel"));
        if (rc == 0)
        {
            ui_save (ui, FALSE);
            if (skinedit_model_dirty (ui->model))
                return;  // the name was not given
        }
        else if (rc != 1)
            return;
    }

    arg = edit_arg_new (ui->model->path, 1);
    edit_file (arg);
    edit_arg_free (arg);
    ui_reopen_file (ui);
}

/* --------------------------------------------------------------------------------------------- */

static int
button_cb (WButton *button, int action)
{
    skineditor_ui_t *ui = ui_of (WIDGET (button));

    switch (action)
    {
    case B_EDIT:
        ui_edit_current (ui);
        return 0;
    case B_SAVE:
        ui_save (ui, FALSE);
        return 0;
    case B_SAVE_AS:
        ui_save (ui, TRUE);
        return 0;
    case B_SKIN:
        ui_pick_skin (ui);
        return 0;
    case B_CLASS:
        ui_pick_class (ui);
        return 0;
    case B_EDIT_FILE:
        ui_open_in_mcedit (ui);
        return 0;
    case B_CANCEL:
        return ui_cancel (ui) ? 1 : 0;
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
list_on_change (WSkinKeyList *l, void *data)
{
    (void) l;
    ui_update_info ((skineditor_ui_t *) data);
}

/* --------------------------------------------------------------------------------------------- */

static void
list_on_edit (WSkinKeyList *l, void *data)
{
    (void) l;
    ui_edit_current ((skineditor_ui_t *) data);
}

/* --------------------------------------------------------------------------------------------- */

/* Space: the sample pane keeps the colors of the current key only, until Space again */

static void
list_on_show (WSkinKeyList *l, void *data)
{
    skineditor_ui_t *ui = (skineditor_ui_t *) data;

    (void) l;
    ui->sample->isolate = !ui->sample->isolate;
    widget_draw (WIDGET (ui->sample));
}

/* --------------------------------------------------------------------------------------------- */

static void
sample_on_pick (WSkinSample *s, skinedit_entry_t *e, void *data)
{
    skineditor_ui_t *ui = (skineditor_ui_t *) data;

    (void) s;
    skinkeylist_goto_entry (ui->list, e);
    widget_select (WIDGET (ui->list));
}

/* --------------------------------------------------------------------------------------------- */

static void
sample_on_edit (WSkinSample *s, skinedit_entry_t *e, void *data)
{
    skineditor_ui_t *ui = (skineditor_ui_t *) data;

    (void) s;
    skinkeylist_goto_entry (ui->list, e);
    ui_edit_current (ui);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
ui_dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    skineditor_ui_t *ui = (skineditor_ui_t *) DIALOG (w)->data.p;

    switch (msg)
    {
    case MSG_RESIZE:
        dlg_default_callback (w, sender, msg, parm, data);
        ui_layout (ui);
        return MSG_HANDLED;

    case MSG_KEY:
        switch (parm)
        {
        case KEY_F (1):
            ui_help ();
            return MSG_HANDLED;
        case KEY_F (2):
            ui_save (ui, FALSE);
            return MSG_HANDLED;
        case KEY_F (12):
            ui_save (ui, TRUE);
            return MSG_HANDLED;
        case KEY_F (4):
            ui_edit_current (ui);
            return MSG_HANDLED;
        case KEY_F (5):
            ui_reset_current (ui);
            return MSG_HANDLED;
        case KEY_DC:
            ui_inherit_current (ui);
            return MSG_HANDLED;
        case KEY_M_CTRL | KEY_IC:
            ui_copy_current (ui);
            return MSG_HANDLED;
        case KEY_M_SHIFT | KEY_IC:
            ui_paste_current (ui);
            return MSG_HANDLED;
        case XCTRL ('u'):
            ui_undo (ui);
            return MSG_HANDLED;
        default:
            return MSG_NOT_HANDLED;
        }

    case MSG_IDLE:
        // the idle loop neither waits nor flushes the screen by itself
        g_usleep (50000);
        skinsample_tick (ui->sample);
        widget_update_cursor (WIDGET (ui->dlg));
        mc_refresh ();
        return MSG_HANDLED;

    case MSG_ACTION:
        if (parm == CK_Cancel)
        {
            if (ui_cancel (ui))
            {
                ui->dlg->ret_value = B_CANCEL;
                dlg_close (ui->dlg);
            }
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
skineditor_entry_color (const skinedit_entry_t *e)
{
    tty_color_pair_t pair;

    pair.fg = e->effective[SKINEDIT_PART_FG];
    pair.bg = e->effective[SKINEDIT_PART_BG];
    pair.attrs = e->effective[SKINEDIT_PART_ATTRS];
    pair.pair_index = 0;
    return tty_try_alloc_color_pair (&pair, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
skineditor_run (void)
{
    skineditor_ui_t ui;
    WGroup *g;
    GError *error = NULL;
    int i;

    memset (&ui, 0, sizeof (ui));
    ui.model = skinedit_model_open (mc_skin__default.name, &error);
    if (ui.model == NULL)
    {
        mc_error_message (&error, NULL);
        return;
    }
    ui.running_name = g_strdup (mc_skin__default.name);
    ui.can_preview = TRUE;
    ui.undo = g_array_new (FALSE, FALSE, sizeof (undo_step_t));

    ui.dlg = dlg_create (TRUE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, dialog_colors,
                         ui_dialog_callback, NULL, NULL, "");
    ui.dlg->data.p = &ui;
    g = GROUP (ui.dlg);
    ui_set_title (&ui);

    ui.skin_label = label_new (2, 2, _ ("Skin:"));
    group_add_widget (g, ui.skin_label);
    ui.skin_button = button_new (2, 8, B_SKIN, NORMAL_BUTTON, "", button_cb);
    group_add_widget (g, ui.skin_button);
    ui.class_button = button_new (2, 40, B_CLASS, NORMAL_BUTTON, "", button_cb);
    group_add_widget (g, ui.class_button);

    ui.list = skinkeylist_new (3, 2, 10, MIN_LIST_COLS, ui.model);
    ui.list->on_change = list_on_change;
    ui.list->on_edit = list_on_edit;
    ui.list->on_show = list_on_show;
    ui.list->data = &ui;
    group_add_widget (g, ui.list);

    ui.sample = skinsample_new (2, 40, 10, 30, ui.model);
    ui.sample->on_pick = sample_on_pick;
    ui.sample->on_edit = sample_on_edit;
    ui.sample->data = &ui;
    group_add_widget (g, ui.sample);

    ui.info_line = hline_new (13, 0, -1);
    group_add_widget (g, ui.info_line);
    ui.value_label = label_new (14, 2, "");
    group_add_widget (g, ui.value_label);
    ui.desc_label = label_new (15, 2, "");
    group_add_widget (g, ui.desc_label);
    ui.button_line = hline_new (16, 0, -1);
    group_add_widget (g, ui.button_line);

    ui.edit_button = button_new (17, 2, B_EDIT, NORMAL_BUTTON, _ ("&Edit"), button_cb);
    group_add_widget (g, ui.edit_button);
    ui.save_button = button_new (17, 12, B_SAVE, NORMAL_BUTTON, _ ("&Save"), button_cb);
    group_add_widget (g, ui.save_button);
    ui.save_as_button = button_new (17, 22, B_SAVE_AS, NORMAL_BUTTON, _ ("Save &as..."), button_cb);
    group_add_widget (g, ui.save_as_button);
    ui.edit_file_button =
        button_new (17, 30, B_EDIT_FILE, NORMAL_BUTTON, _ ("Edit as &file"), button_cb);
    group_add_widget (g, ui.edit_file_button);
    ui.cancel_button = button_new (17, 36, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), button_cb);
    group_add_widget (g, ui.cancel_button);

    {
        int min_cols = MAX (MIN_COLS, ui_buttons_width (&ui) + 4);

        if (COLS < min_cols || LINES < MIN_LINES)
        {
            message (D_ERROR, _ ("Skin editor"), _ ("The skin editor needs a terminal of %dx%d"),
                     min_cols, MIN_LINES);
            widget_destroy (WIDGET (ui.dlg));
            skinedit_model_free (ui.model);
            g_free (ui.running_name);
            g_array_free (ui.undo, TRUE);
            return;
        }
    }

    ui_layout (&ui);
    ui_update_info (&ui);
    widget_select (WIDGET (ui.list));

    dlg_run (ui.dlg);
    widget_destroy (WIDGET (ui.dlg));

    tty_color_free_temp ();
    skinedit_model_free (ui.model);
    g_free (ui.running_name);
    for (i = 0; i < SKINEDIT_PARTS; i++)
        g_free (ui.clip[i]);
    undo_clear (&ui);
    g_array_free (ui.undo, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
