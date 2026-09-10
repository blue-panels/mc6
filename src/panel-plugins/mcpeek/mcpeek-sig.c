/*
   Panel plugin mcpeek for the M-Commander
   signature blobs and type names

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

#include "mcpeek-sig.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* ECMA-335 II.23.1.16 */
#define ET_VOID        0x01
#define ET_BOOLEAN     0x02
#define ET_CHAR        0x03
#define ET_I1          0x04
#define ET_U1          0x05
#define ET_I2          0x06
#define ET_U2          0x07
#define ET_I4          0x08
#define ET_U4          0x09
#define ET_I8          0x0A
#define ET_U8          0x0B
#define ET_R4          0x0C
#define ET_R8          0x0D
#define ET_STRING      0x0E
#define ET_PTR         0x0F
#define ET_BYREF       0x10
#define ET_VALUETYPE   0x11
#define ET_CLASS       0x12
#define ET_VAR         0x13
#define ET_ARRAY       0x14
#define ET_GENERICINST 0x15
#define ET_TYPEDBYREF  0x16
#define ET_I           0x18
#define ET_U           0x19
#define ET_FNPTR       0x1B
#define ET_OBJECT      0x1C
#define ET_SZARRAY     0x1D
#define ET_MVAR        0x1E
#define ET_CMOD_REQD   0x1F
#define ET_CMOD_OPT    0x20
#define ET_PINNED      0x45

/* the vararg sentinel of a call site signature */
#define SIG_SENTINEL 0x41

/* types nest through arrays, pointers, generics and TypeSpecs; a chain
   longer than this is a broken blob, not a type */
#define SIG_MAX_DEPTH 64

/* what the runtime allows an array */
#define SIG_MAX_RANK 32

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const guint8 *p;
    const guint8 *end;
    int depth;
} sig_cursor_t;

/*** forward declarations (file scope functions) *************************************************/

