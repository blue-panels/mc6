/*
   Panel plugin mcpeek for the M-Commander
   finding uses of a type or member across assemblies

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
 */

#include <config.h>

#include <string.h>

#include "mcpeek-find.h"
#include "mcpeek-il.h"
#include "mcpeek-resolve.h"
#include "mcpeek-sig.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define TOKEN_TABLE(t) ((int) ((t) >> 24))
#define TOKEN_RID(t)   ((t) & 0x00FFFFFFu)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const mcpeek_meta_t *meta;
    GHashTable *wanted; /* the tokens that mean the target, in this assembly */
    const char *path;
    guint32 method_rid;
    GPtrArray *uses;
} scan_ctx_t;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static const char *
assembly_own_name (const mcpeek_meta_t *meta)
{
    return mcpeek_meta_string (meta, mcpeek_meta_col (meta, MCPEEK_T_ASSEMBLY, 1, MCPEEK_ASM_NAME));
}

/* --------------------------------------------------------------------------------------------- */

/* Does assembly @scope hand the type on to @target, directly or through
   another forwarder?  A reference assembly declares almost nothing itself: it
   lists the type as exported and names the assembly that really has it, and a
   compiled caller only ever names the reference assembly.  Without following
   that, a search for anything in the runtime finds next to nothing. */
static gboolean
forwards_to (const char *scope, const char *ns, const char *type, const char *target,
             const char *base_dir, int depth)
{
    char *file;
    mcpeek_image_t *img;
    mcpeek_meta_t meta;
    guint32 i, n;
    gboolean result = FALSE;

    if (depth <= 0)
        return FALSE;

    if (strcmp (scope, target) == 0)
        return TRUE;

    file = mcpeek_resolve_assembly (scope, base_dir);
    if (file == NULL)
    {
        /* not installed here: it cannot be ruled out, and ruling it out would
           silently empty the result */
        return TRUE;
    }

    img = mcpeek_image_open (file, NULL);
    g_free (file);
    if (img == NULL)
        return TRUE;

    if (!mcpeek_meta_init (&meta, img, NULL))
    {
        mcpeek_image_free (img);
        return TRUE;
    }

    n = mcpeek_meta_rows (&meta, MCPEEK_T_EXPORTEDTYPE);
    for (i = 1; i <= n && !result; i++)
    {
        const char *e_name, *e_ns;
        guint32 impl;
        int i_table;
        guint32 i_rid;

        e_name = mcpeek_meta_string (&meta, mcpeek_meta_col (&meta, MCPEEK_T_EXPORTEDTYPE, i, 2));
        if (strcmp (e_name, type) != 0)
            continue;

        e_ns = mcpeek_meta_string (&meta, mcpeek_meta_col (&meta, MCPEEK_T_EXPORTEDTYPE, i, 3));
        if (strcmp (e_ns, ns) != 0)
            continue;

        impl = mcpeek_meta_col (&meta, MCPEEK_T_EXPORTEDTYPE, i, 4);
        if (mcpeek_meta_decode (MCPEEK_CI_IMPLEMENTATION, impl, &i_table, &i_rid)
            && i_table == MCPEEK_T_ASSEMBLYREF)
        {
            const char *next;

            next = mcpeek_meta_string (
                &meta, mcpeek_meta_col (&meta, MCPEEK_T_ASSEMBLYREF, i_rid, MCPEEK_ASMREF_NAME));
            result = forwards_to (next, ns, type, target, base_dir, depth - 1);
        }
    }

    mcpeek_image_free (img);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* The last segment of an "Outer/Inner" name. */
static const char *
simple_name (const char *nested)
{
    const char *slash = strrchr (nested, '/');

    return slash != NULL ? slash + 1 : nested;
}

/* --------------------------------------------------------------------------------------------- */

/* The TypeDef that encloses @rid, or 0. */
static guint32
enclosing_typedef (const mcpeek_meta_t *meta, guint32 rid)
{
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_NESTEDCLASS);
    for (i = 1; i <= n; i++)
        if (mcpeek_meta_col (meta, MCPEEK_T_NESTEDCLASS, i, MCPEEK_NESTED_NESTED) == rid)
            return mcpeek_meta_col (meta, MCPEEK_T_NESTEDCLASS, i, MCPEEK_NESTED_ENCLOSING);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* "Outer/Inner" for a nested TypeDef, and the namespace of the outermost
   type, which is the one that carries it.  Caller frees the name. */
static char *
typedef_nested_name (const mcpeek_meta_t *meta, guint32 rid, const char **ns)
{
    GString *s;
    int guard;

    s = g_string_new (mcpeek_meta_string (
        meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, rid, MCPEEK_TYPEDEF_NAME)));
    *ns = mcpeek_meta_string (
        meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, rid, MCPEEK_TYPEDEF_NAMESPACE));

    for (guard = 0; guard < 64; guard++)
    {
        guint32 outer = enclosing_typedef (meta, rid);

        if (outer == 0)
            break;

        g_string_prepend_c (s, '/');
        g_string_prepend (
            s,
            mcpeek_meta_string (
                meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, outer, MCPEEK_TYPEDEF_NAME)));
        *ns = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, outer, MCPEEK_TYPEDEF_NAMESPACE));
        rid = outer;
    }

    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* The same for a TypeRef, whose enclosing type is its resolution scope.
   @outer is the outermost TypeRef: its scope says where the type lives. */
