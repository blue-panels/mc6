/*
   Panel plugin mcpeek for the M-Commander
   ECMA-335 metadata tables and heaps

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

#include "mcpeek-meta.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* Column kinds used by the layout tables below. */
#define K_U8        1
#define K_U16       2
#define K_U32       3
#define K_STR       4
#define K_GUID      5
#define K_BLOB      6
#define K_RID(t)    (0x40 + (t)) /* simple index into table t */
#define K_CI(k)     (0x80 + (k)) /* coded index of kind k */

#define K_IS_RID(c) ((c) >= 0x40 && (c) < 0x80)
#define K_IS_CI(c)  ((c) >= 0x80)

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* Physical layout of every table that can appear in an assembly, ECMA-335
   II.22.  Tables we never read still need their layout: a row base is the sum
   of the sizes of all present tables before it. */
static const guint8 table_layout[MCPEEK_TABLE_MAX][10] = {
    /* 00 Module */
    { K_U16, K_STR, K_GUID, K_GUID, K_GUID, 0 },
    /* 01 TypeRef */
    { K_CI (MCPEEK_CI_RESOLUTIONSCOPE), K_STR, K_STR, 0 },
    /* 02 TypeDef */
    { K_U32, K_STR, K_STR, K_CI (MCPEEK_CI_TYPEDEFORREF), K_RID (MCPEEK_T_FIELD),
      K_RID (MCPEEK_T_METHODDEF), 0 },
    /* 03 FieldPtr */
    { K_RID (MCPEEK_T_FIELD), 0 },
    /* 04 Field */
    { K_U16, K_STR, K_BLOB, 0 },
    /* 05 MethodPtr */
    { K_RID (MCPEEK_T_METHODDEF), 0 },
    /* 06 MethodDef */
    { K_U32, K_U16, K_U16, K_STR, K_BLOB, K_RID (MCPEEK_T_PARAM), 0 },
    /* 07 ParamPtr */
    { K_RID (MCPEEK_T_PARAM), 0 },
    /* 08 Param */
    { K_U16, K_U16, K_STR, 0 },
    /* 09 InterfaceImpl */
    { K_RID (MCPEEK_T_TYPEDEF), K_CI (MCPEEK_CI_TYPEDEFORREF), 0 },
    /* 0A MemberRef */
    { K_CI (MCPEEK_CI_MEMBERREFPARENT), K_STR, K_BLOB, 0 },
    /* 0B Constant */
    { K_U16, K_CI (MCPEEK_CI_HASCONSTANT), K_BLOB, 0 },
    /* 0C CustomAttribute */
    { K_CI (MCPEEK_CI_HASCUSTOMATTRIBUTE), K_CI (MCPEEK_CI_CUSTOMATTRIBUTETYPE), K_BLOB, 0 },
    /* 0D FieldMarshal */
    { K_CI (MCPEEK_CI_HASFIELDMARSHAL), K_BLOB, 0 },
    /* 0E DeclSecurity */
    { K_U16, K_CI (MCPEEK_CI_HASDECLSECURITY), K_BLOB, 0 },
    /* 0F ClassLayout */
    { K_U16, K_U32, K_RID (MCPEEK_T_TYPEDEF), 0 },
    /* 10 FieldLayout */
    { K_U32, K_RID (MCPEEK_T_FIELD), 0 },
    /* 11 StandAloneSig */
    { K_BLOB, 0 },
    /* 12 EventMap */
    { K_RID (MCPEEK_T_TYPEDEF), K_RID (MCPEEK_T_EVENT), 0 },
    /* 13 EventPtr */
    { K_RID (MCPEEK_T_EVENT), 0 },
    /* 14 Event */
    { K_U16, K_STR, K_CI (MCPEEK_CI_TYPEDEFORREF), 0 },
    /* 15 PropertyMap */
    { K_RID (MCPEEK_T_TYPEDEF), K_RID (MCPEEK_T_PROPERTY), 0 },
    /* 16 PropertyPtr */
    { K_RID (MCPEEK_T_PROPERTY), 0 },
    /* 17 Property */
    { K_U16, K_STR, K_BLOB, 0 },
    /* 18 MethodSemantics */
    { K_U16, K_RID (MCPEEK_T_METHODDEF), K_CI (MCPEEK_CI_HASSEMANTICS), 0 },
    /* 19 MethodImpl */
    { K_RID (MCPEEK_T_TYPEDEF), K_CI (MCPEEK_CI_METHODDEFORREF), K_CI (MCPEEK_CI_METHODDEFORREF),
      0 },
    /* 1A ModuleRef */
    { K_STR, 0 },
    /* 1B TypeSpec */
    { K_BLOB, 0 },
    /* 1C ImplMap */
    { K_U16, K_CI (MCPEEK_CI_MEMBERFORWARDED), K_STR, K_RID (MCPEEK_T_MODULEREF), 0 },
    /* 1D FieldRVA */
    { K_U32, K_RID (MCPEEK_T_FIELD), 0 },
    /* 1E EncLog */
    { K_U32, K_U32, 0 },
    /* 1F EncMap */
    { K_U32, 0 },
    /* 20 Assembly */
    { K_U32, K_U16, K_U16, K_U16, K_U16, K_U32, K_BLOB, K_STR, K_STR, 0 },
    /* 21 AssemblyProcessor */
    { K_U32, 0 },
    /* 22 AssemblyOS */
    { K_U32, K_U32, K_U32, 0 },
    /* 23 AssemblyRef */
    { K_U16, K_U16, K_U16, K_U16, K_U32, K_BLOB, K_STR, K_STR, K_BLOB, 0 },
    /* 24 AssemblyRefProcessor */
    { K_U32, K_RID (MCPEEK_T_ASSEMBLYREF), 0 },
    /* 25 AssemblyRefOS */
    { K_U32, K_U32, K_U32, K_RID (MCPEEK_T_ASSEMBLYREF), 0 },
    /* 26 File */
    { K_U32, K_STR, K_BLOB, 0 },
    /* 27 ExportedType */
    { K_U32, K_U32, K_STR, K_STR, K_CI (MCPEEK_CI_IMPLEMENTATION), 0 },
    /* 28 ManifestResource */
    { K_U32, K_U32, K_STR, K_CI (MCPEEK_CI_IMPLEMENTATION), 0 },
    /* 29 NestedClass */
    { K_RID (MCPEEK_T_TYPEDEF), K_RID (MCPEEK_T_TYPEDEF), 0 },
    /* 2A GenericParam */
    { K_U16, K_U16, K_CI (MCPEEK_CI_TYPEORMETHODDEF), K_STR, 0 },
    /* 2B MethodSpec */
    { K_CI (MCPEEK_CI_METHODDEFORREF), K_BLOB, 0 },
    /* 2C GenericParamConstraint */
    { K_RID (MCPEEK_T_GENERICPARAM), K_CI (MCPEEK_CI_TYPEDEFORREF), 0 },
    /* 2D..3F unused in an assembly (0x30+ belong to portable PDBs) */
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
};

