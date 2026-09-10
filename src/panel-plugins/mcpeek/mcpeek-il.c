/*
   Panel plugin mcpeek for the M-Commander
   IL disassembler

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

#include "mcpeek-il.h"
#include "mcpeek-sig.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define TOKEN_TABLE(t) ((int) ((t) >> 24))
#define TOKEN_RID(t)   ((t) & 0x00FFFFFFu)

/*** file scope type declarations ****************************************************************/

typedef enum
{
    OP_NONE = 0,
    OP_I1, /* signed byte */
    OP_U1, /* unsigned byte */
    OP_U2, /* unsigned short: local or argument number */
    OP_I4,
    OP_I8,
    OP_R4,
    OP_R8,
    OP_TOKEN,
    OP_STRING, /* a #US token */
    OP_BR1,    /* one-byte relative branch */
    OP_BR4,    /* four-byte relative branch */
    OP_SWITCH
} mcpeek_operand_t;

typedef struct
{
    const char *name;
    mcpeek_operand_t operand;
} mcpeek_opcode_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* ECMA-335 III.  A NULL name is a byte with no opcode assigned to it. */
static const mcpeek_opcode_t opcodes[256] = {
    [0x00] = { "nop", OP_NONE },
    [0x01] = { "break", OP_NONE },
    [0x02] = { "ldarg.0", OP_NONE },
    [0x03] = { "ldarg.1", OP_NONE },
    [0x04] = { "ldarg.2", OP_NONE },
    [0x05] = { "ldarg.3", OP_NONE },
    [0x06] = { "ldloc.0", OP_NONE },
    [0x07] = { "ldloc.1", OP_NONE },
    [0x08] = { "ldloc.2", OP_NONE },
    [0x09] = { "ldloc.3", OP_NONE },
    [0x0A] = { "stloc.0", OP_NONE },
    [0x0B] = { "stloc.1", OP_NONE },
    [0x0C] = { "stloc.2", OP_NONE },
    [0x0D] = { "stloc.3", OP_NONE },
    [0x0E] = { "ldarg.s", OP_U1 },
    [0x0F] = { "ldarga.s", OP_U1 },
    [0x10] = { "starg.s", OP_U1 },
    [0x11] = { "ldloc.s", OP_U1 },
    [0x12] = { "ldloca.s", OP_U1 },
    [0x13] = { "stloc.s", OP_U1 },
    [0x14] = { "ldnull", OP_NONE },
    [0x15] = { "ldc.i4.m1", OP_NONE },
    [0x16] = { "ldc.i4.0", OP_NONE },
    [0x17] = { "ldc.i4.1", OP_NONE },
    [0x18] = { "ldc.i4.2", OP_NONE },
    [0x19] = { "ldc.i4.3", OP_NONE },
    [0x1A] = { "ldc.i4.4", OP_NONE },
    [0x1B] = { "ldc.i4.5", OP_NONE },
    [0x1C] = { "ldc.i4.6", OP_NONE },
    [0x1D] = { "ldc.i4.7", OP_NONE },
    [0x1E] = { "ldc.i4.8", OP_NONE },
    [0x1F] = { "ldc.i4.s", OP_I1 },
    [0x20] = { "ldc.i4", OP_I4 },
    [0x21] = { "ldc.i8", OP_I8 },
    [0x22] = { "ldc.r4", OP_R4 },
    [0x23] = { "ldc.r8", OP_R8 },
    [0x25] = { "dup", OP_NONE },
    [0x26] = { "pop", OP_NONE },
    [0x27] = { "jmp", OP_TOKEN },
    [0x28] = { "call", OP_TOKEN },
    [0x29] = { "calli", OP_TOKEN },
    [0x2A] = { "ret", OP_NONE },
    [0x2B] = { "br.s", OP_BR1 },
    [0x2C] = { "brfalse.s", OP_BR1 },
    [0x2D] = { "brtrue.s", OP_BR1 },
    [0x2E] = { "beq.s", OP_BR1 },
    [0x2F] = { "bge.s", OP_BR1 },
    [0x30] = { "bgt.s", OP_BR1 },
    [0x31] = { "ble.s", OP_BR1 },
    [0x32] = { "blt.s", OP_BR1 },
    [0x33] = { "bne.un.s", OP_BR1 },
    [0x34] = { "bge.un.s", OP_BR1 },
    [0x35] = { "bgt.un.s", OP_BR1 },
    [0x36] = { "ble.un.s", OP_BR1 },
    [0x37] = { "blt.un.s", OP_BR1 },
    [0x38] = { "br", OP_BR4 },
    [0x39] = { "brfalse", OP_BR4 },
    [0x3A] = { "brtrue", OP_BR4 },
    [0x3B] = { "beq", OP_BR4 },
    [0x3C] = { "bge", OP_BR4 },
    [0x3D] = { "bgt", OP_BR4 },
    [0x3E] = { "ble", OP_BR4 },
    [0x3F] = { "blt", OP_BR4 },
    [0x40] = { "bne.un", OP_BR4 },
    [0x41] = { "bge.un", OP_BR4 },
    [0x42] = { "bgt.un", OP_BR4 },
    [0x43] = { "ble.un", OP_BR4 },
    [0x44] = { "blt.un", OP_BR4 },
    [0x45] = { "switch", OP_SWITCH },
    [0x46] = { "ldind.i1", OP_NONE },
    [0x47] = { "ldind.u1", OP_NONE },
    [0x48] = { "ldind.i2", OP_NONE },
    [0x49] = { "ldind.u2", OP_NONE },
    [0x4A] = { "ldind.i4", OP_NONE },
    [0x4B] = { "ldind.u4", OP_NONE },
    [0x4C] = { "ldind.i8", OP_NONE },
    [0x4D] = { "ldind.i", OP_NONE },
    [0x4E] = { "ldind.r4", OP_NONE },
    [0x4F] = { "ldind.r8", OP_NONE },
    [0x50] = { "ldind.ref", OP_NONE },
    [0x51] = { "stind.ref", OP_NONE },
    [0x52] = { "stind.i1", OP_NONE },
    [0x53] = { "stind.i2", OP_NONE },
    [0x54] = { "stind.i4", OP_NONE },
    [0x55] = { "stind.i8", OP_NONE },
    [0x56] = { "stind.r4", OP_NONE },
    [0x57] = { "stind.r8", OP_NONE },
    [0x58] = { "add", OP_NONE },
    [0x59] = { "sub", OP_NONE },
    [0x5A] = { "mul", OP_NONE },
    [0x5B] = { "div", OP_NONE },
    [0x5C] = { "div.un", OP_NONE },
    [0x5D] = { "rem", OP_NONE },
    [0x5E] = { "rem.un", OP_NONE },
    [0x5F] = { "and", OP_NONE },
    [0x60] = { "or", OP_NONE },
    [0x61] = { "xor", OP_NONE },
    [0x62] = { "shl", OP_NONE },
    [0x63] = { "shr", OP_NONE },
    [0x64] = { "shr.un", OP_NONE },
    [0x65] = { "neg", OP_NONE },
    [0x66] = { "not", OP_NONE },
    [0x67] = { "conv.i1", OP_NONE },
    [0x68] = { "conv.i2", OP_NONE },
    [0x69] = { "conv.i4", OP_NONE },
    [0x6A] = { "conv.i8", OP_NONE },
    [0x6B] = { "conv.r4", OP_NONE },
    [0x6C] = { "conv.r8", OP_NONE },
    [0x6D] = { "conv.u4", OP_NONE },
    [0x6E] = { "conv.u8", OP_NONE },
    [0x6F] = { "callvirt", OP_TOKEN },
    [0x70] = { "cpobj", OP_TOKEN },
    [0x71] = { "ldobj", OP_TOKEN },
    [0x72] = { "ldstr", OP_STRING },
    [0x73] = { "newobj", OP_TOKEN },
    [0x74] = { "castclass", OP_TOKEN },
    [0x75] = { "isinst", OP_TOKEN },
    [0x76] = { "conv.r.un", OP_NONE },
    [0x79] = { "unbox", OP_TOKEN },
    [0x7A] = { "throw", OP_NONE },
    [0x7B] = { "ldfld", OP_TOKEN },
    [0x7C] = { "ldflda", OP_TOKEN },
    [0x7D] = { "stfld", OP_TOKEN },
    [0x7E] = { "ldsfld", OP_TOKEN },
    [0x7F] = { "ldsflda", OP_TOKEN },
    [0x80] = { "stsfld", OP_TOKEN },
    [0x81] = { "stobj", OP_TOKEN },
    [0x82] = { "conv.ovf.i1.un", OP_NONE },
    [0x83] = { "conv.ovf.i2.un", OP_NONE },
    [0x84] = { "conv.ovf.i4.un", OP_NONE },
    [0x85] = { "conv.ovf.i8.un", OP_NONE },
    [0x86] = { "conv.ovf.u1.un", OP_NONE },
    [0x87] = { "conv.ovf.u2.un", OP_NONE },
    [0x88] = { "conv.ovf.u4.un", OP_NONE },
    [0x89] = { "conv.ovf.u8.un", OP_NONE },
    [0x8A] = { "conv.ovf.i.un", OP_NONE },
    [0x8B] = { "conv.ovf.u.un", OP_NONE },
    [0x8C] = { "box", OP_TOKEN },
    [0x8D] = { "newarr", OP_TOKEN },
    [0x8E] = { "ldlen", OP_NONE },
    [0x8F] = { "ldelema", OP_TOKEN },
    [0x90] = { "ldelem.i1", OP_NONE },
    [0x91] = { "ldelem.u1", OP_NONE },
    [0x92] = { "ldelem.i2", OP_NONE },
    [0x93] = { "ldelem.u2", OP_NONE },
    [0x94] = { "ldelem.i4", OP_NONE },
    [0x95] = { "ldelem.u4", OP_NONE },
    [0x96] = { "ldelem.i8", OP_NONE },
    [0x97] = { "ldelem.i", OP_NONE },
    [0x98] = { "ldelem.r4", OP_NONE },
    [0x99] = { "ldelem.r8", OP_NONE },
    [0x9A] = { "ldelem.ref", OP_NONE },
    [0x9B] = { "stelem.i", OP_NONE },
    [0x9C] = { "stelem.i1", OP_NONE },
    [0x9D] = { "stelem.i2", OP_NONE },
    [0x9E] = { "stelem.i4", OP_NONE },
    [0x9F] = { "stelem.i8", OP_NONE },
    [0xA0] = { "stelem.r4", OP_NONE },
    [0xA1] = { "stelem.r8", OP_NONE },
    [0xA2] = { "stelem.ref", OP_NONE },
    [0xA3] = { "ldelem", OP_TOKEN },
    [0xA4] = { "stelem", OP_TOKEN },
    [0xA5] = { "unbox.any", OP_TOKEN },
    [0xB3] = { "conv.ovf.i1", OP_NONE },
    [0xB4] = { "conv.ovf.u1", OP_NONE },
    [0xB5] = { "conv.ovf.i2", OP_NONE },
    [0xB6] = { "conv.ovf.u2", OP_NONE },
    [0xB7] = { "conv.ovf.i4", OP_NONE },
    [0xB8] = { "conv.ovf.u4", OP_NONE },
    [0xB9] = { "conv.ovf.i8", OP_NONE },
    [0xBA] = { "conv.ovf.u8", OP_NONE },
    [0xC2] = { "refanyval", OP_TOKEN },
    [0xC3] = { "ckfinite", OP_NONE },
    [0xC6] = { "mkrefany", OP_TOKEN },
    [0xD0] = { "ldtoken", OP_TOKEN },
    [0xD1] = { "conv.u2", OP_NONE },
    [0xD2] = { "conv.u1", OP_NONE },
    [0xD3] = { "conv.i", OP_NONE },
    [0xD4] = { "conv.ovf.i", OP_NONE },
    [0xD5] = { "conv.ovf.u", OP_NONE },
    [0xD6] = { "add.ovf", OP_NONE },
    [0xD7] = { "add.ovf.un", OP_NONE },
    [0xD8] = { "mul.ovf", OP_NONE },
    [0xD9] = { "mul.ovf.un", OP_NONE },
    [0xDA] = { "sub.ovf", OP_NONE },
    [0xDB] = { "sub.ovf.un", OP_NONE },
    [0xDC] = { "endfinally", OP_NONE },
    [0xDD] = { "leave", OP_BR4 },
    [0xDE] = { "leave.s", OP_BR1 },
    [0xDF] = { "stind.i", OP_NONE },
    [0xE0] = { "conv.u", OP_NONE },
};

