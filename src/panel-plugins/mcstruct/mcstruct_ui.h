/** \file mcstruct_ui.h
 *  \brief Header: the mcstruct screen
 */

#ifndef MC__MCSTRUCT_UI_H
#define MC__MCSTRUCT_UI_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file **********************************************************/

/*** declarations of public functions ************************************************************/

/* open the screen for a local file; @hint selects the def-file (magic group, def name or path),
   NULL = by alias / extension / menu */
/* @last_offset, when not NULL, receives the hex cursor offset when the user left with
   Shift-F4 (back to the caller), -1 on quit */
gboolean mcstruct_run (const char *path, const char *display_name, const char *hint,
                       off_t *last_offset);

#endif
