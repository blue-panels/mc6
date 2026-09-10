/** \file mcpeek-find.h
 *  \brief Header: finding uses of a type or member across assemblies
 */

#ifndef MC__MCPEEK_FIND_H
#define MC__MCPEEK_FIND_H

#include "lib/global.h"

#include "mcpeek-meta.h"

/*** structures declarations (and typedefs of structures)*****************************************/

/* What is being looked for, named the way another assembly would have to
   name it: by declaring assembly, namespace, type and member. */
typedef struct
{
    char *asm_name; /* the declaring assembly, without extension */
    char *ns;
    char *type;         /* "Outer/Inner" when nested */
    char *member;       /* NULL when the target is the type itself */
    char *sig;          /* the member's signature as rendered, to tell overloads apart */
    guint32 token;      /* its token inside the declaring assembly */
    guint32 type_token; /* the TypeDef token of its type there */
} mcpeek_target_t;

typedef struct
{
    char *assembly; /* file the use was found in */
    char *type;     /* type declaring the using method */
    char *method;   /* the using method */
    guint32 method_rid;
    guint32 il_offset;
} mcpeek_use_t;

/*** declarations of public functions ************************************************************/

/* Describe the entity @token names in @meta, whose file is @path. */
mcpeek_target_t *mcpeek_find_target (const mcpeek_meta_t *meta, const char *path, guint32 token);
void mcpeek_target_free (mcpeek_target_t *target);

/* Every use of @target in @files, as mcpeek_use_t.  Reads metadata and IL
   only: no decompilation, and nothing is written. */
GPtrArray *mcpeek_find_uses (const mcpeek_target_t *target, char *const *files);
void mcpeek_uses_free (GPtrArray *uses);

#endif
