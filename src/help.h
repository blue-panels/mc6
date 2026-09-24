/** \file help.h
 *  \brief Header: hypertext file browser
 *
 *  The help file is markdown: src/help_md.c turns it into nodes, links
 *  between them and a contents page, and this browser paints that.
 *
 *  Laziness/widgeting attack: This file does use the dialog manager
 *  and uses mainly the dialog to achieve the help work.  there is only
 *  one specialized widget and it's only used to forward the mouse messages
 *  to the appropriate routine.
 */

#ifndef MC__HELP_H
#define MC__HELP_H

/*** typedefs(not structures) and defined constants **********************************************/

/* Markers of the text the help window paints, put there by help_md.c */
#define CHAR_LINK_START   '\01'   // Ctrl-A
#define CHAR_LINK_POINTER '\02'   // Ctrl-B
#define CHAR_LINK_END     '\03'   // Ctrl-C
#define CHAR_NODE_END     '\04'   // Ctrl-D
#define CHAR_ALTERNATE    '\05'   // Ctrl-E
#define CHAR_NORMAL       '\06'   // Ctrl-F
#define CHAR_VERSION      '\07'   // Ctrl-G
#define CHAR_FONT_BOLD    '\010'  // Ctrl-H
#define CHAR_FONT_NORMAL  '\013'  // Ctrl-K
#define CHAR_FONT_ITALIC  '\024'  // Ctrl-T
/* A place inside a node that a link can lead to; the name of it follows and
   another one closes it */
#define CHAR_ANCHOR '\016'  // Ctrl-N

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean help_interactive_display (const gchar *event_group_name, const gchar *event_name,
                                   gpointer init_data, gpointer data);

/*** inline functions ****************************************************************************/
#endif
