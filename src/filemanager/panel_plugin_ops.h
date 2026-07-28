/*
   Panel plugin file operations -- copy, move, delete, create, put.

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

/** \file panel_plugin_ops.h
 *  \brief Header: file operations on a panel driven by a panel plugin
 */

#ifndef MC__PANEL_PLUGIN_OPS_H
#define MC__PANEL_PLUGIN_OPS_H

#include "lib/global.h"

#include "panel.h"  // WPanel

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures) ****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void plugin_panel_copy_cmd (WPanel *panel, gboolean single);
void plugin_panel_move_cmd (WPanel *panel, gboolean single);
void plugin_panel_delete_cmd (WPanel *panel, gboolean single);
void plugin_panel_create_cmd (WPanel *panel);
void plugin_panel_edit_new_cmd (WPanel *panel);
void plugin_panel_put_cmd (WPanel *panel);
void plugin_panel_put_move_cmd (WPanel *panel);

/*** inline functions ****************************************************************************/

#endif
