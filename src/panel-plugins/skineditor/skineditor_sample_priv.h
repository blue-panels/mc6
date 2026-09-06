/** \file skineditor_sample_priv.h
 *  \brief Header: drawing primitives shared by the sample pane and its mock-ups
 */

#ifndef MC__SKINEDITOR_SAMPLE_PRIV_H
#define MC__SKINEDITOR_SAMPLE_PRIV_H

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "skineditor_sample.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    WRect rect; /* relative to the pane */
    skinedit_entry_t *entry;
} skinsample_region_t;

typedef void (*skinsample_draw_fn) (WSkinSample *s, const WRect *r);

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* relative to the pane and clipped; each paints in the key's colors and records the area */

skinedit_entry_t *sample_entry (WSkinSample *s, const char *group, const char *key);
int sample_color (WSkinSample *s, const char *group, const char *key);
void sample_text (WSkinSample *s, int y, int x, const char *group, const char *key,
                  const char *text);
/* text with one hot letter after '&', painted with the hot key */
void sample_hot (WSkinSample *s, int y, int x, const char *group, const char *key,
                 const char *hot_key, const char *text);
/* one frame piece in the colors of @group/@key, recorded for the CHAR entry @char_key */
void sample_piece (WSkinSample *s, int y, int x, const char *group, const char *key,
                   const char *char_group, const char *char_key, mc_tty_char_t ch);
void sample_fill (WSkinSample *s, int y, int x, int lines, int cols, const char *group,
                  const char *key);
/* a horizontal line of the light frame set */
void sample_hline (WSkinSample *s, int y, int x, int cols, const char *group, const char *key);
void sample_box (WSkinSample *s, int y, int x, int lines, int cols, const char *group,
                 const char *key, gboolean single);
/* the character of a CHAR entry: the value of the skin or the built-in */
const char *sample_char (WSkinSample *s, const char *group, const char *key);

/* the mock-up of a section, by its group and its first key; NULL when there is none */
skinsample_draw_fn skinsample_lookup (const char *group, const char *first_key);

/*** inline functions ****************************************************************************/

#endif