static char *
typeref_nested_name (const mcpeek_meta_t *meta, guint32 rid, const char **ns, guint32 *outer)
{
    GString *s;
    int guard;

    s = g_string_new (mcpeek_meta_string (
        meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEREF, rid, MCPEEK_TYPEREF_NAME)));
    *ns = mcpeek_meta_string (
        meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEREF, rid, MCPEEK_TYPEREF_NAMESPACE));
    *outer = rid;

    for (guard = 0; guard < 64; guard++)
    {
        guint32 scope;
        int s_table;
        guint32 s_rid;

        scope = mcpeek_meta_col (meta, MCPEEK_T_TYPEREF, rid, MCPEEK_TYPEREF_SCOPE);
        if (!mcpeek_meta_decode (MCPEEK_CI_RESOLUTIONSCOPE, scope, &s_table, &s_rid)
            || s_table != MCPEEK_T_TYPEREF || s_rid == rid)
            break;

        rid = s_rid;
        g_string_prepend_c (s, '/');
        g_string_prepend (
            s,
            mcpeek_meta_string (
                meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEREF, rid, MCPEEK_TYPEREF_NAME)));
        *ns = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEREF, rid, MCPEEK_TYPEREF_NAMESPACE));
        *outer = rid;
    }

    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* The TypeRef rows of @meta that name the target's type.  A TypeRef says
   which assembly it comes from through its resolution scope, so a name that
   happens to be shared by two assemblies does not produce false hits; a
   nested type is matched with its enclosing types, for the same reason. */
static void
collect_type_refs (const mcpeek_meta_t *meta, const mcpeek_target_t *target, const char *base_dir,
                   GHashTable *out)
{
    const char *last = simple_name (target->type);
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_TYPEREF);
    for (i = 1; i <= n; i++)
    {
        const char *name, *ns;
        char *full;
        guint32 outer, scope;
        int s_table;
        guint32 s_rid;
        gboolean same;

        name = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEREF, i, MCPEEK_TYPEREF_NAME));
        if (strcmp (name, last) != 0)
            continue;

        full = typeref_nested_name (meta, i, &ns, &outer);
        same = strcmp (full, target->type) == 0 && strcmp (ns, target->ns) == 0;
        g_free (full);
        if (!same)
            continue;

        scope = mcpeek_meta_col (meta, MCPEEK_T_TYPEREF, outer, MCPEEK_TYPEREF_SCOPE);
        if (mcpeek_meta_decode (MCPEEK_CI_RESOLUTIONSCOPE, scope, &s_table, &s_rid)
            && s_table == MCPEEK_T_ASSEMBLYREF)
        {
            const char *ref;

            ref = mcpeek_meta_string (
                meta, mcpeek_meta_col (meta, MCPEEK_T_ASSEMBLYREF, s_rid, MCPEEK_ASMREF_NAME));
            if (!forwards_to (ref, target->ns, target->type, target->asm_name, base_dir, 4))
                continue;
        }

        g_hash_table_add (out, GUINT_TO_POINTER (0x01000000u | i));
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Which tokens of this assembly denote the target: its own, when this is the
   assembly that declares it, and the references that reach it from outside. */
