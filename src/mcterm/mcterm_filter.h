/** \file mcterm_filter.h
 *  \brief Header: showing only the rows of the output that match
 */

#ifndef MC__MCTERM_FILTER_H
#define MC__MCTERM_FILTER_H

#include "lib/global.h"

#include "src/viewer/vterm.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* The rows that matched, as a view of their own. Rows are counted from the first
   one ever printed, the way the marked region counts them. The set is fixed when
   the filter goes on: what the shell prints afterwards is not looked at. */
typedef struct
{
    char *pattern;  // what was filtered by; NULL while the filter is off
    GArray *rows;   // gint64, oldest first
    int top;        // the row drawn at the top of the screen, as an index into @rows
} mcterm_filter_t;

/*** declarations of public functions ************************************************************/

gboolean mcterm_filter_active (const mcterm_filter_t *f);
void mcterm_filter_clear (mcterm_filter_t *f);

/* Collect the rows of the output that match @pattern, the oldest row of the history
   through @newest. FALSE, the filter left off, when none of them does. */
gboolean mcterm_filter_apply (mcterm_filter_t *f, mcview_vterm_t *vt, int cols, gint64 newest,
                              const char *pattern);

int mcterm_filter_len (const mcterm_filter_t *f);
/* The row at @index, -1 outside the set. */
gint64 mcterm_filter_row (const mcterm_filter_t *f, int index);
/* Where @row stands among the rows that matched: its own place, or that of the
   first row after it. The last index when it is past them all. */
int mcterm_filter_index (const mcterm_filter_t *f, gint64 row);

/* The text of @row, its trailing blanks left out. Caller frees. */
char *mcterm_filter_row_text (mcview_vterm_t *vt, gint64 row, int cols);

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_FILTER_H */
