/** \file slv_load.h
 *  \brief Header: def-file search path, aliases and settings
 */

#ifndef MC__MCSTRUCT_SLV_LOAD_H
#define MC__MCSTRUCT_SLV_LOAD_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    int tree_lines; /* 0 = whatever is left after hex and def */
    int def_layout; /* 0 auto, 1 right of the tree, 2 below */
    int def_width;  /* percent of the screen for the def-file zone on the right */
    int hex_lines;
    int def_lines;
    int name_width;
    int offset_column; /* 0 none, 1 local, 2 global */
    int grid_rows;     /* first grid column: 0 none, 1 row number, 2 row offset */
    gboolean show_hidden;
    gint64 lazy_rows;
    char *float_format;
} slv_settings_t;

/*** global variables defined in .c file **********************************************************/

/*** declarations of public functions ************************************************************/

/* directories searched for def-files, user first; NULL terminated, caller frees with g_strfreev */
char **slv_load_search_dirs (void);

/* full path of a def-file for @file_name: hint (magic group or def name), aliases, extension.
   NULL when nothing matches. */
char *slv_load_find_def (const char *file_name, const char *hint);

/* every *.stl on the search path, user files shadow system ones; full paths */
GPtrArray *slv_load_list_defs (void);

/* mcstruct.ini, user file over the plugin defaults */
void slv_settings_load (slv_settings_t *s);
/* write to the user ini (first search dir), TRUE on success */
gboolean slv_settings_save (const slv_settings_t *s);
void slv_settings_free (slv_settings_t *s);

/* test hooks: override the search path */
void slv_load_set_search_dirs (const char *const *dirs);

gboolean slv_float_format_valid (const char *fmt);

#endif
