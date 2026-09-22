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
gboolean plugin_panel_dirsize_cmd (WPanel *panel);

/*** inline functions ****************************************************************************/

#endif
