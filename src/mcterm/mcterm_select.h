/** \file mcterm_select.h
 *  \brief Header: marking the output of the embedded terminal
 */

#ifndef MC__MCTERM_SELECT_H
#define MC__MCTERM_SELECT_H

#include "lib/global.h"

#include "src/viewer/vterm.h"
#include "src/viewer/terminal_buffer.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* A region of the output; rows are counted from the first one ever printed. */
typedef struct
{
    gboolean anchored;  // a click or the first mark key has set the anchor
    gboolean active;    // the region has since grown to at least one cell
    gint64 anchor_row;
    gint64 point_row;
    int anchor_col;
    int point_col;
} mcterm_sel_t;

/*** declarations of public functions ************************************************************/

void mcterm_sel_clear (mcterm_sel_t *sel);
void mcterm_sel_start (mcterm_sel_t *sel, gint64 row, int col);
void mcterm_sel_extend (mcterm_sel_t *sel, gint64 row, int col);

/* The part of @row that is marked, as [*from, *to). FALSE if none of it is. */
gboolean mcterm_sel_row_span (const mcterm_sel_t *sel, gint64 row, int cols, int *from, int *to);

const mcview_vterm_cell_t *mcterm_sel_cell_at (mcview_vterm_t *vt, gint64 row, int col);

/* The marked text, trailing blanks of each row dropped, rows joined by a
   newline. NULL when nothing is marked. */
char *mcterm_sel_text (const mcterm_sel_t *sel, mcview_vterm_t *vt, int cols);

/* Into the clipfile, and from there into the external clipboard, which is
   what the editor and the input line do with their own selections. */
gboolean mcterm_sel_copy (const mcterm_sel_t *sel, mcview_vterm_t *vt, int cols);

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_SELECT_H */
