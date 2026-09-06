/** \file skineditor_color.h
 *  \brief Header: the Color dialog and the 256-color picker
 */

#ifndef MC__SKINEDITOR_COLOR_H
#define MC__SKINEDITOR_COLOR_H

#include "lib/global.h"

#include "skinedit_model.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* called after every change of the entry while the dialog is open, and once more on Cancel
   after the old values are back */
typedef void (*skineditor_change_fn) (void *data);

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Edit the three parts of a COLOR entry. Changes go to the model as they are made; Cancel puts
   the values the dialog opened with back. Returns TRUE on OK. */
gboolean skineditor_color_dialog (skinedit_model_t *m, skinedit_entry_t *e,
                                  skineditor_change_fn on_change, void *data);

/* pick one of the 256 colors; NULL on Cancel, else a "colorN" string to free */
char *skineditor_pick_256 (const char *current);

/*** inline functions ****************************************************************************/

#endif
