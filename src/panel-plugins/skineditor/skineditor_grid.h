/** \file skineditor_grid.h
 *  \brief Header: WColorGrid, a palette of color cells
 */

#ifndef MC__SKINEDITOR_GRID_H
#define MC__SKINEDITOR_GRID_H

#include "lib/global.h"
#include "lib/widget.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define COLORGRID(x) ((WColorGrid *) (x))

/*** enums ***************************************************************************************/

typedef enum
{
    COLORGRID_16, /* the 16 named colors, two rows */
    COLORGRID_256 /* system colors, the 6x6x6 cube, the grays */
} colorgrid_map_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WColorGrid WColorGrid;

typedef struct
{
    int y, x;          /* relative to the widget */
    const char *value; /* "blue", "color135" */
} colorgrid_cell_t;

struct WColorGrid
{
    Widget widget;
    GArray *cells;  /* colorgrid_cell_t */
    int current;    /* the cell under the cursor */
    char *selected; /* the value shown with a mark, may be NULL or not in the grid */

    /* the cursor moved */
    void (*on_move) (WColorGrid *g, const char *value, void *data);
    /* Enter, Space or a click: the user picked a cell */
    void (*on_pick) (WColorGrid *g, const char *value, void *data);
    void *data;
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WColorGrid *colorgrid_new (int y, int x, colorgrid_map_t map);
/* the size a grid of this map takes */
void colorgrid_size (colorgrid_map_t map, int *lines, int *cols);
void colorgrid_set_selected (WColorGrid *g, const char *value);
const char *colorgrid_current (const WColorGrid *g);
/* the foreground that reads on a cell of this color */
const char *colorgrid_contrast (const char *value);

/*** inline functions ****************************************************************************/

#endif
