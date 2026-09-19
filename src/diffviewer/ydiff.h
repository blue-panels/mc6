#ifndef MC__DIFFVIEW_YDIFF_H
#define MC__DIFFVIEW_YDIFF_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

int diff_view (const char *file1, const char *file2, const char *label1, const char *label2);
/* The comparison options, also reachable from the Options menu of the file manager. */
void dview_options_box (void);
gboolean dview_diff_cmd (const void *f0, const void *f1);

#endif
