/** \file skineditor_keylist.h
 *  \brief Header: WSkinKeyList, the skin keys as one list with section headings
 */

#ifndef MC__SKINEDITOR_KEYLIST_H
#define MC__SKINEDITOR_KEYLIST_H

#include "lib/global.h"
#include "lib/widget.h"

#include "skinedit_model.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define SKINKEYLIST(x) ((WSkinKeyList *) (x))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WSkinKeyList WSkinKeyList;

typedef struct
{
    skinedit_section_t *section;
    skinedit_entry_t *entry; /* NULL for a heading */
} skinkeylist_row_t;

struct WSkinKeyList
{
    Widget widget;
    skinedit_model_t *model;
    GArray *rows; /* skinkeylist_row_t */
    int top;      /* first visible row */
    int current;  /* an entry row, or -1 */

    /* the cursor moved to another entry */
    void (*on_change) (WSkinKeyList *l, void *data);
    /* Enter, F4 or a double click on the entry */
    void (*on_edit) (WSkinKeyList *l, void *data);
    /* Space: show where the entry is */
    void (*on_show) (WSkinKeyList *l, void *data);
    void *data;
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WSkinKeyList *skinkeylist_new (int y, int x, int lines, int cols, skinedit_model_t *model);
/* show another skin; the cursor goes to the first entry */
void skinkeylist_set_model (WSkinKeyList *l, skinedit_model_t *model);
skinedit_entry_t *skinkeylist_current (const WSkinKeyList *l);
skinedit_section_t *skinkeylist_current_section (const WSkinKeyList *l);
void skinkeylist_goto_entry (WSkinKeyList *l, const skinedit_entry_t *e);

/*** inline functions ****************************************************************************/

#endif