static void sig_type (const mcpeek_meta_t *meta, sig_cursor_t *c, GString *out);
static char *type_name_at (const mcpeek_meta_t *meta, int table, guint32 rid, gboolean qualified,
                           int depth);

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
sig_init (sig_cursor_t *c, const guint8 *blob, guint32 len, int depth)
{
    c->p = blob;
    c->end = blob + len;
    c->depth = depth;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sig_eof (const sig_cursor_t *c)
{
    return c->p >= c->end;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sig_u8 (sig_cursor_t *c, guint8 *out)
{
    if (c->p >= c->end)
        return FALSE;

    *out = *c->p++;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
sig_compressed (sig_cursor_t *c, guint32 *out)
{
    guint8 b;

    if (!sig_u8 (c, &b))
        return FALSE;

    if ((b & 0x80) == 0)
        *out = b;
    else if ((b & 0xC0) == 0x80)
    {
        guint8 b2;

        if (!sig_u8 (c, &b2))
            return FALSE;
        *out = ((guint32) (b & 0x3F) << 8) | b2;
    }
    else if ((b & 0xE0) == 0xC0)
    {
        guint8 b2, b3, b4;

        if (!sig_u8 (c, &b2) || !sig_u8 (c, &b3) || !sig_u8 (c, &b4))
            return FALSE;
        *out = ((guint32) (b & 0x1F) << 24) | ((guint32) b2 << 16) | ((guint32) b3 << 8) | b4;
    }
    else
        return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The short C# spelling where there is one; it is what a reader expects to
   see in a member list. */
static const char *
sig_primitive (guint8 et)
{
    switch (et)
    {
    case ET_VOID:
        return "void";
    case ET_BOOLEAN:
        return "bool";
    case ET_CHAR:
        return "char";
    case ET_I1:
        return "sbyte";
    case ET_U1:
        return "byte";
    case ET_I2:
        return "short";
    case ET_U2:
        return "ushort";
    case ET_I4:
        return "int";
    case ET_U4:
        return "uint";
    case ET_I8:
        return "long";
    case ET_U8:
        return "ulong";
    case ET_R4:
        return "float";
    case ET_R8:
        return "double";
    case ET_STRING:
        return "string";
    case ET_OBJECT:
        return "object";
    case ET_TYPEDBYREF:
        return "TypedReference";
    case ET_I:
        return "IntPtr";
    case ET_U:
        return "UIntPtr";
    default:
        return NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* TypeDefOrRefOrSpecEncoded: a compressed integer whose low two bits pick the
   table. */
static void
sig_type_token (const mcpeek_meta_t *meta, sig_cursor_t *c, GString *out)
{
    static const int tag_table[4] = {
        MCPEEK_T_TYPEDEF,
        MCPEEK_T_TYPEREF,
        MCPEEK_T_TYPESPEC,
        -1,
    };
    guint32 v;
    int table;
    char *name;

    if (!sig_compressed (c, &v))
    {
        g_string_append_c (out, '?');
        return;
    }

    table = tag_table[v & 3];
    if (table < 0)
    {
        g_string_append_c (out, '?');
        return;
    }

    name = type_name_at (meta, table, v >> 2, FALSE, c->depth);
    g_string_append (out, name);
    g_free (name);
}

/* --------------------------------------------------------------------------------------------- */

static void
sig_type_body (const mcpeek_meta_t *meta, sig_cursor_t *c, GString *out)
{
    guint8 et;
    const char *prim;

    if (!sig_u8 (c, &et))
    {
        g_string_append_c (out, '?');
        return;
    }

    /* custom modifiers decorate the type that follows; the reader does not
       need to see them */
    while (et == ET_CMOD_REQD || et == ET_CMOD_OPT || et == ET_PINNED)
    {
        if (et != ET_PINNED)
        {
            guint32 ignored;

            if (!sig_compressed (c, &ignored))
                break;
        }
        if (!sig_u8 (c, &et))
        {
            g_string_append_c (out, '?');
            return;
        }
    }

    prim = sig_primitive (et);
    if (prim != NULL)
    {
        g_string_append (out, prim);
        return;
    }

    switch (et)
    {
    case ET_CLASS:
    case ET_VALUETYPE:
        sig_type_token (meta, c, out);
        break;

    case ET_SZARRAY:
        sig_type (meta, c, out);
        g_string_append (out, "[]");
        break;

    case ET_PTR:
        sig_type (meta, c, out);
        g_string_append_c (out, '*');
        break;

    case ET_BYREF:
        g_string_append (out, "ref ");
        sig_type (meta, c, out);
        break;

    case ET_ARRAY:
    {
        guint32 rank, i;

        sig_type (meta, c, out);
        if (!sig_compressed (c, &rank))
            rank = 1;
        rank = MIN (rank, SIG_MAX_RANK);
        g_string_append_c (out, '[');
        for (i = 1; i < rank; i++)
            g_string_append_c (out, ',');
        g_string_append_c (out, ']');

        /* sizes and lower bounds: skipped, but must be consumed */
        {
            guint32 n, j, v;

            if (sig_compressed (c, &n))
                for (j = 0; j < n && sig_compressed (c, &v); j++)
                    ;
            if (sig_compressed (c, &n))
                for (j = 0; j < n && sig_compressed (c, &v); j++)
                    ;
        }
        break;
    }

    case ET_GENERICINST:
    {
        guint32 argc, i;

        sig_type (meta, c, out);
        if (!sig_compressed (c, &argc))
            argc = 0;
        g_string_append_c (out, '<');
        for (i = 0; i < argc && !sig_eof (c); i++)
        {
            if (i != 0)
                g_string_append (out, ", ");
            sig_type (meta, c, out);
        }
        g_string_append_c (out, '>');
        break;
    }

    case ET_VAR:
    case ET_MVAR:
    {
        guint32 n;

        if (!sig_compressed (c, &n))
            n = 0;
        g_string_append_printf (out, "%c%u", et == ET_VAR ? 'T' : 'M', n);
        break;
    }

    case ET_FNPTR:
    {
        /* the pointed-to signature is inline and has to be consumed, or the
           rest of the enclosing signature is read at the wrong offset */
        guint8 conv;
        guint32 count, i;
        GString *ret;

        if (!sig_u8 (c, &conv))
        {
            g_string_append_c (out, '?');
            break;
        }
        if ((conv & 0x10) != 0)
        {
            guint32 arity;

            if (!sig_compressed (c, &arity))
            {
                g_string_append_c (out, '?');
                break;
            }
        }
        if (!sig_compressed (c, &count))
        {
            g_string_append_c (out, '?');
            break;
        }

        ret = g_string_new (NULL);
        sig_type (meta, c, ret);

        g_string_append (out, "delegate*<");
        for (i = 0; i < count && !sig_eof (c); i++)
        {
            sig_type (meta, c, out);
            g_string_append (out, ", ");
        }
        g_string_append (out, ret->str);
        g_string_append_c (out, '>');
        g_string_free (ret, TRUE);
        break;
    }

    default:
        g_string_append_c (out, '?');
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
sig_type (const mcpeek_meta_t *meta, sig_cursor_t *c, GString *out)
{
    if (c->depth >= SIG_MAX_DEPTH)
    {
        g_string_append_c (out, '?');
        return;
    }

    c->depth++;
    sig_type_body (meta, c, out);
    c->depth--;
}

/* --------------------------------------------------------------------------------------------- */

/* ", " between the parameters of a list that opened with one character */
static void
sig_sep (GString *out)
{
    if (out->len > 1)
        g_string_append (out, ", ");
}

/* --------------------------------------------------------------------------------------------- */

static char *
type_name_at (const mcpeek_meta_t *meta, int table, guint32 rid, gboolean qualified, int depth)
{
    const char *name = "";
    const char *ns = "";

    switch (table)
    {
    case MCPEEK_T_TYPEDEF:
        name = mcpeek_meta_string (meta, mcpeek_meta_col (meta, table, rid, MCPEEK_TYPEDEF_NAME));
        ns =
            mcpeek_meta_string (meta, mcpeek_meta_col (meta, table, rid, MCPEEK_TYPEDEF_NAMESPACE));
        break;

    case MCPEEK_T_TYPEREF:
        name = mcpeek_meta_string (meta, mcpeek_meta_col (meta, table, rid, MCPEEK_TYPEREF_NAME));
        ns =
            mcpeek_meta_string (meta, mcpeek_meta_col (meta, table, rid, MCPEEK_TYPEREF_NAMESPACE));
        break;

    case MCPEEK_T_TYPESPEC:
    {
        /* a TypeSpec is a signature, not a name */
        sig_cursor_t c;
        guint32 len = 0;
        const guint8 *blob;
        GString *out;

        blob = mcpeek_meta_blob (meta, mcpeek_meta_col (meta, table, rid, 0), &len);
        if (blob == NULL)
            return g_strdup ("?");

        sig_init (&c, blob, len, depth);
        out = g_string_new (NULL);
        sig_type (meta, &c, out);
        return g_string_free (out, FALSE);
    }

    default:
        return g_strdup ("?");
    }

    if (qualified && *ns != '\0')
        return g_strconcat (ns, ".", name, (char *) NULL);

    return g_strdup (name);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_type_name (const mcpeek_meta_t *meta, int table, guint32 rid, gboolean qualified)
{
    return type_name_at (meta, table, rid, qualified, 0);
}

/* --------------------------------------------------------------------------------------------- */

guint32
mcpeek_sig_typespec_base (const mcpeek_meta_t *meta, guint32 rid)
{
    const guint8 *blob;
    guint32 len = 0;
    sig_cursor_t c;
    guint8 et;
    guint32 v;

    blob = mcpeek_meta_blob (meta, mcpeek_meta_col (meta, MCPEEK_T_TYPESPEC, rid, 0), &len);
    if (blob == NULL)
        return 0;

    sig_init (&c, blob, len, 0);
    if (!sig_u8 (&c, &et))
        return 0;

    /* down through what wraps the type, to the type itself */
    for (;;)
    {
        if (et == ET_CMOD_REQD || et == ET_CMOD_OPT)
        {
            if (!sig_compressed (&c, &v))
                return 0;
        }
        else if (et != ET_PINNED && et != ET_SZARRAY && et != ET_PTR && et != ET_BYREF
                 && et != ET_ARRAY && et != ET_GENERICINST)
            break;

        if (!sig_u8 (&c, &et))
            return 0;
    }

    if ((et != ET_CLASS && et != ET_VALUETYPE) || !sig_compressed (&c, &v))
        return 0;

    switch (v & 3)
    {
    case 0:
        return ((guint32) MCPEEK_T_TYPEDEF << 24) | (v >> 2);
    case 1:
        return ((guint32) MCPEEK_T_TYPEREF << 24) | (v >> 2);
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_sig_method (const mcpeek_meta_t *meta, guint32 blob_idx)
{
    const guint8 *blob;
    guint32 len = 0;
    sig_cursor_t c;
    GString *out;
    guint8 conv;
    guint32 count, i;
    GString *ret;

    blob = mcpeek_meta_blob (meta, blob_idx, &len);
    if (blob == NULL)
        return g_strdup ("()");

    sig_init (&c, blob, len, 0);

    if (!sig_u8 (&c, &conv))
        return g_strdup ("()");

    /* generic method: the arity comes before the parameter count */
    if ((conv & 0x10) != 0)
    {
        guint32 arity;

        if (!sig_compressed (&c, &arity))
            return g_strdup ("()");
    }

    if (!sig_compressed (&c, &count))
        return g_strdup ("()");

    ret = g_string_new (NULL);
    sig_type (meta, &c, ret);

    out = g_string_new ("(");
    for (i = 0; i < count && !sig_eof (&c); i++)
    {
        /* a vararg call site: the fixed parameters end here */
        if (*c.p == SIG_SENTINEL)
        {
            c.p++;
            sig_sep (out);
            g_string_append (out, "...");
            if (sig_eof (&c))
                break;
        }
        sig_sep (out);
        sig_type (meta, &c, out);
    }
    g_string_append (out, ") : ");
    g_string_append (out, ret->str);
    g_string_free (ret, TRUE);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_sig_property (const mcpeek_meta_t *meta, guint32 blob_idx, char **params)
{
    const guint8 *blob;
    guint32 len = 0;
    sig_cursor_t c;
    GString *out;
    guint8 conv;
    guint32 count, i;

    if (params != NULL)
        *params = NULL;

    blob = mcpeek_meta_blob (meta, blob_idx, &len);
    if (blob == NULL)
        return g_strdup ("?");

    sig_init (&c, blob, len, 0);

    if (!sig_u8 (&c, &conv) || !sig_compressed (&c, &count))
        return g_strdup ("?");

    out = g_string_new (NULL);
    sig_type (meta, &c, out);

    if (params != NULL && count != 0)
    {
        GString *list = g_string_new (NULL);

        for (i = 0; i < count && !sig_eof (&c); i++)
        {
            if (i != 0)
                g_string_append (list, ", ");
            sig_type (meta, &c, list);
        }
        *params = g_string_free (list, FALSE);
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_sig_locals (const mcpeek_meta_t *meta, guint32 blob_idx)
{
    const guint8 *blob;
    guint32 len = 0;
    sig_cursor_t c;
    GString *out;
    guint8 conv;
    guint32 count, i;

    blob = mcpeek_meta_blob (meta, blob_idx, &len);
    if (blob == NULL)
        return NULL;

    sig_init (&c, blob, len, 0);

    if (!sig_u8 (&c, &conv) || conv != 0x07 || !sig_compressed (&c, &count) || count == 0)
        return NULL;

    out = g_string_new (NULL);
    for (i = 0; i < count && !sig_eof (&c); i++)
    {
        if (i != 0)
            g_string_append (out, ", ");
        g_string_append_printf (out, "[%u] ", i);

        sig_type (meta, &c, out);
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_sig_field (const mcpeek_meta_t *meta, guint32 blob_idx)
{
    const guint8 *blob;
    guint32 len = 0;
    sig_cursor_t c;
    GString *out;
    guint8 conv;

    blob = mcpeek_meta_blob (meta, blob_idx, &len);
    if (blob == NULL)
        return g_strdup ("?");

    sig_init (&c, blob, len, 0);

    if (!sig_u8 (&c, &conv))
        return g_strdup ("?");

    out = g_string_new (NULL);
    sig_type (meta, &c, out);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
