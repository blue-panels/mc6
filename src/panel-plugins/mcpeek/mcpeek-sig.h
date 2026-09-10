/** \file mcpeek-sig.h
 *  \brief Header: signature blobs and type names for mcpeek
 */

#ifndef MC__MCPEEK_SIG_H
#define MC__MCPEEK_SIG_H

#include "lib/global.h"

#include "mcpeek-meta.h"

/*** declarations of public functions ************************************************************/

/* Full name of a type row: "Namespace.Name", or "Outer/Inner" for a nested
   one.  @table is TypeDef, TypeRef or TypeSpec.  Caller frees. */
char *mcpeek_type_name (const mcpeek_meta_t *meta, int table, guint32 rid, gboolean qualified);

/* The TypeDef or TypeRef token a TypeSpec is built on, through arrays,
   pointers and generic instantiations: List<Foo> gives List`1.  0 when the
   TypeSpec is a generic parameter or a function pointer. */
guint32 mcpeek_sig_typespec_base (const mcpeek_meta_t *meta, guint32 rid);

/* "(int, string) : void" for a MethodDef signature blob.  Caller frees. */
char *mcpeek_sig_method (const mcpeek_meta_t *meta, guint32 blob_idx);

/* The type of a field signature blob.  Caller frees. */
char *mcpeek_sig_field (const mcpeek_meta_t *meta, guint32 blob_idx);

/* The type of a property signature blob, which unlike a field's carries a
   parameter count before the type.  When @params is given it gets the
   parameter types of an indexer, "int, string", or NULL.  Caller frees both. */
char *mcpeek_sig_property (const mcpeek_meta_t *meta, guint32 blob_idx, char **params);

/* The local variables of a StandAloneSig blob, comma separated and numbered
   the way the IL refers to them.  NULL when there are none.  Caller frees. */
char *mcpeek_sig_locals (const mcpeek_meta_t *meta, guint32 blob_idx);

#endif
