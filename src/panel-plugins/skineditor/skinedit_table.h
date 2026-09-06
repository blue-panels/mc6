/** \file skinedit_table.h
 *  \brief Header: the keys a skin may set, with labels
 */

#ifndef MC__SKINEDIT_TABLE_H
#define MC__SKINEDIT_TABLE_H

#include "skinedit_model.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    const char *group;
    const char *key;
    skinedit_kind_t kind;
    const char *label;       /* N_ () */
    const char *description; /* N_ (), may be NULL */
    const char *builtin;     /* CHAR/STRING only */
} skinedit_table_row_t;

typedef struct
{
    const char *label; /* N_ () */
    const skinedit_table_row_t *rows;
    size_t nrows;
} skinedit_table_section_t;

/*** global variables defined in .c file *********************************************************/

extern const skinedit_table_section_t skinedit_table[];
extern const size_t skinedit_table_count;

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif
