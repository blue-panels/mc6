/*
   Skin editor plugin - registration.

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

#include "lib/global.h"
#include "lib/panel-plugin.h"

#include "skineditor_ui.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

static void *skineditor_open (mc_panel_host_t *host, const char *open_path);
static void skineditor_close (void *plugin_data);
static mc_pp_result_t skineditor_get_items (void *plugin_data, void *list);
static void *skineditor_action (mc_panel_host_t *host, const char *open_path);
static void skineditor_configure (void);

/*** file scope variables ************************************************************************/

static const mc_pp_action_t skineditor_actions[] = {
    { N_ ("Skin &editor"), skineditor_action },
};

static const mc_pp_cmd_menu_entry_t skineditor_menu[] = {
    { N_ ("S&kin editor..."), 0, NULL, 0, MC_PP_MENU_COMMAND },
};

/* no panel: one dialog, reached from the Command menu, the plugin list and Manage Plugins */
static const mc_panel_plugin_t skineditor_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "skineditor",
    .display_name = N_ ("Skin editor"),
    .proto = NULL,
    .prefix = NULL,
    .flags = MC_PPF_NONE,
    .open = skineditor_open,
    .close = skineditor_close,
    .get_items = skineditor_get_items,
    .actions = skineditor_actions,
    .action_count = G_N_ELEMENTS (skineditor_actions),
    .cmd_menu_entries = skineditor_menu,
    .cmd_menu_entry_count = G_N_ELEMENTS (skineditor_menu),
    .configure = skineditor_configure,
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* the plugin chooser and the drive menu come here: run the editor, open no panel */

static void *
skineditor_open (mc_panel_host_t *host, const char *open_path)
{
    (void) host;
    (void) open_path;
    skineditor_run ();
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
skineditor_close (void *plugin_data)
{
    (void) plugin_data;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
skineditor_get_items (void *plugin_data, void *list)
{
    (void) plugin_data;
    (void) list;
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static void *
skineditor_action (mc_panel_host_t *host, const char *open_path)
{
    (void) host;
    (void) open_path;
    skineditor_run ();
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
skineditor_configure (void)
{
    skineditor_run ();
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &skineditor_plugin;
}

/* --------------------------------------------------------------------------------------------- */