/* The 0xFE escape range. */
static const mcpeek_opcode_t opcodes_fe[32] = {
    [0x00] = { "arglist", OP_NONE },       [0x01] = { "ceq", OP_NONE },
    [0x02] = { "cgt", OP_NONE },           [0x03] = { "cgt.un", OP_NONE },
    [0x04] = { "clt", OP_NONE },           [0x05] = { "clt.un", OP_NONE },
    [0x06] = { "ldftn", OP_TOKEN },        [0x07] = { "ldvirtftn", OP_TOKEN },
    [0x09] = { "ldarg", OP_U2 },           [0x0A] = { "ldarga", OP_U2 },
    [0x0B] = { "starg", OP_U2 },           [0x0C] = { "ldloc", OP_U2 },
    [0x0D] = { "ldloca", OP_U2 },          [0x0E] = { "stloc", OP_U2 },
    [0x0F] = { "localloc", OP_NONE },      [0x11] = { "endfilter", OP_NONE },
    [0x12] = { "unaligned.", OP_U1 },      [0x13] = { "volatile.", OP_NONE },
    [0x14] = { "tail.", OP_NONE },         [0x15] = { "initobj", OP_TOKEN },
    [0x16] = { "constrained.", OP_TOKEN }, [0x17] = { "cpblk", OP_NONE },
    [0x18] = { "initblk", OP_NONE },       [0x19] = { "no.", OP_U1 },
    [0x1A] = { "rethrow", OP_NONE },       [0x1C] = { "sizeof", OP_TOKEN },
    [0x1D] = { "refanytype", OP_NONE },    [0x1E] = { "readonly.", OP_NONE },
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static guint16
rd16 (const guint8 *p)
{
    guint16 v;

    memcpy (&v, p, 2);
    return GUINT16_FROM_LE (v);
}

/* --------------------------------------------------------------------------------------------- */

static guint32
rd32 (const guint8 *p)
{
    guint32 v;

    memcpy (&v, p, 4);
    return GUINT32_FROM_LE (v);
}

/* --------------------------------------------------------------------------------------------- */

/* A #US entry is UTF-16 with a trailing flag byte. */
static char *
user_string (const mcpeek_meta_t *meta, guint32 offset)
{
    const mcpeek_stream_t *us = &meta->img->us;
    guint32 len, used;
    const guint8 *p;
    char *utf8;
    GString *out;
    gsize i;

    if (us->data == NULL || offset >= us->size)
        return g_strdup ("\"?\"");

    p = us->data + offset;
    {
        guint8 b = p[0];

        if ((b & 0x80) == 0)
        {
            len = b;
            used = 1;
        }
        else if ((b & 0xC0) == 0x80)
        {
            if (offset + 2 > us->size)
                return g_strdup ("\"?\"");
            len = ((guint32) (b & 0x3F) << 8) | p[1];
            used = 2;
        }
        else
        {
            if (offset + 4 > us->size)
                return g_strdup ("\"?\"");
            len = ((guint32) (b & 0x1F) << 24) | ((guint32) p[1] << 16) | ((guint32) p[2] << 8)
                | p[3];
            used = 4;
        }
    }

    if (len == 0 || len > us->size - offset - used)
        return g_strdup ("\"\"");

    /* the last byte says whether any character needs more than a byte; the
       characters themselves are the bytes before it */
    {
        glong n = (glong) ((len - 1) / 2);
        gunichar2 *units;
        glong k;

        /* little-endian bytes at any alignment, made into units */
        units = g_new (gunichar2, n + 1);
        for (k = 0; k < n; k++)
            units[k] = (gunichar2) (p[used + 2 * k] | ((guint16) p[used + 2 * k + 1] << 8));
        utf8 = g_utf16_to_utf8 (units, n, NULL, NULL, NULL);
        g_free (units);
    }
    if (utf8 == NULL)
        return g_strdup ("\"?\"");

    /* keep the listing one line per instruction */
    out = g_string_new ("\"");
    for (i = 0; utf8[i] != '\0'; i++)
        switch (utf8[i])
        {
        case '\n':
            g_string_append (out, "\\n");
            break;
        case '\r':
            g_string_append (out, "\\r");
            break;
        case '\t':
            g_string_append (out, "\\t");
            break;
        case '"':
            g_string_append (out, "\\\"");
            break;
        case '\\':
            g_string_append (out, "\\\\");
            break;
        default:
            g_string_append_c (out, utf8[i]);
            break;
        }
    g_string_append_c (out, '"');
    g_free (utf8);

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* How many bytes of operand an instruction carries.  The switch table is
   variable, so it is measured here rather than assumed. */
static guint32
operand_size (const mcpeek_opcode_t *op, const guint8 *code, guint32 off, guint32 code_size)
{
    switch (op->operand)
    {
    case OP_NONE:
        return 0;
    case OP_I1:
    case OP_U1:
    case OP_BR1:
        return 1;
    case OP_U2:
        return 2;
    case OP_I4:
    case OP_R4:
    case OP_TOKEN:
    case OP_STRING:
    case OP_BR4:
        return 4;
    case OP_I8:
    case OP_R8:
        return 8;
    case OP_SWITCH:
    {
        guint32 n;

        if (off + 4 > code_size)
            return 4;
        n = rd32 (code + off);
        /* a count the body cannot hold: a size past the end stops the walk
           instead of wrapping it around */
        if (n > (code_size - off - 4) / 4)
            return code_size - off + 1;
        return 4 + n * 4;
    }
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The body of a method, or NULL: the two headers differ in where the code
   starts and in what else they carry. */
static const guint8 *
method_body (const mcpeek_meta_t *meta, guint32 rid, guint32 *code_size, guint32 *max_stack,
             guint32 *locals_tok)
{
    guint32 rva;
    const guint8 *p;

    *code_size = 0;
    *max_stack = 8;
    *locals_tok = 0;

    rva = mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, rid, MCPEEK_METHOD_RVA);
    if (rva == 0)
        return NULL;

    p = mcpeek_rva_ptr (meta->img, rva, 1);
    if (p == NULL)
        return NULL;

    if ((p[0] & 3) == 2)
    {
        *code_size = p[0] >> 2;
        return mcpeek_rva_ptr (meta->img, rva + 1, *code_size);
    }

    p = mcpeek_rva_ptr (meta->img, rva, 12);
    if (p == NULL)
        return NULL;

    *max_stack = rd16 (p + 2);
    *code_size = rd32 (p + 4);
    *locals_tok = rd32 (p + 8);

    return mcpeek_rva_ptr (meta->img, rva + (rd16 (p) >> 12) * 4, *code_size);
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_il_token_name (const mcpeek_meta_t *meta, guint32 token)
{
    int table = TOKEN_TABLE (token);
    guint32 rid = TOKEN_RID (token);

    switch (table)
    {
    case MCPEEK_T_TYPEDEF:
    case MCPEEK_T_TYPEREF:
    case MCPEEK_T_TYPESPEC:
        return mcpeek_type_name (meta, table, rid, TRUE);

    case MCPEEK_T_METHODDEF:
    {
        guint32 owner;
        char *type, *sig, *res;
        const char *name;

        owner = mcpeek_meta_owner_type (meta, MCPEEK_T_METHODDEF, MCPEEK_TYPEDEF_METHODLIST, rid);
        type = owner != 0 ? mcpeek_type_name (meta, MCPEEK_T_TYPEDEF, owner, TRUE) : g_strdup ("?");
        name = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, rid, MCPEEK_METHOD_NAME));
        sig = mcpeek_sig_method (
            meta, mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, rid, MCPEEK_METHOD_SIGNATURE));

        res = g_strconcat (type, "::", name, sig, (char *) NULL);
        g_free (type);
        g_free (sig);
        return res;
    }

    case MCPEEK_T_FIELD:
    {
        guint32 owner;
        char *type, *res;
        const char *name;

        owner = mcpeek_meta_owner_type (meta, MCPEEK_T_FIELD, MCPEEK_TYPEDEF_FIELDLIST, rid);
        type = owner != 0 ? mcpeek_type_name (meta, MCPEEK_T_TYPEDEF, owner, TRUE) : g_strdup ("?");
        name = mcpeek_meta_string (meta,
                                   mcpeek_meta_col (meta, MCPEEK_T_FIELD, rid, MCPEEK_FIELD_NAME));

        res = g_strconcat (type, "::", name, (char *) NULL);
        g_free (type);
        return res;
    }

    case MCPEEK_T_MEMBERREF:
    {
        guint32 parent;
        int p_table;
        guint32 p_rid;
        char *type, *res;
        const char *name;

        parent = mcpeek_meta_col (meta, MCPEEK_T_MEMBERREF, rid, 0);
        name = mcpeek_meta_string (meta, mcpeek_meta_col (meta, MCPEEK_T_MEMBERREF, rid, 1));

        if (mcpeek_meta_decode (MCPEEK_CI_MEMBERREFPARENT, parent, &p_table, &p_rid)
            && (p_table == MCPEEK_T_TYPEDEF || p_table == MCPEEK_T_TYPEREF
                || p_table == MCPEEK_T_TYPESPEC))
            type = mcpeek_type_name (meta, p_table, p_rid, TRUE);
        else
            type = g_strdup ("?");

        {
            guint32 sig_idx;
            const guint8 *sig_blob;
            guint32 sig_len = 0;

            sig_idx = mcpeek_meta_col (meta, MCPEEK_T_MEMBERREF, rid, 2);
            sig_blob = mcpeek_meta_blob (meta, sig_idx, &sig_len);

            /* a field signature starts with 0x06; anything else is a method,
               and then the parameters belong in the listing */
            if (sig_blob != NULL && sig_len != 0 && sig_blob[0] != 0x06)
            {
                char *sig;

                sig = mcpeek_sig_method (meta, sig_idx);
                res = g_strconcat (type, "::", name, sig, (char *) NULL);
                g_free (sig);
                g_free (type);
                return res;
            }
        }

        res = g_strconcat (type, "::", name, (char *) NULL);
        g_free (type);
        return res;
    }

    case 0x2B: /* MethodSpec: the generic method plus its instantiation */
    {
        guint32 method;
        int m_table;
        guint32 m_rid;

        method = mcpeek_meta_col (meta, 0x2B, rid, 0);
        if (mcpeek_meta_decode (MCPEEK_CI_METHODDEFORREF, method, &m_table, &m_rid))
            return mcpeek_il_token_name (meta, ((guint32) m_table << 24) | m_rid);
        return g_strdup ("?");
    }

    default:
        return g_strdup_printf ("token(%08x)", token);
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
mcpeek_il_method (const mcpeek_meta_t *meta, guint32 rid)
{
    guint32 rva;
    const guint8 *p;
    const guint8 *code;
    guint32 code_size;
    guint32 max_stack = 8;
    guint32 locals_tok = 0;
    GString *out;
    guint32 off;

    rva = mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, rid, MCPEEK_METHOD_RVA);
    if (rva == 0)
        return NULL;

    p = mcpeek_rva_ptr (meta->img, rva, 1);
    if (p == NULL)
        return NULL;

    if ((p[0] & 3) == 2)
    {
        code_size = p[0] >> 2;
        code = mcpeek_rva_ptr (meta->img, rva + 1, code_size);
    }
    else
    {
        p = mcpeek_rva_ptr (meta->img, rva, 12);
        if (p == NULL)
            return NULL;

        max_stack = rd16 (p + 2);
        code_size = rd32 (p + 4);
        locals_tok = rd32 (p + 8);
        code = mcpeek_rva_ptr (meta->img, rva + (rd16 (p) >> 12) * 4, code_size);
    }

    if (code == NULL)
        return NULL;

    out = g_string_new (NULL);
    g_string_append_printf (
        out, "// %s\n",
        mcpeek_meta_string (meta,
                            mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, rid, MCPEEK_METHOD_NAME)));
    g_string_append_printf (out, "// code size %u, maxstack %u\n\n", code_size, max_stack);

    if (locals_tok != 0)
    {
        char *locals;

        locals = mcpeek_sig_locals (meta, mcpeek_meta_col (meta, 0x11, TOKEN_RID (locals_tok), 0));
        if (locals != NULL)
        {
            g_string_append_printf (out, ".locals init (%s)\n\n", locals);
            g_free (locals);
        }
    }

    for (off = 0; off < code_size;)
    {
        const mcpeek_opcode_t *op;
        guint32 start = off;
        guint8 b = code[off++];

        if (b == 0xFE)
        {
            if (off >= code_size || code[off] >= G_N_ELEMENTS (opcodes_fe))
                break;
            op = &opcodes_fe[code[off++]];
        }
        else
            op = &opcodes[b];

        if (op->name == NULL)
        {
            g_string_append_printf (out, "IL_%04x:  .byte 0x%02x\n", start, b);
            continue;
        }

        if (op->operand == OP_NONE)
            g_string_append_printf (out, "IL_%04x:  %s", start, op->name);
        else
            g_string_append_printf (out, "IL_%04x:  %-13s", start, op->name);

        switch (op->operand)
        {
        case OP_NONE:
            break;

        case OP_I1:
            if (off < code_size)
                g_string_append_printf (out, "%d", (gint8) code[off]);
            off += 1;
            break;

        case OP_U1:
            if (off < code_size)
                g_string_append_printf (out, "%u", code[off]);
            off += 1;
            break;

        case OP_U2:
            if (off + 2 <= code_size)
                g_string_append_printf (out, "%u", rd16 (code + off));
            off += 2;
            break;

        case OP_I4:
            if (off + 4 <= code_size)
                g_string_append_printf (out, "%d", (gint32) rd32 (code + off));
            off += 4;
            break;

        case OP_I8:
            if (off + 8 <= code_size)
            {
                gint64 v;

                memcpy (&v, code + off, 8);
                g_string_append_printf (out, "%" G_GINT64_FORMAT, (gint64) GINT64_FROM_LE (v));
            }
            off += 8;
            break;

        case OP_R4:
            if (off + 4 <= code_size)
            {
                guint32 bits = rd32 (code + off);
                gfloat f;

                memcpy (&f, &bits, 4);
                g_string_append_printf (out, "%g", (double) f);
            }
            off += 4;
            break;

        case OP_R8:
            if (off + 8 <= code_size)
            {
                guint64 bits;
                gdouble d;

                memcpy (&bits, code + off, 8);
                bits = GUINT64_FROM_LE (bits);
                memcpy (&d, &bits, 8);
                g_string_append_printf (out, "%g", d);
            }
            off += 8;
            break;

        case OP_TOKEN:
            if (off + 4 <= code_size)
            {
                char *name = mcpeek_il_token_name (meta, rd32 (code + off));

                g_string_append (out, name);
                g_free (name);
            }
            off += 4;
            break;

        case OP_STRING:
            if (off + 4 <= code_size)
            {
                char *s = user_string (meta, TOKEN_RID (rd32 (code + off)));

                g_string_append (out, s);
                g_free (s);
            }
            off += 4;
            break;

        case OP_BR1:
            if (off < code_size)
                g_string_append_printf (out, "IL_%04x", (guint32) (off + 1 + (gint8) code[off]));
            off += 1;
            break;

        case OP_BR4:
            if (off + 4 <= code_size)
                g_string_append_printf (out, "IL_%04x",
                                        (guint32) (off + 4 + (gint32) rd32 (code + off)));
            off += 4;
            break;

        case OP_SWITCH:
            if (off + 4 <= code_size)
            {
                guint32 n = rd32 (code + off);
                guint32 base;
                guint32 i;

                off += 4;
                base = off + n * 4;
                g_string_append_c (out, '(');
                for (i = 0; i < n && off + 4 <= code_size; i++, off += 4)
                    g_string_append_printf (out, "%sIL_%04x", i != 0 ? ", " : "",
                                            (guint32) (base + (gint32) rd32 (code + off)));
                g_string_append_c (out, ')');
            }
            else
                off += 4;
            break;

        default:
            break;
        }

        g_string_append_c (out, '\n');
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

gssize
mcpeek_il_scan (const mcpeek_meta_t *meta, guint32 rid, mcpeek_il_token_fn cb, gpointer user_data)
{
    const guint8 *code;
    guint32 code_size, max_stack, locals_tok;
    guint32 off;
    gssize count = 0;

    code = method_body (meta, rid, &code_size, &max_stack, &locals_tok);
    if (code == NULL)
        return -1;

    for (off = 0; off < code_size;)
    {
        const mcpeek_opcode_t *op;
        guint32 start = off;
        guint32 size;
        guint8 b = code[off++];

        if (b == 0xFE)
        {
            if (off >= code_size || code[off] >= G_N_ELEMENTS (opcodes_fe))
                break;
            op = &opcodes_fe[code[off++]];
        }
        else
            op = &opcodes[b];

        if (op->name == NULL)
        {
            count++;
            continue;
        }

        size = operand_size (op, code, off, code_size);
        if (size > code_size - off)
            break;

        if ((op->operand == OP_TOKEN || op->operand == OP_STRING) && cb != NULL
            && !cb (rd32 (code + off), start, user_data))
            return count + 1;

        off += size;
        count++;
    }

    return count;
}
