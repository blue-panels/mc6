/*
   Archive browser panel plugin -archiver settings dialog.

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

#include <config.h>

#include <string.h>

#include "lib/global.h"
#include "lib/tty/key.h"
#include "lib/keybind.h"
#include "lib/widget.h"

#include "arcmc-types.h"
#include "arcmc-config.h"
#include "archive-io.h"
#include "dialog-settings.h"

/*** file scope variables ************************************************************************/

/* table widgets, used by the dialog callback to tell the two tables apart */
static WTable *settings_tbl_ext = NULL;
static WTable *settings_tbl_builtin = NULL;

/*** file scope macro definitions ****************************************************************/

/* Columns: Format(7) | Ext(8) | Pack(8) | Unpack(8) | Tool(10) | On(3).
   The choice columns spend one character on the cursor mark. */
#define SETTINGS_TABLE_NCOLS 6
#define SETTINGS_TABLE_WIDTH 49 /* 7+1+8+1+8+1+8+1+10+1+3 = 49 */

/* Column indices */
#define SETTINGS_COL_PACK   2
#define SETTINGS_COL_UNPACK 3

/*** file scope functions ************************************************************************/

/* ---- Table datasource callbacks for the settings dialog ---- */

/* Edited copy of the settings, committed to the format table on OK. Cancel
   drops it, so nothing the format editor changed survives either. */
typedef struct
{
    arcmc_backend_t pack;
    arcmc_backend_t unpack;
    gboolean enabled;
    char *pack_bin;
    char *pack_args;
    char *unpack_bin;
    char *helper;
} settings_row_t;

/* working copy shown by the dialog, needed by the callback that opens the editor */
static settings_row_t *settings_builtin_rows = NULL;

/* labels of the format editor that name a backend, they are highlighted */
static unsigned long params_lbl_both_id = 0;
static unsigned long params_lbl_extern_id = 0;

/* --------------------------------------------------------------------------------------------- */

/* Paint the two backend names of the format editor in the highlight color. */
static cb_ret_t
builtin_params_dlg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    if (msg == MSG_INIT)
    {
        cb_ret_t ret;
        Widget *lw;

        ret = dlg_default_callback (w, sender, msg, parm, data);

        lw = widget_find_by_id (w, params_lbl_both_id);
        if (lw != NULL)
            LABEL (lw)->color_idx = DLG_COLOR_HOT_NORMAL;

        lw = widget_find_by_id (w, params_lbl_extern_id);
        if (lw != NULL)
            LABEL (lw)->color_idx = DLG_COLOR_HOT_NORMAL;

        return ret;
    }

    return dlg_default_callback (w, sender, msg, parm, data);
}

/* Tool name with a marker when the program is not in PATH. */
static const char *
settings_tool_text (const char *bin)
{
    static char buf[32];

    if (bin == NULL)
        return "-";

    if (arcmc_check_bin_available (bin))
        return bin;

    g_snprintf (buf, sizeof (buf), "%s !", bin);

    return buf;
}

/* --------------------------------------------------------------------------------------------- */

