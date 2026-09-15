/** \file dir_watch.h
 *  \brief Header: keep the panels in step with the directories they show
 */

#ifndef MC__DIR_WATCH_H
#define MC__DIR_WATCH_H

#include "panel.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void dir_watch_init (void);
void dir_watch_shutdown (void);

/* Watch the directory the panel shows now, or stop watching when it is not a local one. */
void dir_watch_track (WPanel *panel);
void dir_watch_forget (WPanel *panel);

/* Reread the panels the filesystem has changed under. Only the visible ones: a hidden panel
   keeps its mark and is reread when it comes back on screen. */
void dir_watch_reload_pending (void);

/*** inline functions ****************************************************************************/

#endif