/* Tag -> table for every coded index kind; -1 marks a tag with no table. */
static const gint8 coded_tags[MCPEEK_CI_COUNT][32] = {
    /* TypeDefOrRef */
    { 0x02, 0x01, 0x1B },
    /* HasConstant */
    { 0x04, 0x08, 0x17 },
    /* HasCustomAttribute, ECMA-335 II.24.2.6 */
    { 0x06, 0x04, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x00, 0x0E, 0x17, 0x14,
      0x11, 0x1A, 0x1B, 0x20, 0x23, 0x26, 0x27, 0x28, 0x2A, 0x2C, 0x2B },
    /* HasFieldMarshal */
    { 0x04, 0x08 },
    /* HasDeclSecurity */
    { 0x02, 0x06, 0x20 },
    /* MemberRefParent */
    { 0x02, 0x01, 0x1A, 0x06, 0x1B },
    /* HasSemantics */
    { 0x14, 0x17 },
    /* MethodDefOrRef */
    { 0x06, 0x0A },
    /* MemberForwarded */
    { 0x04, 0x06 },
    /* Implementation */
    { 0x26, 0x23, 0x27 },
    /* CustomAttributeType */
    { -1, -1, 0x06, 0x0A, -1 },
    /* ResolutionScope */
    { 0x00, 0x1A, 0x23, 0x01 },
    /* TypeOrMethodDef */
    { 0x02, 0x06 },
};

/* Number of tags each coded index kind can take, i.e. how many tables are in
   its set.  The tag occupies ceil(log2(count)) low bits. */