static int
settings_builtin_get_nrows (const void *data)
{
    (void) data;
    return (int) arcmc_builtin_formats_count;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
settings_builtin_get_text (const void *data, int row, int col)
{
    const settings_row_t *rows = (const settings_row_t *) data;
    const arcmc_builtin_format_t *f;

    if (row < 0 || row >= (int) arcmc_builtin_formats_count)
        return "";

    f = &arcmc_builtin_formats[row];

    switch (col)
    {
    case 0:
        return f->name;
    case 1:
        return f->ext;
    case SETTINGS_COL_PACK:
        if (!f->lib_pack && rows[row].pack_bin == NULL)
            return "-";
        return arcmc_backend_name (rows[row].pack);
    case SETTINGS_COL_UNPACK:
        if (!f->lib_unpack && rows[row].unpack_bin == NULL)
            return "-";
        return arcmc_backend_name (rows[row].unpack);
    case 4:
        return settings_tool_text (arcmc_resolve_tool (
            rows[row].pack_bin != NULL ? rows[row].pack_bin : rows[row].unpack_bin));
    default:
        return "";
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
settings_builtin_get_checked (const void *data, int row, int col)
{
    const settings_row_t *rows = (const settings_row_t *) data;

    (void) col;

    return rows[row].enabled;
}

/* --------------------------------------------------------------------------------------------- */

static void
settings_builtin_set_checked (void *data, int row, int col, gboolean val)
{
    settings_row_t *rows = (settings_row_t *) data;

    (void) col;

    rows[row].enabled = val;
}

/* --------------------------------------------------------------------------------------------- */

/* Step a Pack/Unpack cell to the next value this build can serve. */
static void
settings_builtin_cycle (void *data, int row, int col, int dir)
{
    settings_row_t *rows = (settings_row_t *) data;
    const arcmc_builtin_format_t *f;
    arcmc_backend_t *val;
    arcmc_backend_t orig;
    gboolean lib;
    const char *bin;
    int i;

    if (row < 0 || row >= (int) arcmc_builtin_formats_count)
        return;

    f = &arcmc_builtin_formats[row];

    if (col == SETTINGS_COL_PACK)
    {
        val = &rows[row].pack;
        lib = f->lib_pack;
        bin = rows[row].pack_bin;
    }
    else if (col == SETTINGS_COL_UNPACK)
    {
        val = &rows[row].unpack;
        lib = f->lib_unpack;
        bin = rows[row].unpack_bin;
    }
    else
        return;

    orig = *val;

    for (i = 0; i < ARCMC_BACKEND_COUNT; i++)
    {
        *val = (arcmc_backend_t) (((int) *val + dir + ARCMC_BACKEND_COUNT) % ARCMC_BACKEND_COUNT);

        if (arcmc_backend_possible (*val, lib, bin))
            return;
    }

    /* nothing else to offer */
    *val = orig;
}

/* --------------------------------------------------------------------------------------------- */

static int
settings_ext_get_nrows (const void *data)
{
    (void) data;
    return (int) ext_archivers_count;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
settings_ext_get_text (const void *data, int row, int col)
{
    const arcmc_ext_archiver_t *a;

    (void) data;

    if (row < 0 || row >= (int) ext_archivers_count)
        return "";

    a = &ext_archivers[row];

    switch (col)
    {
    case 0:
        return a->name;
    case 1:
        return a->ext;
    case SETTINGS_COL_PACK:
        return a->pack_bin != NULL ? "extern" : "-";
    case SETTINGS_COL_UNPACK:
        return a->unpack_bin != NULL ? "extern" : "-";
    case 4:
        return settings_tool_text (a->pack_bin != NULL ? a->pack_bin : a->unpack_bin);
    default:
        return "";
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
settings_get_checked (const void *data, int row, int col)
{
    const gboolean *checks = (const gboolean *) data;

    (void) col;

    return checks[row];
}

/* --------------------------------------------------------------------------------------------- */

static void
settings_set_checked (void *data, int row, int col, gboolean val)
{
    gboolean *checks = (gboolean *) data;

    (void) col;

    checks[row] = val;
}

/* --------------------------------------------------------------------------------------------- */

/* Split "7z a -y -t7z" into the program and its arguments. */
static void
settings_split_command (const char *cmd, char **bin, char **args)
{
    const char *sp;

    *bin = NULL;
    *args = NULL;

    if (cmd == NULL)
        return;

    while (*cmd == ' ')
        cmd++;

    if (*cmd == '\0')
        return;

    sp = strchr (cmd, ' ');
    if (sp == NULL)
    {
        *bin = g_strdup (cmd);
        return;
    }

    *bin = g_strndup (cmd, (gsize) (sp - cmd));

    while (*sp == ' ')
        sp++;

    if (*sp != '\0')
        *args = g_strdup (sp);
}

/* --------------------------------------------------------------------------------------------- */

/* Backends and tool of one builtin format. `row` is the working copy of the
   parent dialog, so its Cancel still discards the change. */
static void
arcmc_show_builtin_params_dialog (size_t idx, settings_row_t *row)
{
    static const char *backend_options[] = {
        N_ ("&builtin"),
        N_ ("bo&th"),
        N_ ("&external"),
        N_ ("&off"),
    };
    static const arcmc_backend_t backend_order[] = {
        ARCMC_BACKEND_BUILTIN,
        ARCMC_BACKEND_BOTH,
        ARCMC_BACKEND_EXTERN,
        ARCMC_BACKEND_OFF,
    };

    const arcmc_builtin_format_t *f;
    const char *tool;
    char *pack_cmd;
    char *found;
    char *found_label;
    char *new_pack_cmd = NULL;
    char *new_helper = NULL;
    int pack_sel;
    int unpack_sel;
    int ret;

    if (idx >= arcmc_builtin_formats_count)
        return;

    f = &arcmc_builtin_formats[idx];

    pack_sel = 0;
    unpack_sel = 0;
    {
        size_t i;

        for (i = 0; i < G_N_ELEMENTS (backend_order); i++)
        {
            if (backend_order[i] == row->pack)
                pack_sel = (int) i;
            if (backend_order[i] == row->unpack)
                unpack_sel = (int) i;
        }
    }

    pack_cmd =
        g_strconcat (row->pack_bin != NULL ? row->pack_bin : "", row->pack_args != NULL ? " " : "",
                     row->pack_args != NULL ? row->pack_args : "", (char *) NULL);

    tool = arcmc_resolve_tool (row->pack_bin != NULL ? row->pack_bin : row->unpack_bin);
    found = tool != NULL ? g_find_program_in_path (tool) : NULL;
    found_label = g_strdup_printf (_ ("Found in PATH: %s"), found != NULL ? found : _ ("no"));
    g_free (found);

    {
        /* *INDENT-OFF* */
        quick_widget_t quick_widgets[] = {
            QUICK_LABEL (f->ext, NULL),
            QUICK_SEPARATOR (FALSE),
            QUICK_START_COLUMNS,
            QUICK_START_GROUPBOX (N_ ("Pack")),
            QUICK_RADIO (G_N_ELEMENTS (backend_options), backend_options, &pack_sel, NULL),
            QUICK_STOP_GROUPBOX,
            QUICK_NEXT_COLUMN,
            QUICK_START_GROUPBOX (N_ ("Unpack")),
            QUICK_RADIO (G_N_ELEMENTS (backend_options), backend_options, &unpack_sel, NULL),
            QUICK_STOP_GROUPBOX,
            QUICK_STOP_COLUMNS,
            QUICK_LABEL (N_ ("Both:"), &params_lbl_both_id),
            QUICK_LABEL (N_ ("use libarchive when possible; fall back to the"), NULL),
            QUICK_LABEL (N_ ("external tool for unsupported archives."), NULL),
            QUICK_LABEL (N_ ("For 7z, this applies only to encrypted archives."), NULL),
            QUICK_SEPARATOR (FALSE),
            QUICK_LABEL (N_ ("External:"), &params_lbl_extern_id),
            QUICK_LABEL (N_ ("always use the external tool for this format."), NULL),
            QUICK_SEPARATOR (FALSE),
            QUICK_LABELED_INPUT (N_ ("Pack command:"), input_label_left, pack_cmd,
                                 "arcmc-builtin-pack-cmd", &new_pack_cmd, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_FILENAMES),
            QUICK_LABELED_INPUT (N_ ("Unpack helper:"), input_label_left,
                                 row->helper != NULL ? row->helper : "", "arcmc-builtin-helper",
                                 &new_helper, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABEL (found_label, NULL),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
        };
        /* *INDENT-ON* */

        WRect r = { -1, -1, 0, 64 };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = f->name,
            .help = "[arcmc]",
            .widgets = quick_widgets,
            .callback = builtin_params_dlg_callback,
            .mouse_callback = NULL,
        };

        ret = quick_dialog (&qdlg);
    }

    if (ret == B_ENTER)
    {
        arcmc_backend_t pack = backend_order[pack_sel];
        arcmc_backend_t unpack = backend_order[unpack_sel];
        char *bin = NULL;
        char *args = NULL;

        settings_split_command (new_pack_cmd, &bin, &args);

        /* the tool of the format may have just been named, judge by the new value */
        if (!arcmc_backend_possible (pack, f->lib_pack, bin)
            || !arcmc_backend_possible (unpack, f->lib_unpack, bin != NULL ? bin : row->unpack_bin))
            message (D_ERROR, MSG_ERROR, "%s", _ ("This backend is not available for the format"));
        else
        {
            row->pack = pack;
            row->unpack = unpack;

            g_free (row->pack_bin);
            g_free (row->pack_args);
            row->pack_bin = bin;
            row->pack_args = args;
            bin = NULL;
            args = NULL;

            /* the same program serves reading, that is what the helper runs */
            if (row->pack_bin != NULL)
            {
                g_free (row->unpack_bin);
                row->unpack_bin = g_strdup (row->pack_bin);
            }

            g_free (row->helper);
            row->helper =
                (new_helper != NULL && new_helper[0] != '\0') ? g_strdup (new_helper) : NULL;
        }

        g_free (bin);
        g_free (args);
    }

    g_free (new_pack_cmd);
    g_free (new_helper);
    g_free (pack_cmd);
    g_free (found_label);
}

/* --------------------------------------------------------------------------------------------- */

/* Show sub-dialog for editing external archiver parameters. */
static void
arcmc_show_ext_params_dialog (size_t idx)
{
    arcmc_ext_archiver_t *a;
    char *pack_bin = NULL;
    char *pack_args = NULL;
    char *unpack_bin = NULL;
    char *unpack_args = NULL;
    char *test_bin = NULL;
    char *test_args = NULL;
    char *list_file_arg = NULL;
    char *extfs_helper = NULL;
    char title[64];
    int ret;

    a = &ext_archivers[idx];
    g_snprintf (title, sizeof (title), "%s parameters", a->name);

    {
        /* *INDENT-OFF* */
        quick_widget_t quick_widgets[] = {
            QUICK_LABELED_INPUT (N_ ("Pack binary:"), input_label_above,
                                 a->pack_bin != NULL ? a->pack_bin : "", "arcmc-ext-pack-bin",
                                 &pack_bin, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
            QUICK_LABELED_INPUT (N_ ("Pack arguments:"), input_label_above,
                                 a->pack_args != NULL ? a->pack_args : "", "arcmc-ext-pack-args",
                                 &pack_args, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_ ("Unpack binary:"), input_label_above,
                                 a->unpack_bin != NULL ? a->unpack_bin : "", "arcmc-ext-unpack-bin",
                                 &unpack_bin, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
            QUICK_LABELED_INPUT (N_ ("Unpack arguments:"), input_label_above,
                                 a->unpack_args != NULL ? a->unpack_args : "",
                                 "arcmc-ext-unpack-args", &unpack_args, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_ ("Test binary:"), input_label_above,
                                 a->test_bin != NULL ? a->test_bin : "", "arcmc-ext-test-bin",
                                 &test_bin, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
            QUICK_LABELED_INPUT (N_ ("Test arguments:"), input_label_above,
                                 a->test_args != NULL ? a->test_args : "", "arcmc-ext-test-args",
                                 &test_args, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_ ("File list argument:"), input_label_above,
                                 a->list_file_arg != NULL ? a->list_file_arg : "",
                                 "arcmc-ext-list-file-arg", &list_file_arg, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_ ("Extfs helper:"), input_label_above,
                                 a->extfs_helper != NULL ? a->extfs_helper : "",
                                 "arcmc-ext-extfs-helper", &extfs_helper, NULL, FALSE, FALSE,
                                 INPUT_COMPLETE_NONE),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
        };
        /* *INDENT-ON* */

        WRect r = { -1, -1, 0, 50 };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = title,
            .help = "[arcmc]",
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        ret = quick_dialog (&qdlg);
    }

    if (ret == B_ENTER)
    {
        /* helper: set field from dialog result, empty string -> NULL */
#define SET_FIELD(field, val)                                                                      \
    do                                                                                             \
    {                                                                                              \
        if (val != NULL && val[0] != '\0')                                                         \
            a->field = val;                                                                        \
        else                                                                                       \
        {                                                                                          \
            a->field = NULL;                                                                       \
            g_free (val);                                                                          \
        }                                                                                          \
    }                                                                                              \
    while (0)

        SET_FIELD (pack_bin, pack_bin);
        SET_FIELD (pack_args, pack_args);
        SET_FIELD (unpack_bin, unpack_bin);
        SET_FIELD (unpack_args, unpack_args);
        SET_FIELD (test_bin, test_bin);
        SET_FIELD (test_args, test_args);
        SET_FIELD (list_file_arg, list_file_arg);
        SET_FIELD (extfs_helper, extfs_helper);

#undef SET_FIELD

        arcmc_config_save ();
    }
    else
    {
        g_free (pack_bin);
        g_free (pack_args);
        g_free (unpack_bin);
        g_free (unpack_args);
        g_free (test_bin);
        g_free (test_args);
        g_free (list_file_arg);
        g_free (extfs_helper);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Dialog callback for the settings dialog - F4 edits ext archiver params,
   double-click on ext archiver row also opens the editor. */
static cb_ret_t
settings_dlg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_UNHANDLED_KEY:
        if (parm == KEY_F (4) && settings_tbl_ext != NULL
            && widget_get_state (WIDGET (settings_tbl_ext), WST_FOCUSED))
        {
            int row = table_get_current (settings_tbl_ext);

            if (row >= 0 && row < (int) ext_archivers_count)
                arcmc_show_ext_params_dialog ((size_t) row);
            return MSG_HANDLED;
        }

        if (parm == KEY_F (4) && settings_tbl_builtin != NULL
            && widget_get_state (WIDGET (settings_tbl_builtin), WST_FOCUSED))
        {
            int row = table_get_current (settings_tbl_builtin);

            if (settings_builtin_rows != NULL && row >= 0
                && row < (int) arcmc_builtin_formats_count)
            {
                arcmc_show_builtin_params_dialog ((size_t) row, &settings_builtin_rows[row]);
                /* the sub-dialog restores the screen it covered, cells included */
                widget_draw (WIDGET (settings_tbl_builtin));
            }
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_NOTIFY:
        /* double-click on ext archiver row */
        if (sender != NULL && settings_tbl_ext != NULL && sender == WIDGET (settings_tbl_ext)
            && parm == CK_Enter)
        {
            int row = table_get_current (settings_tbl_ext);

            if (row >= 0 && row < (int) ext_archivers_count)
                arcmc_show_ext_params_dialog ((size_t) row);
            return MSG_HANDLED;
        }

        if (sender != NULL && settings_tbl_builtin != NULL
            && sender == WIDGET (settings_tbl_builtin) && parm == CK_Enter)
        {
            int row = table_get_current (settings_tbl_builtin);

            if (settings_builtin_rows != NULL && row >= 0
                && row < (int) arcmc_builtin_formats_count)
            {
                arcmc_show_builtin_params_dialog ((size_t) row, &settings_builtin_rows[row]);
                /* the sub-dialog restores the screen it covered, cells included */
                widget_draw (WIDGET (settings_tbl_builtin));
            }
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/*** public functions ****************************************************************************/

void
arcmc_show_settings_dialog (void)
{
    /* only the builtin table has a backend to choose */
    static const table_column_def_t col_defs_builtin[SETTINGS_TABLE_NCOLS] = {
        { 7, J_LEFT, TABLE_COL_TEXT },    /* Format */
        { 8, J_LEFT, TABLE_COL_TEXT },    /* Ext */
        { 8, J_LEFT, TABLE_COL_CHOICE },  /* Pack */
        { 8, J_LEFT, TABLE_COL_CHOICE },  /* Unpack */
        { 10, J_LEFT, TABLE_COL_TEXT },   /* Tool */
        { 3, J_CENTER, TABLE_COL_CHECK }, /* On */
    };
    static const table_column_def_t col_defs_ext[SETTINGS_TABLE_NCOLS] = {
        { 7, J_LEFT, TABLE_COL_TEXT },    /* Format */
        { 8, J_LEFT, TABLE_COL_TEXT },    /* Ext */
        { 8, J_LEFT, TABLE_COL_TEXT },    /* Pack */
        { 8, J_LEFT, TABLE_COL_TEXT },    /* Unpack */
        { 10, J_LEFT, TABLE_COL_TEXT },   /* Tool */
        { 3, J_CENTER, TABLE_COL_CHECK }, /* On */
    };

    WDialog *dlg;
    WGroup *g;
    WTable *tbl_builtin;
    WTable *tbl_ext;
    settings_row_t *builtin_rows;
    gboolean *ext_checks;
    int dlg_width = SETTINGS_TABLE_WIDTH + 6;
    int dlg_height;
    int builtin_lines;
    int ext_lines;
    int y;
    size_t i;

    builtin_lines = ((int) arcmc_builtin_formats_count * 2 + 2) / 3;
    ext_lines = ((int) ext_archivers_count * 2 + 2) / 3;
    /* header(1) + separator(1) + builtins + hline(1) + externals + hline(1) + hint(1)
       + hline(1) + button(1) */
    dlg_height = 1 + 1 + builtin_lines + 1 + ext_lines + 1 + 1 + 1 + 1 + 2;

    dlg = dlg_create (TRUE, 0, 0, dlg_height, dlg_width, WPOS_CENTER, TRUE, dialog_colors,
                      settings_dlg_callback, NULL, "[arcmc]", _ ("Archiver settings"));
    g = GROUP (dlg);

    y = 1;

    /* column header */
    {
        WLabel *lbl;

        lbl = label_new (y++, 3, _ ("Format  Ext       Pack     Unpack   Tool       On"));
        lbl->color_idx = DLG_COLOR_TITLE;
        group_add_widget (g, lbl);
    }

    {
        WHLine *hl;

        hl = hline_new (y++, -1, -1);
        hl->text_color_idx = DLG_COLOR_TITLE;
        hline_set_text (hl, _ (" Builtin (libarchive) "));
        group_add_widget (g, hl);
    }

    /* builtin libarchive formats */
    /* the last column lands on the frame, that is where the scrollbar goes */
    tbl_builtin =
        table_new (y, 2, builtin_lines, dlg_width - 2, SETTINGS_TABLE_NCOLS, col_defs_builtin);
    tbl_builtin->scrollbar_on_frame = TRUE;
    builtin_rows = g_new (settings_row_t, arcmc_builtin_formats_count);
    for (i = 0; i < arcmc_builtin_formats_count; i++)
    {
        const arcmc_builtin_format_t *f = &arcmc_builtin_formats[i];

        builtin_rows[i].pack = f->pack;
        builtin_rows[i].unpack = f->unpack;
        builtin_rows[i].enabled = f->enabled;
        builtin_rows[i].pack_bin = g_strdup (f->pack_bin);
        builtin_rows[i].pack_args = g_strdup (f->pack_args);
        builtin_rows[i].unpack_bin = g_strdup (f->unpack_bin);
        builtin_rows[i].helper = g_strdup (f->extfs_helper);
    }
    {
        table_datasource_t ds = { settings_builtin_get_nrows,
                                  settings_builtin_get_text,
                                  settings_builtin_get_checked,
                                  settings_builtin_set_checked,
                                  builtin_rows,
                                  settings_builtin_cycle };

        table_set_datasource (tbl_builtin, ds);
    }
    settings_tbl_builtin = tbl_builtin;
    settings_builtin_rows = builtin_rows;
    group_add_widget (g, tbl_builtin);
    y += builtin_lines;

    {
        WHLine *hl;

        hl = hline_new (y++, -1, -1);
        hl->text_color_idx = DLG_COLOR_TITLE;
        hline_set_text (hl, _ (" External archivers "));
        group_add_widget (g, hl);
    }

    /* external archivers */
    tbl_ext = table_new (y, 2, ext_lines, dlg_width - 2, SETTINGS_TABLE_NCOLS, col_defs_ext);
    tbl_ext->scrollbar_on_frame = TRUE;
    settings_tbl_ext = tbl_ext;
    ext_checks = g_new (gboolean, ext_archivers_count > 0 ? ext_archivers_count : 1);
    for (i = 0; i < ext_archivers_count; i++)
        ext_checks[i] = arcmc_ext_enabled != NULL ? arcmc_ext_enabled[i] : TRUE;
    {
        table_datasource_t ds = { settings_ext_get_nrows,
                                  settings_ext_get_text,
                                  settings_get_checked,
                                  settings_set_checked,
                                  ext_checks,
                                  NULL };

        table_set_datasource (tbl_ext, ds);
    }
    group_add_widget (g, tbl_ext);
    y += ext_lines;

    {
        WHLine *hl;

        hl = hline_new (y++, -1, -1);
        hl->text_color_idx = DLG_COLOR_TITLE;
        hline_set_text (hl, _ (" Hint "));
        group_add_widget (g, hl);
    }

    {
        WLabel *lbl;

        lbl = label_new (y++, 3, _ ("Space - change   F4 - tool params   ! - missing"));
        lbl->color_idx = DLG_COLOR_TITLE;
        group_add_widget (g, lbl);
    }

    group_add_widget (g, hline_new (y++, -1, -1));

    group_add_widget (
        g,
        button_new (dlg_height - 2, (dlg_width - 8) / 2, B_ENTER, DEFPUSH_BUTTON, _ ("&OK"), NULL));

    widget_select (WIDGET (tbl_builtin));

    if (dlg_run (dlg) == B_ENTER)
    {
        for (i = 0; i < arcmc_builtin_formats_count; i++)
        {
            arcmc_builtin_format_t *f = &arcmc_builtin_formats[i];

            f->pack = builtin_rows[i].pack;
            f->unpack = builtin_rows[i].unpack;
            f->enabled = builtin_rows[i].enabled;

            /* the replaced value is a .rodata literal or a config string of the
               session, program-lifetime either way, so it is not freed */
            if (g_strcmp0 (f->pack_bin, builtin_rows[i].pack_bin) != 0)
                f->pack_bin = g_strdup (builtin_rows[i].pack_bin);
            if (g_strcmp0 (f->pack_args, builtin_rows[i].pack_args) != 0)
                f->pack_args = g_strdup (builtin_rows[i].pack_args);
            if (g_strcmp0 (f->unpack_bin, builtin_rows[i].unpack_bin) != 0)
                f->unpack_bin = g_strdup (builtin_rows[i].unpack_bin);
            if (g_strcmp0 (f->extfs_helper, builtin_rows[i].helper) != 0)
                f->extfs_helper = g_strdup (builtin_rows[i].helper);
        }

        for (i = 0; i < ext_archivers_count; i++)
            if (arcmc_ext_enabled != NULL)
                arcmc_ext_enabled[i] = ext_checks[i];

        arcmc_config_save ();
    }

    for (i = 0; i < arcmc_builtin_formats_count; i++)
    {
        g_free (builtin_rows[i].pack_bin);
        g_free (builtin_rows[i].pack_args);
        g_free (builtin_rows[i].unpack_bin);
        g_free (builtin_rows[i].helper);
    }

    g_free (builtin_rows);
    g_free (ext_checks);
    settings_tbl_ext = NULL;
    settings_tbl_builtin = NULL;
    settings_builtin_rows = NULL;
    widget_destroy (WIDGET (dlg));
}

/* --------------------------------------------------------------------------------------------- */
