/** \file skineditor_ui.h
 *  \brief Header: skin editor dialog
 */

#ifndef MC__SKINEDITOR_UI_H
#define MC__SKINEDITOR_UI_H

#include "lib/global.h"

#include "skinedit_model.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Open the editor on the skin mc is running with. */
void skineditor_run (void);

/* A temporary color pair for the effective colors of a COLOR entry. Allocated on every call:
   the live preview drops all pairs when it reloads the skin. */
int skineditor_entry_color (const skinedit_entry_t *e);

/*** inline functions ****************************************************************************/

#endif