static const guint8 coded_count[MCPEEK_CI_COUNT] = {
    3, 3, 22, 2, 3, 5, 2, 2, 2, 3, 5, 4, 2,
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static guint8
coded_bits (mcpeek_coded_t kind)
{
    guint8 bits = 0;
    guint8 n = coded_count[kind];

    while ((1u << bits) < n)
        bits++;

    return bits;
}

/* --------------------------------------------------------------------------------------------- */

static guint8
coded_width (const mcpeek_meta_t *meta, mcpeek_coded_t kind)
{
    guint32 max_rows = 0;
    guint8 bits = coded_bits (kind);
    guint8 i;

    for (i = 0; i < coded_count[kind]; i++)
    {
        gint8 t = coded_tags[kind][i];

        if (t >= 0 && (guint32) meta->rows[(int) t] > max_rows)
            max_rows = meta->rows[(int) t];
    }

    return max_rows < (1u << (16 - bits)) ? 2 : 4;
}

/* --------------------------------------------------------------------------------------------- */

static guint8
column_width (const mcpeek_meta_t *meta, guint8 code)
{
    if (K_IS_CI (code))
        return coded_width (meta, (mcpeek_coded_t) (code - 0x80));
    if (K_IS_RID (code))
        return meta->rows[code - 0x40] < 65536 ? 2 : 4;

    switch (code)
    {
    case K_U8:
        return 1;
    case K_U16:
        return 2;
    case K_U32:
        return 4;
    case K_STR:
        return meta->str_w;
    case K_GUID:
        return meta->guid_w;
    case K_BLOB:
        return meta->blob_w;
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Compressed unsigned integer, ECMA-335 II.23.2: one, two or four bytes
   selected by the top bits of the first one. */
static gboolean
read_compressed (const guint8 *p, gsize avail, guint32 *out, guint32 *used)
{
    if (avail == 0)
        return FALSE;

    if ((p[0] & 0x80) == 0)
    {
        *out = p[0];
        *used = 1;
    }
    else if ((p[0] & 0xC0) == 0x80)
    {
        if (avail < 2)
            return FALSE;
        *out = ((guint32) (p[0] & 0x3F) << 8) | p[1];
        *used = 2;
    }
    else if ((p[0] & 0xE0) == 0xC0)
    {
        if (avail < 4)
            return FALSE;
        *out =
            ((guint32) (p[0] & 0x1F) << 24) | ((guint32) p[1] << 16) | ((guint32) p[2] << 8) | p[3];
        *used = 4;
    }
    else
        return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mcpeek_meta_init (mcpeek_meta_t *meta, mcpeek_image_t *img, GError **error)
{
    const guint8 *t = img->tables.data;
    gsize size = img->tables.size;
    guint8 heap_sizes;
    guint64 valid;
    gsize off;
    int i;
    const guint8 *row_ptr;

    memset (meta, 0, sizeof (*meta));
    meta->img = img;

    if (size < 24)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("truncated table stream"));
        return FALSE;
    }

    heap_sizes = t[6];
    meta->str_w = (heap_sizes & 0x01) != 0 ? 4 : 2;
    meta->guid_w = (heap_sizes & 0x02) != 0 ? 4 : 2;
    meta->blob_w = (heap_sizes & 0x04) != 0 ? 4 : 2;

    memcpy (&valid, t + 8, 8);
    valid = GUINT64_FROM_LE (valid);

    /* row counts of the present tables, in table order */
    off = 24;
    for (i = 0; i < MCPEEK_TABLE_MAX; i++)
        if ((valid & (G_GUINT64_CONSTANT (1) << i)) != 0)
        {
            guint32 n;

            if (off + 4 > size)
            {
                g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                             _ ("truncated row count array"));
                return FALSE;
            }
            memcpy (&n, t + off, 4);
            meta->rows[i] = GUINT32_FROM_LE (n);
            off += 4;
        }

    /* HeapSizes bit 6: one more 4-byte field before the rows */
    if ((heap_sizes & 0x40) != 0)
        off += 4;
    if (off > size)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("truncated row count array"));
        return FALSE;
    }

    /* widths depend on the row counts, so this is a second pass */
    for (i = 0; i < MCPEEK_TABLE_MAX; i++)
    {
        guint8 c;
        guint32 width = 0;

        if (meta->rows[i] == 0)
            continue;

        if (table_layout[i][0] == 0)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                         _ ("unsupported metadata table 0x%02x"), (unsigned int) i);
            return FALSE;
        }

        for (c = 0; c < 10 && table_layout[i][c] != 0; c++)
        {
            guint8 w = column_width (meta, table_layout[i][c]);

            meta->col_off[i][c] = width;
            meta->col_width[i][c] = w;
            width += w;
        }
        meta->col_count[i] = c;
        meta->row_size[i] = width;
    }

    /* and the row bases, which follow the row count array back to back */
    row_ptr = t + off;
    for (i = 0; i < MCPEEK_TABLE_MAX; i++)
    {
        guint64 span;

        if (meta->rows[i] == 0)
            continue;

        span = (guint64) meta->rows[i] * meta->row_size[i];
        if (span > (guint64) (size - (gsize) (row_ptr - t)))
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("truncated table rows"));
            return FALSE;
        }

        meta->base[i] = row_ptr;
        row_ptr += span;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

