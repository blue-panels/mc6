/** \file hexstrip.h
 *  \brief Header: WHexStrip, a few rows of hex dump over a byte source
 */

#ifndef MC__WIDGET_HEXSTRIP_H
#define MC__WIDGET_HEXSTRIP_H

#include <sys/types.h>

#include "lib/global.h"

/* forward declarations needed by widget-common.h */
struct Widget;
typedef struct Widget Widget;
struct WGroup;
typedef struct WGroup WGroup;

#include "lib/widget/rect.h"
#include "lib/widget/widget-common.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define HEXSTRIP(x) ((WHexStrip *) (x))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WHexStrip WHexStrip;

/* colors of the dump; -1 = the viewer color of the skin */
typedef struct
{
    int normal;  /* bytes and text */
    int offset;  /* the address column */
    int mark;    /* the highlighted range */
    int changed; /* bytes with an unsaved edit */
    int cursor;  /* the cursor byte in the focused column */
    int frame;   /* group separators */
    int block;   /* the range selected by the user */
} hexstrip_colors_t;

typedef struct
{
    off_t (*get_size) (void *ctx);
    gssize (*read) (void *ctx, off_t offset, void *buf, gsize len);
    /* optional: TRUE for a byte with an unsaved edit */
    gboolean (*is_changed) (void *ctx, off_t offset);
    void *ctx;
} hexstrip_source_t;

struct WHexStrip
{
    Widget widget;
    hexstrip_source_t source;

    off_t top;    /* offset of the first displayed byte, row aligned */
    off_t cursor; /* cursor byte */
    gboolean low_nibble;
    gboolean in_text; /* cursor in the ascii column */

    off_t mark_start; /* highlighted range */
    off_t mark_len;
    off_t block_start; /* a range selected by the user, drawn over the mark */
    off_t block_len;

    int bytes_per_line;
    int text_start;
    int cursor_y; /* cached for MSG_CURSOR */
    int cursor_x;

    /* the user moved the cursor */
    void (*on_cursor) (WHexStrip *h, void *data);
    /* the user typed a byte; return TRUE when the source took it */
    gboolean (*on_edit) (WHexStrip *h, off_t offset, unsigned char value, void *data);
    void *data;
    hexstrip_colors_t colors;
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WHexStrip *hexstrip_new (int y, int x, int lines, int cols);
void hexstrip_set_source (WHexStrip *h, const hexstrip_source_t *source);
void hexstrip_set_colors (WHexStrip *h, const hexstrip_colors_t *colors);
void hexstrip_set_handlers (WHexStrip *h, void (*on_cursor) (WHexStrip *, void *),
                            gboolean (*on_edit) (WHexStrip *, off_t, unsigned char, void *),
                            void *data);

/* highlight a range and bring it into view */
void hexstrip_set_mark (WHexStrip *h, off_t offset, off_t len);
/* len 0 clears the block */
void hexstrip_set_block (WHexStrip *h, off_t offset, off_t len);
void hexstrip_set_cursor (WHexStrip *h, off_t offset);
off_t hexstrip_get_cursor (const WHexStrip *h);
void hexstrip_move (WHexStrip *h, off_t delta);
void hexstrip_scroll (WHexStrip *h, int rows);

/* pure layout: how many bytes fit in a row and where the ascii column starts */
void hexstrip_layout (int cols, int *bytes_per_line, int *text_start);
/* screen column of byte i in a row */
int hexstrip_byte_col (int cols, int bytes_per_line, int i);

/*** inline functions ****************************************************************************/

#endif
