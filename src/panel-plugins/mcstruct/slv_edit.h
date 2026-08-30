/** \file slv_edit.h
 *  \brief Header: turn a typed value back into the bytes of a field
 */

#ifndef MC__MCSTRUCT_SLV_EDIT_H
#define MC__MCSTRUCT_SLV_EDIT_H

#include "slv.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* TRUE when the node's bytes can be replaced from text */
gboolean slv_node_editable (const slv_node_t *node);

/* the value in the form the user edits; caller frees */
/* @raw: the bytes of the field, may be NULL; NULL result: the bytes are not text */
char *slv_node_edit_text (const slv_node_t *node, const unsigned char *raw);

/* parse @text into exactly node->size bytes at @out; on failure sets *error (caller frees) */
gboolean slv_node_encode (const slv_node_t *node, const char *text, unsigned char *out,
                          char **error);

#endif