static GHashTable *
wanted_tokens (const mcpeek_meta_t *meta, const mcpeek_target_t *target, const char *base_dir)
{
    GHashTable *wanted;
    GHashTable *type_refs;
    gboolean is_home;
    guint32 i, n;

    wanted = g_hash_table_new (g_direct_hash, g_direct_equal);
    is_home = strcmp (assembly_own_name (meta), target->asm_name) == 0;

    if (is_home)
        g_hash_table_add (wanted, GUINT_TO_POINTER (target->token));

    /* the type is named by its TypeRefs from outside and, at home, by its
       own TypeDef; either way a generic instantiation of it is a TypeSpec
       built on that */
    type_refs = g_hash_table_new (g_direct_hash, g_direct_equal);
    collect_type_refs (meta, target, base_dir, type_refs);
    if (is_home)
        g_hash_table_add (type_refs, GUINT_TO_POINTER (target->type_token));

    if (target->member == NULL)
    {
        GHashTableIter iter;
        gpointer key;

        g_hash_table_iter_init (&iter, type_refs);
        while (g_hash_table_iter_next (&iter, &key, NULL))
            g_hash_table_add (wanted, key);

        n = mcpeek_meta_rows (meta, MCPEEK_T_TYPESPEC);
        for (i = 1; i <= n; i++)
        {
            guint32 base = mcpeek_sig_typespec_base (meta, i);

            if (base != 0 && g_hash_table_contains (type_refs, GUINT_TO_POINTER (base)))
                g_hash_table_add (wanted, GUINT_TO_POINTER (0x1B000000u | i));
        }

        g_hash_table_destroy (type_refs);
        return wanted;
    }

    /* a member is reached through a MemberRef whose parent names the type,
       directly or as an instantiation, and whose name is the member's */
    n = mcpeek_meta_rows (meta, MCPEEK_T_MEMBERREF);
    for (i = 1; i <= n; i++)
    {
        guint32 parent;
        int p_table;
        guint32 p_rid;
        const char *name;

        name = mcpeek_meta_string (meta, mcpeek_meta_col (meta, MCPEEK_T_MEMBERREF, i, 1));
        if (strcmp (name, target->member) != 0)
            continue;

        /* the same name with another signature is another member: an
           overload, or a field beside a method */
        {
            guint32 sig_idx = mcpeek_meta_col (meta, MCPEEK_T_MEMBERREF, i, 2);
            char *sig;
            gboolean same;

            if (TOKEN_TABLE (target->token) == MCPEEK_T_FIELD)
                sig = mcpeek_sig_field (meta, sig_idx);
            else
                sig = mcpeek_sig_method (meta, sig_idx);
            same = target->sig != NULL && strcmp (sig, target->sig) == 0;
            g_free (sig);
            if (!same)
                continue;
        }

        parent = mcpeek_meta_col (meta, MCPEEK_T_MEMBERREF, i, 0);
        if (!mcpeek_meta_decode (MCPEEK_CI_MEMBERREFPARENT, parent, &p_table, &p_rid))
            continue;

        if (p_table == MCPEEK_T_TYPESPEC)
            parent = mcpeek_sig_typespec_base (meta, p_rid);
        else
            parent = ((guint32) p_table << 24) | p_rid;

        if (parent != 0 && g_hash_table_contains (type_refs, GUINT_TO_POINTER (parent)))
            g_hash_table_add (wanted, GUINT_TO_POINTER (0x0A000000u | i));
    }

    g_hash_table_destroy (type_refs);

    return wanted;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
on_token (guint32 token, guint32 il_offset, gpointer user_data)
{
    scan_ctx_t *ctx = (scan_ctx_t *) user_data;
    mcpeek_use_t *use;
    guint32 owner;
    char *sig;

    /* a generic instantiation stands for the method it instantiates */
    if (TOKEN_TABLE (token) == 0x2B)
    {
        guint32 method;
        int m_table;
        guint32 m_rid;

        method = mcpeek_meta_col (ctx->meta, 0x2B, TOKEN_RID (token), 0);
        if (mcpeek_meta_decode (MCPEEK_CI_METHODDEFORREF, method, &m_table, &m_rid))
            token = ((guint32) m_table << 24) | m_rid;
    }

    if (!g_hash_table_contains (ctx->wanted, GUINT_TO_POINTER (token)))
        return TRUE;

    use = g_new0 (mcpeek_use_t, 1);
    use->assembly = g_strdup (ctx->path);
    use->il_offset = il_offset;
    use->method_rid = ctx->method_rid;

    owner = mcpeek_meta_owner_type (ctx->meta, MCPEEK_T_METHODDEF, MCPEEK_TYPEDEF_METHODLIST,
                                    ctx->method_rid);
    use->type =
        owner != 0 ? mcpeek_type_name (ctx->meta, MCPEEK_T_TYPEDEF, owner, TRUE) : g_strdup ("?");

    sig = mcpeek_sig_method (
        ctx->meta,
        mcpeek_meta_col (ctx->meta, MCPEEK_T_METHODDEF, ctx->method_rid, MCPEEK_METHOD_SIGNATURE));
    use->method =
        g_strconcat (mcpeek_meta_string (ctx->meta,
                                         mcpeek_meta_col (ctx->meta, MCPEEK_T_METHODDEF,
                                                          ctx->method_rid, MCPEEK_METHOD_NAME)),
                     sig, (char *) NULL);
    g_free (sig);

    g_ptr_array_add (ctx->uses, use);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
scan_assembly (const char *path, const mcpeek_target_t *target, GPtrArray *uses)
{
    mcpeek_image_t *img;
    mcpeek_meta_t meta;
    scan_ctx_t ctx;
    guint32 i, n;

    img = mcpeek_image_open (path, NULL);
    if (img == NULL)
        return;

    if (!mcpeek_meta_init (&meta, img, NULL))
    {
        mcpeek_image_free (img);
        return;
    }

    ctx.meta = &meta;
    ctx.path = path;
    ctx.uses = uses;
    {
        char *base_dir = g_path_get_dirname (path);

        ctx.wanted = wanted_tokens (&meta, target, base_dir);
        g_free (base_dir);
    }

    /* nothing here names the target, so nothing here can use it */
    if (g_hash_table_size (ctx.wanted) != 0)
    {
        n = mcpeek_meta_rows (&meta, MCPEEK_T_METHODDEF);
        for (i = 1; i <= n; i++)
        {
            ctx.method_rid = i;
            (void) mcpeek_il_scan (&meta, i, on_token, &ctx);
        }
    }

    g_hash_table_destroy (ctx.wanted);
    mcpeek_image_free (img);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mcpeek_target_t *
mcpeek_find_target (const mcpeek_meta_t *meta, const char *path, guint32 token)
{
    mcpeek_target_t *t;
    guint32 rid = TOKEN_RID (token);
    guint32 type_rid = 0;

    (void) path;

    switch (TOKEN_TABLE (token))
    {
    case MCPEEK_T_TYPEDEF:
        type_rid = rid;
        break;
    case MCPEEK_T_METHODDEF:
        type_rid =
            mcpeek_meta_owner_type (meta, MCPEEK_T_METHODDEF, MCPEEK_TYPEDEF_METHODLIST, rid);
        break;
    case MCPEEK_T_FIELD:
        type_rid = mcpeek_meta_owner_type (meta, MCPEEK_T_FIELD, MCPEEK_TYPEDEF_FIELDLIST, rid);
        break;
    default:
        return NULL;
    }

    if (type_rid == 0)
        return NULL;

    t = g_new0 (mcpeek_target_t, 1);
    t->token = token;
    t->type_token = 0x02000000u | type_rid;
    t->asm_name = g_strdup (assembly_own_name (meta));
    {
        const char *ns;

        t->type = typedef_nested_name (meta, type_rid, &ns);
        t->ns = g_strdup (ns);
    }

    if (TOKEN_TABLE (token) == MCPEEK_T_METHODDEF)
    {
        t->member = g_strdup (mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, rid, MCPEEK_METHOD_NAME)));
        t->sig = mcpeek_sig_method (
            meta, mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, rid, MCPEEK_METHOD_SIGNATURE));
    }
    else if (TOKEN_TABLE (token) == MCPEEK_T_FIELD)
    {
        t->member = g_strdup (mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_FIELD, rid, MCPEEK_FIELD_NAME)));
        t->sig = mcpeek_sig_field (
            meta, mcpeek_meta_col (meta, MCPEEK_T_FIELD, rid, MCPEEK_FIELD_SIGNATURE));
    }

    return t;
}

/* --------------------------------------------------------------------------------------------- */

void
mcpeek_target_free (mcpeek_target_t *target)
{
    if (target == NULL)
        return;

    g_free (target->asm_name);
    g_free (target->ns);
    g_free (target->type);
    g_free (target->member);
    g_free (target->sig);
    g_free (target);
}

/* --------------------------------------------------------------------------------------------- */

GPtrArray *
mcpeek_find_uses (const mcpeek_target_t *target, char *const *files)
{
    GPtrArray *uses;
    int i;

    uses = g_ptr_array_new ();

    for (i = 0; files != NULL && files[i] != NULL; i++)
        scan_assembly (files[i], target, uses);

    return uses;
}

/* --------------------------------------------------------------------------------------------- */

void
mcpeek_uses_free (GPtrArray *uses)
{
    guint i;

    if (uses == NULL)
        return;

    for (i = 0; i < uses->len; i++)
    {
        mcpeek_use_t *u = g_ptr_array_index (uses, i);

        g_free (u->assembly);
        g_free (u->type);
        g_free (u->method);
        g_free (u);
    }

    g_ptr_array_free (uses, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
