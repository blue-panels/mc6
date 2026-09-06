/** \file skineditor_sample.h
 *  \brief Header: WSkinSample, a mock-up of the screen area a section paints
 */

#ifndef MC__SKINEDITOR_SAMPLE_H
#define MC__SKINEDITOR_SAMPLE_H

#include "lib/global.h"
#include "lib/widget.h"

#include "skinedit_model.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define SKINSAMPLE(x) ((WSkinSample *) (x))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WSkinSample WSkinSample;

struct WSkinSample
{
    Widget widget;
    skinedit_model_t *model;
    skinedit_section_t *section; /* what is drawn */
    skinedit_entry_t *current;   /* the key under the cursor of the list */
    GArray *regions;             /* skinsample_region_t, rebuilt on every draw */
    gboolean isolate;            /* only the areas of the current key keep their colors */
    unsigned int tick;           /* spinner frame */
    gint64 tick_at;              /* monotonic time of the last frame */

    /* the user clicked the area of an entry */
    void (*on_pick) (WSkinSample *s, skinedit_entry_t *e, void *data);
    /* the user double-clicked it */
    void (*on_edit) (WSkinSample *s, skinedit_entry_t *e, void *data);
    void *data;
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WSkinSample *skinsample_new (int y, int x, int lines, int cols, skinedit_model_t *model);
void skinsample_set (WSkinSample *s, skinedit_section_t *section, skinedit_entry_t *current);
/* advance the spinner; call from the idle handler */
void skinsample_tick (WSkinSample *s);

/*** inline functions ****************************************************************************/

#endif