guint32
mcpeek_meta_rows (const mcpeek_meta_t *meta, int table)
{
    if (table < 0 || table >= MCPEEK_TABLE_MAX)
        return 0;

    return meta->rows[table];
}

/* --------------------------------------------------------------------------------------------- */

guint32
mcpeek_meta_col (const mcpeek_meta_t *meta, int table, guint32 rid, int col)
{
    const guint8 *p;
    guint8 w;

    if (table < 0 || table >= MCPEEK_TABLE_MAX || meta->base[table] == NULL)
        return 0;
    if (rid == 0 || rid > meta->rows[table])
        return 0;
    if (col < 0 || col >= meta->col_count[table])
        return 0;

    p = meta->base[table] + (gsize) (rid - 1) * meta->row_size[table] + meta->col_off[table][col];
    w = meta->col_width[table][col];

    switch (w)
    {
    case 1:
        return p[0];
    case 2:
    {
        guint16 v;

        memcpy (&v, p, 2);
        return GUINT16_FROM_LE (v);
    }
    case 4:
    {
        guint32 v;

        memcpy (&v, p, 4);
        return GUINT32_FROM_LE (v);
    }
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

const char *
mcpeek_meta_string (const mcpeek_meta_t *meta, guint32 idx)
{
    const mcpeek_stream_t *s = &meta->img->strings;

    if (s->data == NULL || idx >= s->size)
        return "";

    /* the heap ends with a NUL, so a valid index always yields a bounded
       string; guard anyway for a heap that does not */
    if (memchr (s->data + idx, '\0', s->size - idx) == NULL)
        return "";

    return (const char *) s->data + idx;
}

/* --------------------------------------------------------------------------------------------- */

const guint8 *
mcpeek_meta_blob (const mcpeek_meta_t *meta, guint32 idx, guint32 *len)
{
    const mcpeek_stream_t *s = &meta->img->blob;
    guint32 n, used;

    if (s->data == NULL || idx >= s->size)
        return NULL;

    if (!read_compressed (s->data + idx, s->size - idx, &n, &used))
        return NULL;

    if (n > s->size - idx - used)
        return NULL;

    *len = n;
    return s->data + idx + used;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcpeek_meta_decode (mcpeek_coded_t kind, guint32 value, int *table, guint32 *rid)
{
    guint8 bits = coded_bits (kind);
    guint32 tag = value & ((1u << bits) - 1);
    gint8 t;

    if (tag >= coded_count[kind])
        return FALSE;

    t = coded_tags[kind][tag];
    if (t < 0)
        return FALSE;

    *table = t;
    *rid = value >> bits;

    return *rid != 0;
}

/* --------------------------------------------------------------------------------------------- */

guint32
mcpeek_meta_list_end (const mcpeek_meta_t *meta, int table, guint32 rid, int list_col,
                      int child_table)
{
    guint32 max = mcpeek_meta_rows (meta, child_table) + 1;
    guint32 end;

    if (rid >= mcpeek_meta_rows (meta, table))
        return max;

    /* the next row's list starts where this one ends; a start past the child
       table is a broken file and is read as the end of that table */
    end = mcpeek_meta_col (meta, table, rid + 1, list_col);

    return end == 0 || end > max ? max : end;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

guint32
mcpeek_meta_owner_type (const mcpeek_meta_t *meta, int child_table, int list_col, guint32 rid)
{
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_TYPEDEF);
    for (i = 1; i <= n; i++)
    {
        guint32 first, last;

        first = mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, list_col);
        last = mcpeek_meta_list_end (meta, MCPEEK_T_TYPEDEF, i, list_col, child_table);

        if (first != 0 && rid >= first && rid < last)
            return i;
    }

    return 0;
}
