/** \file slv_reader.h
 *  \brief Header: file reader with an overlay of unsaved byte edits
 */

#ifndef MC__MCSTRUCT_SLV_READER_H
#define MC__MCSTRUCT_SLV_READER_H

#include "slv.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    slv_reader_t reader; /* what the evaluator and the hex strip read through */
    char *path;
    int fd;
    off_t size;
    GArray *changes; /* sorted by offset */
} slv_file_reader_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

slv_file_reader_t *slv_file_reader_open (const char *path, GError **error);
void slv_file_reader_free (slv_file_reader_t *fr);
void slv_file_reader_set_byte (slv_file_reader_t *fr, off_t offset, unsigned char value);
gboolean slv_file_reader_is_changed (const slv_file_reader_t *fr, off_t offset);
guint slv_file_reader_change_count (const slv_file_reader_t *fr);
void slv_file_reader_discard (slv_file_reader_t *fr);
gboolean slv_file_reader_save (slv_file_reader_t *fr, GError **error);

#endif
