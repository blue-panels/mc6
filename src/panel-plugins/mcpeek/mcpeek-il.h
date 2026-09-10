/** \file mcpeek-il.h
 *  \brief Header: IL disassembler for mcpeek
 */

#ifndef MC__MCPEEK_IL_H
#define MC__MCPEEK_IL_H

#include "lib/global.h"

#include "mcpeek-meta.h"

/*** declarations of public functions ************************************************************/

/* Disassemble the body of MethodDef row @rid.  Returns the listing, header
   and all, or NULL when the method has no body (abstract, extern, native).
   Caller frees. */
char *mcpeek_il_method (const mcpeek_meta_t *meta, guint32 rid);

/* The name a token stands for, as it should read inside an instruction.
   Caller frees. */
char *mcpeek_il_token_name (const mcpeek_meta_t *meta, guint32 token);

/* Called for every token an instruction carries.  FALSE stops the walk. */
typedef gboolean (*mcpeek_il_token_fn) (guint32 token, guint32 il_offset, gpointer user_data);

/* Walk the body of MethodDef @rid without rendering it, handing every token
   operand to @cb.  Returns the number of instructions walked, or -1 when the
   method has no body.  A body that ends mid-instruction stops the walk. */
gssize mcpeek_il_scan (const mcpeek_meta_t *meta, guint32 rid, mcpeek_il_token_fn cb,
                       gpointer user_data);

#endif
