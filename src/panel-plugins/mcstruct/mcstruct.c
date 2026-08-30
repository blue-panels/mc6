/*
   mcstruct - structured binary viewer plugin (Struct Look)

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

#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/widget.h"

#include "slv_load.h"
#include "mcstruct_ui.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* the plugin has no panel of its own */
static void *
mcstruct_open (mc_panel_host_t *host, const char *open_path)
{
    (void) host;
    (void) open_path;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcstruct_close (void *plugin_data)
{
    (void) plugin_data;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcstruct_get_items (void *plugin_data, void *list)
{
    (void) plugin_data;
    (void) list;
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcstruct_show (mc_panel_host_t *host, const char *display_name, const char *local_path,
               const char *hint)
{
    off_t last = -1;
    gboolean ok;

    ok = mcstruct_run (local_path, display_name, hint, &last);
    /* Shift-F4: the caller continues at the byte the user was on; no offset on quit */
    if (host != NULL && last >= 0)
    {
        g_free (host->focus_after);
        host->focus_after = g_strdup_printf ("@0x%llx", (unsigned long long) last);
    }
    return ok ? MC_PPR_OK : MC_PPR_FAILED;
}

/* --------------------------------------------------------------------------------------------- */

/* Command menu: the file under the cursor in the current panel */
static void *
mcstruct_action_open (mc_panel_host_t *host, const char *open_path)
{
    const GString *name;
    char *path;

    (void) open_path;
    if (host == NULL || host->get_current == NULL)
        return NULL;
    name = host->get_current (host);
    if (name == NULL || name->len == 0 || strcmp (name->str, "..") == 0)
        return NULL;
    if (g_path_is_absolute (name->str))
        path = g_strdup (name->str);
    else
    {
        char *cwd = g_get_current_dir ();

        path = g_build_filename (cwd, name->str, (char *) NULL);
        g_free (cwd);
    }
    if (!g_file_test (path, G_FILE_TEST_IS_REGULAR))
        message (D_ERROR, _ ("Struct look"), _ ("%s is not a regular file"), name->str);
    else
        mcstruct_run (path, name->str, NULL, NULL);
    g_free (path);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcstruct_configure (void)
{
    slv_settings_t s;
    char *hex_lines = NULL, *def_lines = NULL, *name_width = NULL, *lazy_rows = NULL;
    char *frag_days = NULL, *frag_def;
    char *tree_lines = NULL, *tree_def;
    char *float_format = NULL, *hex_def, *def_def, *name_def, *lazy_def;
    int offset_column, def_layout, grid_rows;
    gboolean show_hidden;
    const char *offset_items[] = { N_ ("&None"), N_ ("&Local"), N_ ("&Global") };
    const char *grid_items[] = { N_ ("N&one"), N_ ("Row n&umber"), N_ ("Row o&ffset") };
    const char *layout_items[] = { N_ ("&Auto (right when wide)"), N_ ("&Right of the tree"),
                                   N_ ("&Below the hex zone") };
    char *def_width = NULL, *width_def;

    slv_settings_load (&s);
    tree_def = g_strdup_printf ("%d", s.tree_lines);
    hex_def = g_strdup_printf ("%d", s.hex_lines);
    def_def = g_strdup_printf ("%d", s.def_lines);
    name_def = g_strdup_printf ("%d", s.name_width);
    lazy_def = g_strdup_printf ("%d", (int) s.lazy_rows);
    frag_def = g_strdup_printf ("%d", s.fragment_days);
    offset_column = s.offset_column;
    grid_rows = s.grid_rows;
    def_layout = s.def_layout;
    show_hidden = s.show_hidden;
    width_def = g_strdup_printf ("%d", s.def_width);

    {
        /* clang-format off */
        quick_widget_t quick_widgets[] = {
            QUICK_START_GROUPBOX (N_("Zones")),
                QUICK_LABELED_INPUT (N_("Tree lines (0 = the rest):"), input_label_left, tree_def,
                                     "mcstruct-tree-lines", &tree_lines, NULL, FALSE, FALSE,
                                     INPUT_COMPLETE_NONE),
                QUICK_LABELED_INPUT (N_("Hex zone lines (0 hides):"), input_label_left, hex_def,
                                     "mcstruct-hex-lines", &hex_lines, NULL, FALSE, FALSE,
                                     INPUT_COMPLETE_NONE),
                QUICK_LABELED_INPUT (N_("Def-file lines (0 = the rest):"), input_label_left,
                                     def_def, "mcstruct-def-lines", &def_lines, NULL, FALSE,
                                     FALSE, INPUT_COMPLETE_NONE),
                QUICK_LABELED_INPUT (N_("Def-file width on the right, %:"), input_label_left,
                                     width_def, "mcstruct-def-width", &def_width, NULL, FALSE,
                                     FALSE, INPUT_COMPLETE_NONE),
            QUICK_STOP_GROUPBOX,
            QUICK_START_GROUPBOX (N_("Def-file zone")),
                QUICK_RADIO (3, layout_items, &def_layout, NULL),
            QUICK_STOP_GROUPBOX,
            QUICK_START_GROUPBOX (N_("Structure tree")),
                QUICK_LABELED_INPUT (N_("Name column width:"), input_label_left, name_def,
                                     "mcstruct-name-width", &name_width, NULL, FALSE, FALSE,
                                     INPUT_COMPLETE_NONE),
                QUICK_LABELED_INPUT (N_("Build rows lazily above:"), input_label_left, lazy_def,
                                     "mcstruct-lazy-rows", &lazy_rows, NULL, FALSE, FALSE,
                                     INPUT_COMPLETE_NONE),
                QUICK_LABELED_INPUT (N_("Float format (printf):"), input_label_left,
                                     s.float_format, "mcstruct-float", &float_format, NULL,
                                     FALSE, FALSE, INPUT_COMPLETE_NONE),
                QUICK_CHECKBOX (N_("Show &hidden structures"), &show_hidden, NULL),
            QUICK_STOP_GROUPBOX,
            QUICK_START_GROUPBOX (N_("Saved bytes")),
                QUICK_LABELED_INPUT (N_("Keep fragments, days (0 = always):"), input_label_left,
                                     frag_def, "mcstruct-fragment-days", &frag_days, NULL, FALSE,
                                     FALSE, INPUT_COMPLETE_NONE),
            QUICK_STOP_GROUPBOX,
            QUICK_START_GROUPBOX (N_("Offset column")),
                QUICK_RADIO (3, offset_items, &offset_column, NULL),
            QUICK_STOP_GROUPBOX,
            QUICK_START_GROUPBOX (N_("First grid column")),
                QUICK_RADIO (3, grid_items, &grid_rows, NULL),
            QUICK_STOP_GROUPBOX,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END,
        };
        /* clang-format on */

        WRect r = { -1, -1, 0, 56 };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = N_ ("Struct look settings"),
            .help = "[mcstruct]",
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        if (quick_dialog (&qdlg) == B_ENTER)
        {
            s.tree_lines = atoi (tree_lines != NULL ? tree_lines : "");
            s.hex_lines = atoi (hex_lines != NULL ? hex_lines : "");
            s.def_lines = atoi (def_lines != NULL ? def_lines : "");
            s.name_width = atoi (name_width != NULL ? name_width : "");
            s.lazy_rows = atoi (lazy_rows != NULL ? lazy_rows : "");
            s.fragment_days = atoi (frag_days != NULL ? frag_days : "");
            s.offset_column = offset_column;
            s.grid_rows = grid_rows;
            s.def_layout = def_layout;
            s.def_width = atoi (def_width != NULL ? def_width : "");
            s.show_hidden = show_hidden;
            if (slv_float_format_valid (float_format))
            {
                g_free (s.float_format);
                s.float_format = float_format;
                float_format = NULL;
            }
            if (!slv_settings_save (&s))
                message (D_ERROR, _ ("Struct look"), _ ("Cannot write the settings file"));
        }
    }

    g_free (tree_lines);
    g_free (tree_def);
    g_free (def_width);
    g_free (width_def);
    g_free (hex_lines);
    g_free (def_lines);
    g_free (name_width);
    g_free (lazy_rows);
    g_free (frag_days);
    g_free (frag_def);
    g_free (float_format);
    g_free (hex_def);
    g_free (def_def);
    g_free (name_def);
    g_free (lazy_def);
    slv_settings_free (&s);
}

/* --------------------------------------------------------------------------------------------- */

static const mc_pp_file_operation_t mcstruct_file_operations[] = {
    {
        .name = "show",
        .kind = MC_PP_FILE_OPERATION_SHOW,
        .show = mcstruct_show,
    },
};

static const mc_pp_action_t mcstruct_actions[] = {
    { N_ ("Struct look"), mcstruct_action_open },
};

static const mc_pp_cmd_menu_entry_t mcstruct_menu[] = {
    { N_ ("Struct loo&k"), 0, NULL, 0, MC_PP_MENU_COMMAND },
};

static const mc_panel_plugin_t mcstruct_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "mcstruct",
    .display_name = N_ ("Struct look"),
    .proto = NULL,
    .prefix = NULL,
    .flags = MC_PPF_NONE,
    .open = mcstruct_open,
    .close = mcstruct_close,
    .get_items = mcstruct_get_items,
    .file_operations = mcstruct_file_operations,
    .file_operation_count = G_N_ELEMENTS (mcstruct_file_operations),
    .actions = mcstruct_actions,
    .action_count = G_N_ELEMENTS (mcstruct_actions),
    .cmd_menu_entries = mcstruct_menu,
    .cmd_menu_entry_count = G_N_ELEMENTS (mcstruct_menu),
    .configure = mcstruct_configure,
};

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &mcstruct_plugin;
}

/* --------------------------------------------------------------------------------------------- */
