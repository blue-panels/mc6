/*
   mcstruct - evaluate STL definitions against file bytes

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <string.h>

#include "lib/global.h"

#include "slv.h"
#include "slv_internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAX_FIELD_BYTES (1024 * 1024)
#define MAX_REPEAT      1000000

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const unsigned char *data;
    gsize len;
} memory_reader_t;

/*** forward declarations (file scope functions) *************************************************/

static slv_node_t *eval_struct_node (slv_eval_ctx_t *outer, const slv_def_t *def, off_t offset,
                                     slv_node_t *parent);
static void eval_items (slv_eval_ctx_t *ctx, GPtrArray *items, slv_node_t *parent);
static void eval_repeat (slv_eval_ctx_t *ctx, const slv_item_t *item, slv_node_t *parent);
static void eval_check (slv_eval_ctx_t *ctx, const slv_item_t *item, slv_node_t *parent);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gssize
memory_read (void *ctx, off_t offset, void *buf, gsize len)
{
    memory_reader_t *m = ctx;

    if (offset < 0 || (gsize) offset >= m->len)
        return 0;
    if (len > m->len - offset)
        len = m->len - offset;
    memcpy (buf, m->data + offset, len);
    return (gssize) len;
}

/* --------------------------------------------------------------------------------------------- */

static off_t
memory_size (void *ctx)
{
    memory_reader_t *m = ctx;

    return (off_t) m->len;
}

/* --------------------------------------------------------------------------------------------- */

static slv_node_t *
node_new (slv_node_kind_t kind, const slv_item_t *item, slv_node_t *parent)
{
    slv_node_t *n;

    n = g_new0 (slv_node_t, 1);
    n->kind = kind;
    n->item = item;
    n->line = item != NULL ? item->line : 0;
    n->parent = parent;
    if (parent != NULL)
    {
        if (parent->children == NULL)
            parent->children = g_ptr_array_new_with_free_func ((GDestroyNotify) slv_node_free);
        g_ptr_array_add (parent->children, n);
    }
    return n;
}

/* --------------------------------------------------------------------------------------------- */

static slv_node_t *
error_node (slv_node_t *parent, const slv_item_t *item, off_t offset, char *message)
{
    slv_node_t *n;

    n = node_new (SLV_NODE_ERROR, item, parent);
    n->offset = offset;
    n->text = message;
    return n;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
eval_expr (slv_eval_ctx_t *ctx, const slv_expr_t *e, slv_node_t *parent, const slv_item_t *item,
           gint64 *out)
{
    char *err = NULL;

    if (e == NULL)
    {
        error_node (parent, item, ctx->current_offset, g_strdup ("missing expression"));
        return FALSE;
    }
    if (!slv_expr_eval (e, ctx, out, &err))
    {
        error_node (parent, item, ctx->current_offset, err);
        return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
set_label (slv_eval_ctx_t *ctx, const slv_item_t *item, gint64 value, off_t offset)
{
    slv_label_t *lab;

    if (item->label == NULL)
        return;
    lab = g_new0 (slv_label_t, 1);
    lab->value = value;
    lab->offset = offset;
    g_hash_table_replace (ctx->labels, g_strdup (item->label), lab);
}

/* --------------------------------------------------------------------------------------------- */

static const slv_def_t *
find_legend (slv_eval_ctx_t *ctx, const slv_item_t *item)
{
    const slv_def_t *def;

    if (item->legend == NULL)
        return NULL;
    def = g_hash_table_lookup (ctx->ev->file->by_key, item->legend);
    if (def == NULL || (def->kind != SLV_DEF_VALUE_LEGEND && def->kind != SLV_DEF_BIT_LEGEND))
        return NULL;
    return def;
}

/* --------------------------------------------------------------------------------------------- */

/* the title: the def-file text, or the C string at "=expr" */
static char *
field_title (slv_eval_ctx_t *ctx, const slv_item_t *item)
{
    if (item->title_at != NULL)
    {
        gint64 at = 0;
        char *err = NULL;
        unsigned char buf[33];
        gsize n = 0;

        if (slv_expr_eval (item->title_at, ctx, &at, &err))
        {
            while (n < sizeof (buf) - 1 && slv_read_bytes (ctx->ev->reader, at + n, buf + n, 1)
                   && buf[n] != '\0')
                n++;
            buf[n] = '\0';
            if (n > 0 && g_utf8_validate ((const char *) buf, n, NULL))
                return g_strdup ((const char *) buf);
        }
        g_free (err);
    }
    return g_strdup (item->name != NULL && item->name[0] != '\0' ? item->name
                         : item->label != NULL                   ? item->label
                                                                 : "");
}

/* --------------------------------------------------------------------------------------------- */

static char *
make_hint (const slv_item_t *item, gint64 count)
{
    const char *tn = slv_type_name (item->type, item->size);

    if (item->type == SLV_TYPE_CHAR)
        return g_strdup_printf ("c[%lld]", (long long) count);
    if (count > 1)
        return g_strdup_printf ("%s[%lld]", tn, (long long) count);
    return g_strdup (tn);
}

/* --------------------------------------------------------------------------------------------- */

/* number of bytes a string field occupies; -1 on read error */
static gssize
string_field_size (slv_eval_ctx_t *ctx, const slv_item_t *item, gint64 max)
{
    unsigned char b;

    if (item->type == SLV_TYPE_PSTRING)
    {
        if (max > 0)
            return (gssize) max + 1;
        if (!slv_read_bytes (ctx->ev->reader, ctx->current_offset, &b, 1))
            return -1;
        return 1 + b;
    }
    /* C string */
    if (max > 0)
        return (gssize) max;
    {
        gssize n = 0;

        for (;;)
        {
            if (!slv_read_bytes (ctx->ev->reader, ctx->current_offset + n, &b, 1))
                return n; /* unterminated: take what is there */
            n++;
            if (b == (unsigned char) item->terminator)
                return n;
            if (n >= MAX_FIELD_BYTES)
                return n;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
eval_field (slv_eval_ctx_t *ctx, const slv_item_t *item, slv_node_t *parent)
{
    gint64 count = 1;
    gssize nbytes;
    unsigned char *buf;
    slv_node_t *n;
    const slv_def_t *legend = NULL;
    gint64 value = 0;
    double dvalue = 0.0;
    off_t offset = ctx->current_offset;

    switch (item->follower_kind)
    {
    case SLV_FOLLOWER_EXPR:
        ctx->current_size = item->size;
        if (!eval_expr (ctx, item->follower, parent, item, &count))
            return;
        break;
    case SLV_FOLLOWER_LEGEND:
        legend = find_legend (ctx, item);
        if (legend == NULL)
        {
            error_node (parent, item, offset,
                        g_strdup_printf ("unknown legend '%s'", item->legend));
            return;
        }
        count = 1;
        break;
    case SLV_FOLLOWER_BITS:
    default:
        count = 1;
        break;
    }

    if (legend == NULL && item->legend != NULL)
        legend = find_legend (ctx, item);

    if (item->type == SLV_TYPE_VARINT || item->type == SLV_TYPE_LEB128
        || item->type == SLV_TYPE_ZVARINT)
    {
        unsigned char vb[10];
        gsize avail = 0, got;

        while (avail < sizeof (vb)
               && slv_read_bytes (ctx->ev->reader, offset + avail, vb + avail, 1))
            avail++;
        got = slv_varint_size (item->type == SLV_TYPE_VARINT       ? 0
                                   : item->type == SLV_TYPE_LEB128 ? 1
                                                                   : 2,
                               vb, avail);
        if (got == 0)
        {
            error_node (parent, item, offset, g_strdup ("unterminated varint"));
            return;
        }
        nbytes = (gssize) got;
        count = 1;
    }
    else if (item->type == SLV_TYPE_PSTRING || item->type == SLV_TYPE_CSTRING)
    {
        nbytes = string_field_size (ctx, item, count);
        count = 1;
    }
    else if (item->type == SLV_TYPE_STR16Z)
    {
        /* words up to a NUL word, at most count of them when count > 0 */
        unsigned char wb[2];
        gssize n = 0;

        for (;;)
        {
            if (!slv_read_bytes (ctx->ev->reader, ctx->current_offset + n, wb, 2))
                break;
            n += 2;
            if ((wb[0] == 0 && wb[1] == 0) || (count > 0 && n >= count * 2) || n >= MAX_FIELD_BYTES)
                break;
        }
        nbytes = n;
        count = 1;
    }
    else if (item->type == SLV_TYPE_CHAR || item->type == SLV_TYPE_STR8
             || item->type == SLV_TYPE_STR16)
        nbytes = count > MAX_FIELD_BYTES ? MAX_FIELD_BYTES + 1 : (gssize) count * item->size;
    else if (item->type == SLV_TYPE_BITS)
        nbytes = item->size;
    else
    {
        if (count <= 0)
            count = 1; /* follower 0: read one element, do not show it */
        nbytes = count > MAX_FIELD_BYTES ? MAX_FIELD_BYTES + 1 : (gssize) count * item->size;
    }

    if (nbytes < 0)
    {
        error_node (parent, item, offset, g_strdup ("read past end of file"));
        return;
    }
    if (nbytes > MAX_FIELD_BYTES)
    {
        error_node (parent, item, offset,
                    g_strdup_printf ("field of %lld bytes is too large", (long long) nbytes));
        return;
    }

    buf = g_malloc (nbytes > 0 ? nbytes : 1);
    if (nbytes > 0 && !slv_read_bytes (ctx->ev->reader, offset, buf, nbytes))
    {
        g_free (buf);
        error_node (parent, item, offset, g_strdup ("read past end of file"));
        return;
    }

    n = node_new (SLV_NODE_FIELD, item, parent);
    n->offset = offset;
    n->size = nbytes;
    n->big_endian = item->endian_set ? item->big_endian : ctx->big_endian;
    n->text = slv_format_value_endian (item, n->big_endian, buf, nbytes, ctx->ev->float_format,
                                       &value, &dvalue);
    n->value = value;
    n->dvalue = dvalue;
    n->key = field_title (ctx, item);
    n->hint = make_hint (item, item->type == SLV_TYPE_CHAR ? nbytes : count);
    if (item->type == SLV_TYPE_STR8 || item->type == SLV_TYPE_STR16)
    {
        g_free (n->hint);
        n->hint =
            g_strdup_printf ("%s[%lld]", slv_type_name (item->type, item->size), (long long) count);
    }
    n->comment = g_strdup (item->comment);
    if (legend != NULL)
        n->legend = slv_format_legend (legend, value);
    n->lazy = FALSE;
    if (item->hidden)
    {
        /* keep the node for the label, but the view skips it */
        n->kind = SLV_NODE_FIELD;
        n->expanded = FALSE;
    }
    g_free (buf);

    set_label (ctx, item, value, offset);
    if (item->label != NULL
        && (item->type == SLV_TYPE_CHAR || item->type == SLV_TYPE_CSTRING
            || item->type == SLV_TYPE_PSTRING || item->type == SLV_TYPE_STR8
            || item->type == SLV_TYPE_STR16Z))
    {
        slv_label_t *lab = g_hash_table_lookup (ctx->labels, item->label);

        if (lab != NULL)
            lab->text = g_strdup (n->text);
    }
    ctx->current_offset = offset + nbytes;
}

/* --------------------------------------------------------------------------------------------- */

static void
eval_jump (slv_eval_ctx_t *ctx, const slv_item_t *item, slv_node_t *parent)
{
    unsigned char buf[8];
    slv_node_t *n;
    gint64 value, target, rows = 0;
    off_t offset = ctx->current_offset;
    const slv_def_t *def;
    char *err = NULL;

    if (!slv_read_bytes (ctx->ev->reader, offset, buf, item->size))
    {
        error_node (parent, item, offset, g_strdup ("read past end of file"));
        return;
    }

    n = node_new (SLV_NODE_JUMP, item, parent);
    n->offset = offset;
    n->size = item->size;
    {
        double dv;

        n->big_endian = item->endian_set ? item->big_endian : ctx->big_endian;
        n->text = slv_format_value_endian (item, n->big_endian, buf, item->size, NULL, &value, &dv);
    }
    n->value = value;
    n->key = g_strdup (item->name != NULL ? item->name : "");
    n->comment = g_strdup (item->comment);

    def = g_hash_table_lookup (ctx->ev->file->by_key, item->target != NULL ? item->target : "");
    n->def = def;
    n->hint = g_strdup_printf ("-> %s%s", item->target_is_table ? ":" : "",
                               def != NULL ? def->name : item->target);

    ctx->current_size = item->size;
    set_label (ctx, item, value, offset);
    if (item->jump == NULL || !slv_expr_eval (item->jump, ctx, &target, &err))
    {
        n->kind = SLV_NODE_ERROR;
        g_free (n->text);
        n->text = err != NULL ? err : g_strdup ("bad jump expression");
    }
    else
    {
        n->jump_target = (off_t) target;
        if (item->rows != NULL && slv_expr_eval (item->rows, ctx, &rows, &err))
            n->rows = rows;
        else
            g_free (err);
        if (def == NULL)
        {
            n->kind = SLV_NODE_ERROR;
            g_free (n->text);
            n->text = g_strdup_printf ("unknown structure '%s'", item->target);
        }
    }
    ctx->current_offset = offset + item->size;
}

/* --------------------------------------------------------------------------------------------- */

/* size of a definition whose items all have constant sizes, -1 otherwise */
static gssize
def_fixed_size (const slv_def_t *def)
{
    return slv_def_fixed_size (def);
}

/* --------------------------------------------------------------------------------------------- */

gssize
slv_def_fixed_size (const slv_def_t *def)
{
    gssize total = 0;
    guint i;

    if (def->items == NULL)
        return -1;
    for (i = 0; i < def->items->len; i++)
    {
        const slv_item_t *it = g_ptr_array_index (def->items, i);
        gint64 count;

        switch (it->kind)
        {
        case SLV_ITEM_REMARK:
            continue;
        case SLV_ITEM_SKIP:
            if (it->follower == NULL || it->follower->op != SLV_EXPR_NUMBER)
                return -1;
            total += it->direction * (gssize) it->follower->value;
            continue;
        case SLV_ITEM_JUMP:
            total += it->size;
            continue;
        case SLV_ITEM_ENDIAN:
        case SLV_ITEM_CHECK:
        case SLV_ITEM_SET:
        case SLV_ITEM_VALUE:
            continue;
        case SLV_ITEM_FIELD:
            break;
        default:
            return -1;
        }
        if (it->type == SLV_TYPE_VARINT || it->type == SLV_TYPE_LEB128
            || it->type == SLV_TYPE_ZVARINT)
            return -1;
        if (it->type == SLV_TYPE_BITS)
        {
            total += it->size;
            continue;
        }
        if (it->follower_kind == SLV_FOLLOWER_LEGEND)
        {
            total += it->size;
            continue;
        }
        if (it->follower == NULL || it->follower->op != SLV_EXPR_NUMBER)
            return -1;
        count = it->follower->value;
        if (it->type == SLV_TYPE_PSTRING)
        {
            if (count <= 0)
                return -1;
            total += count + 1;
        }
        else if (it->type == SLV_TYPE_CSTRING)
        {
            if (count <= 0)
                return -1;
            total += count;
        }
        else if (it->type == SLV_TYPE_CHAR || it->type == SLV_TYPE_STR8
                 || it->type == SLV_TYPE_STR16)
            total += count * it->size;
        else
            total += (count > 0 ? count : 1) * it->size;
    }
    return total;
}

/* --------------------------------------------------------------------------------------------- */

static void
build_rows (slv_eval_ctx_t *ctx, slv_node_t *n, const slv_def_t *def, gint64 rows)
{
    gint64 i;

    for (i = 0; i < rows; i++)
    {
        slv_node_t *row;

        row = eval_struct_node (ctx, def, ctx->current_offset, n);
        if (row == NULL)
            break;
        if (row->size == 0 && i + 1 < rows)
        {
            error_node (n, NULL, ctx->current_offset, g_strdup ("array element of zero size"));
            break;
        }
        ctx->current_offset = row->offset + row->size;
        if (ctx->current_offset >= ctx->ev->reader->size (ctx->ev->reader->ctx) && i + 1 < rows)
        {
            error_node (n, NULL, ctx->current_offset, g_strdup ("end of file inside array"));
            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
eval_nested (slv_eval_ctx_t *ctx, const slv_item_t *item, slv_node_t *parent)
{
    gint64 rows = 1;
    const slv_def_t *def;
    slv_node_t *n;
    gssize fixed;
    off_t offset = ctx->current_offset;

    if (item->follower_kind == SLV_FOLLOWER_EXPR)
    {
        ctx->current_size = 0;
        if (!eval_expr (ctx, item->follower, parent, item, &rows))
            return;
    }
    def = g_hash_table_lookup (ctx->ev->file->by_key, item->target != NULL ? item->target : "");
    if (def == NULL || (def->kind != SLV_DEF_STRUCT && def->kind != SLV_DEF_TABLE))
    {
        error_node (parent, item, offset, g_strdup_printf ("unknown structure '%s'", item->target));
        return;
    }
    if (ctx->depth >= SLV_MAX_NEST_DEPTH)
    {
        error_node (parent, item, offset, g_strdup ("structures nested too deep"));
        return;
    }

    if (item->via != NULL)
    {
        gint64 vals[16];
        guint i, nargs = item->via->args != NULL ? item->via->args->len : 0;
        char *err = NULL;
        GBytes *bytes;

        ctx->current_size = 0;
        for (i = 0; i < nargs && i < G_N_ELEMENTS (vals); i++)
            if (!eval_expr (ctx, g_ptr_array_index (item->via->args, i), parent, item, &vals[i]))
                return;
        bytes = slv_call_bytes (ctx, item->via->name, vals, nargs, &err);
        if (bytes == NULL)
        {
            error_node (parent, item, offset, err);
            return;
        }
        n = node_new (SLV_NODE_BUFFER, item, parent);
        n->def = def;
        n->offset = offset;
        n->size = 0;
        n->rows = rows;
        n->buffer = bytes;
        n->key = g_strdup (item->comment_title != NULL ? item->comment_title : def->name);
        n->hint = g_strdup_printf ("via %s", item->via->name);
        n->text = g_strdup_printf ("%lld bytes", (long long) g_bytes_get_size (bytes));
        n->comment = g_strdup (item->comment);
        return;
    }

    n = node_new (def->kind == SLV_DEF_TABLE ? SLV_NODE_TABLE : SLV_NODE_NESTED, item, parent);
    n->def = def;
    n->offset = offset;
    n->rows = rows;
    n->key = g_strdup (item->comment_title != NULL ? item->comment_title : def->name);
    n->hint = g_strdup_printf ("%s%s[%lld]", def->kind == SLV_DEF_TABLE ? ":" : "", def->name,
                               (long long) rows);
    n->comment = g_strdup (item->comment);

    if (rows <= 0)
    {
        n->size = 0;
        return;
    }
    if (rows > MAX_REPEAT)
    {
        error_node (n, NULL, offset,
                    g_strdup_printf ("%lld elements, at most %d", (long long) rows, MAX_REPEAT));
        return;
    }

    fixed = def_fixed_size (def);
    if (fixed >= 0 && rows > ctx->ev->lazy_rows)
    {
        n->lazy = TRUE;
        n->size = (off_t) fixed * rows;
        n->outer_base = ctx->outer_base;
        n->big_endian = ctx->big_endian;
        ctx->current_offset = offset + n->size;
        return;
    }

    build_rows (ctx, n, def, rows);
    n->size = ctx->current_offset - offset;
}

/* --------------------------------------------------------------------------------------------- */

static void
eval_if (slv_eval_ctx_t *ctx, const slv_item_t *item, slv_node_t *parent)
{
    guint i;

    for (i = 0; i < item->branches->len; i++)
    {
        const slv_branch_t *br = g_ptr_array_index (item->branches, i);
        gint64 v = 1;

        if (br->cond != NULL)
        {
            ctx->current_size = 0;
            if (!eval_expr (ctx, br->cond, parent, item, &v))
                return;
        }
        if (v != 0)
        {
            eval_items (ctx, br->items, parent);
            return;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
eval_repeat (slv_eval_ctx_t *ctx, const slv_item_t *item, slv_node_t *parent)
{
    const slv_branch_t *br = g_ptr_array_index (item->branches, 0);
    slv_node_t *n;
    gint64 count = 0, i;
    off_t size = ctx->ev->reader->size (ctx->ev->reader->ctx);

    if (!item->repeat_while)
    {
        ctx->current_size = 0;
        if (!eval_expr (ctx, br->cond, parent, item, &count))
            return;
        if (count > MAX_REPEAT)
            count = MAX_REPEAT;
    }

    n = node_new (SLV_NODE_REPEAT, item, parent);
    n->offset = ctx->current_offset;
    n->key = g_strdup ("#repeat");
    n->comment = g_strdup (item->comment);

    for (i = 0; item->repeat_while ? i < MAX_REPEAT : i < count; i++)
    {
        slv_node_t *row;

        if (ctx->current_offset >= size)
            break;
        if (item->repeat_while)
        {
            gint64 v = 0;
            char *err = NULL;

            ctx->current_size = 0;
            if (br->cond == NULL || !slv_expr_eval (br->cond, ctx, &v, &err))
            {
                error_node (n, item, ctx->current_offset,
                            err != NULL ? err : g_strdup ("missing condition"));
                break;
            }
            if (v == 0)
                break;
        }
        row = node_new (SLV_NODE_STRUCT, NULL, n);
        row->line = item->line;
        row->offset = ctx->current_offset;
        row->key = g_strdup ("");
        row->labels = NULL;
        row->outer_base = ctx->outer_base;
        eval_items (ctx, br->items, row);
        row->size = ctx->current_offset - row->offset;
        if (row->size < 0)
        {
            /* the body seeked backwards: cover what it read */
            row->size = 0;
            if (row->children != NULL)
            {
                guint k;

                for (k = 0; k < row->children->len; k++)
                {
                    const slv_node_t *c = g_ptr_array_index (row->children, k);

                    if (c->size > 0 && c->offset + c->size - row->offset > row->size)
                        row->size = c->offset + c->size - row->offset;
                }
            }
        }
        if (ctx->current_offset == row->offset && item->repeat_while)
        {
            /* a body that leaves the offset alone would loop forever */
            error_node (n, item, ctx->current_offset, g_strdup ("#repeat body consumes no bytes"));
            break;
        }
    }
    n->rows = n->children != NULL ? (gint64) n->children->len : 0;
    n->hint = g_strdup_printf ("[%lld]", (long long) n->rows);
    n->size = ctx->current_offset - n->offset;
}

/* --------------------------------------------------------------------------------------------- */

static void
eval_check (slv_eval_ctx_t *ctx, const slv_item_t *item, slv_node_t *parent)
{
    gint64 from, to, expected = 0;
    slv_node_t *n;
    unsigned char buf[4096];
    guint32 crc = 0xFFFFFFFFu;
    guint64 sum = 0;
    off_t pos, size = ctx->ev->reader->size (ctx->ev->reader->ctx);
    guint64 value;
    static const char *const names[] = { "crc32", "sum8", "sum16", "check" };

    ctx->current_size = 0;
    if (item->check_kind == 3)
    {
        char *err = NULL;

        n = node_new (SLV_NODE_FIELD, item, parent);
        n->offset = ctx->current_offset;
        n->size = 0;
        n->key = g_strdup (item->name != NULL && item->name[0] != '\0' ? item->name
                               : item->label != NULL                   ? item->label
                                                                       : "check");
        n->hint = g_strdup ("check");
        n->comment = g_strdup (item->comment);
        if (item->expected == NULL || !slv_expr_eval (item->expected, ctx, &expected, &err))
        {
            n->text = g_strdup ("");
            n->legend = err != NULL ? err : g_strdup ("bad expression");
        }
        else
        {
            n->value = expected;
            n->text = g_strdup_printf ("%lld", (long long) expected);
            n->legend = g_strdup (expected != 0 ? "OK" : "MISMATCH");
        }
        set_label (ctx, item, expected, ctx->current_offset);
        return;
    }
    if (!eval_expr (ctx, item->range_from, parent, item, &from)
        || !eval_expr (ctx, item->range_to, parent, item, &to))
        return;
    if (from < 0 || to < from || to > size)
    {
        error_node (parent, item, ctx->current_offset,
                    g_strdup_printf ("bad %s range %lld..%lld", names[item->check_kind],
                                     (long long) from, (long long) to));
        return;
    }
    for (pos = from; pos < to;)
    {
        gsize want = MIN ((gsize) (to - pos), sizeof (buf));
        gsize i;

        if (!slv_read_bytes (ctx->ev->reader, pos, buf, want))
        {
            error_node (parent, item, pos, g_strdup ("read past end of file"));
            return;
        }
        if (item->check_kind == 0)
            crc = slv_crc32 (crc, buf, want);
        else
            for (i = 0; i < want; i++)
                sum += buf[i];
        pos += want;
    }
    if (item->check_kind == 0)
        value = crc ^ 0xFFFFFFFFu;
    else if (item->check_kind == 1)
        value = sum & 0xFF;
    else
        value = sum & 0xFFFF;

    n = node_new (SLV_NODE_FIELD, item, parent);
    n->offset = from;
    n->size = 0;
    n->value = (gint64) value;
    n->key = g_strdup (item->name != NULL && item->name[0] != '\0' ? item->name
                           : item->label != NULL                   ? item->label
                                                                   : names[item->check_kind]);
    n->hint = g_strdup (names[item->check_kind]);
    n->comment = g_strdup (item->comment);
    n->text = g_strdup_printf (item->check_kind == 0       ? "%08llX"
                                   : item->check_kind == 1 ? "%02llX"
                                                           : "%04llX",
                               (unsigned long long) value);
    if (item->expected != NULL)
    {
        char *err = NULL;

        if (!slv_expr_eval (item->expected, ctx, &expected, &err))
        {
            n->legend = err;
        }
        else if ((guint64) expected == value)
            n->legend = g_strdup ("OK");
        else
            n->legend = g_strdup_printf ("MISMATCH, expected %llX", (unsigned long long) expected);
    }
    set_label (ctx, item, (gint64) value, from);
}

/* --------------------------------------------------------------------------------------------- */

static void
eval_items (slv_eval_ctx_t *ctx, GPtrArray *items, slv_node_t *parent)
{
    guint i;

    for (i = 0; i < items->len; i++)
    {
        const slv_item_t *it = g_ptr_array_index (items, i);
        gint64 v;

        switch (it->kind)
        {
        case SLV_ITEM_FIELD:
            eval_field (ctx, it, parent);
            break;
        case SLV_ITEM_REMARK:
        {
            slv_node_t *n = node_new (SLV_NODE_REMARK, it, parent);

            n->offset = ctx->current_offset;
            n->key = g_strdup (it->name != NULL ? it->name : "");
            n->comment = g_strdup (it->comment);
            break;
        }
        case SLV_ITEM_SKIP:
            ctx->current_size = 0;
            if (eval_expr (ctx, it->follower, parent, it, &v))
                ctx->current_offset += it->direction * v;
            break;
        case SLV_ITEM_SEEK:
            ctx->current_size = 0;
            if (eval_expr (ctx, it->follower, parent, it, &v))
                ctx->current_offset = it->direction == 2 ? (off_t) v : ctx->struct_base + v;
            break;
        case SLV_ITEM_NESTED:
            eval_nested (ctx, it, parent);
            break;
        case SLV_ITEM_JUMP:
            eval_jump (ctx, it, parent);
            break;
        case SLV_ITEM_IF:
            eval_if (ctx, it, parent);
            break;
        case SLV_ITEM_REPEAT:
            eval_repeat (ctx, it, parent);
            break;
        case SLV_ITEM_ENDIAN:
            ctx->big_endian = it->big_endian;
            break;
        case SLV_ITEM_SET:
            ctx->current_size = 0;
            if (eval_expr (ctx, it->follower, parent, it, &v))
                set_label (ctx, it, v, ctx->current_offset);
            break;
        case SLV_ITEM_CHECK:
            eval_check (ctx, it, parent);
            break;
        case SLV_ITEM_VALUE:
            ctx->current_size = 0;
            if (eval_expr (ctx, it->follower, parent, it, &v))
            {
                slv_node_t *n = node_new (SLV_NODE_FIELD, it, parent);

                n->offset = ctx->current_offset;
                n->size = 0;
                n->value = v;
                n->key = field_title (ctx, it);
                n->hint = g_strdup (it->direction == 1 ? "=x" : it->direction == 2 ? "=t" : "=");
                n->comment = g_strdup (it->comment);
                n->big_endian = ctx->big_endian;
                if (it->direction == 1)
                    n->text = g_strdup_printf ("%llX", (unsigned long long) v);
                else if (it->direction == 2)
                    n->text = slv_format_unix_time (v);
                else
                    n->text = g_strdup_printf ("%lld", (long long) v);
                set_label (ctx, it, v, ctx->current_offset);
            }
            break;
        default:
            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
label_free (gpointer p)
{
    slv_label_t *lab = p;

    g_free (lab->text);
    g_free (lab);
}

/* --------------------------------------------------------------------------------------------- */

static slv_node_t *
eval_struct_node (slv_eval_ctx_t *outer, const slv_def_t *def, off_t offset, slv_node_t *parent)
{
    slv_eval_ctx_t ctx;
    slv_node_t *n;

    n = node_new (SLV_NODE_STRUCT, NULL, parent);
    n->def = def;
    n->line = def->line;
    n->offset = offset;
    n->key = g_strdup (def->name);
    n->labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, label_free);

    ctx.ev = outer->ev;
    ctx.labels = n->labels;
    ctx.struct_base = offset;
    ctx.outer_base = outer->outer_base;
    ctx.current_offset = offset;
    ctx.current_size = 0;
    ctx.depth = outer->depth + 1;
    ctx.big_endian = outer->big_endian;
    ctx.outer = outer->labels != NULL ? outer : outer->outer;
    n->outer_base = ctx.outer_base;

    if (def->items != NULL)
        eval_items (&ctx, def->items, n);
    n->size = ctx.current_offset - offset;
    return n;
}

/* --------------------------------------------------------------------------------------------- */

static slv_node_t *
nearest_struct (const slv_node_t *node)
{
    while (node != NULL && node->kind != SLV_NODE_STRUCT)
        node = node->parent;
    return (slv_node_t *) node;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
slv_read_bytes (const slv_reader_t *reader, off_t offset, void *buf, gsize len)
{
    gssize got;

    if (len == 0)
        return TRUE;
    if (offset < 0)
        return FALSE;
    got = reader->read (reader->ctx, offset, buf, len);
    return got == (gssize) len;
}

/* --------------------------------------------------------------------------------------------- */

const slv_label_t *
slv_ctx_lookup_label (const slv_eval_ctx_t *ctx, const char *key)
{
    for (; ctx != NULL; ctx = ctx->outer)
    {
        const slv_label_t *lab;

        if (ctx->labels == NULL)
            continue;
        lab = g_hash_table_lookup (ctx->labels, key);
        if (lab != NULL)
            return lab;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

slv_reader_t *
slv_reader_new_memory (const void *data, gsize len)
{
    slv_reader_t *r;
    memory_reader_t *m;

    m = g_new0 (memory_reader_t, 1);
    m->data = data;
    m->len = len;
    r = g_new0 (slv_reader_t, 1);
    r->read = memory_read;
    r->size = memory_size;
    r->ctx = m;
    return r;
}

/* --------------------------------------------------------------------------------------------- */

void
slv_reader_free (slv_reader_t *reader)
{
    if (reader == NULL)
        return;
    g_free (reader->ctx);
    g_free (reader);
}

/* --------------------------------------------------------------------------------------------- */

slv_node_t *
slv_eval_struct (const slv_eval_t *ev, const slv_def_t *def, off_t offset)
{
    slv_eval_ctx_t top;

    if (def == NULL)
        return NULL;
    memset (&top, 0, sizeof (top));
    top.ev = ev;
    top.outer_base = offset;
    return eval_struct_node (&top, def, offset, NULL);
}

/* --------------------------------------------------------------------------------------------- */

slv_node_t *
slv_eval_table (const slv_eval_t *ev, const slv_def_t *def, off_t offset, gint64 rows)
{
    slv_eval_ctx_t top;
    slv_node_t *n;

    if (def == NULL)
        return NULL;
    memset (&top, 0, sizeof (top));
    top.ev = ev;
    top.outer_base = offset;
    top.current_offset = offset;

    n = node_new (SLV_NODE_TABLE, NULL, NULL);
    n->def = def;
    n->line = def->line;
    n->offset = offset;
    n->rows = rows;
    n->key = g_strdup (def->name);
    n->hint = g_strdup_printf (":%s[%lld]", def->name, (long long) rows);
    build_rows (&top, n, def, rows);
    n->size = top.current_offset - offset;
    return n;
}

/* --------------------------------------------------------------------------------------------- */

/* build the rows of a lazy NESTED / TABLE node */
gboolean
slv_node_expand (const slv_eval_t *ev, slv_node_t *node)
{
    slv_eval_ctx_t ctx;

    if (node == NULL || !node->lazy || node->def == NULL)
        return FALSE;

    memset (&ctx, 0, sizeof (ctx));
    ctx.ev = ev;
    ctx.outer_base = node->outer_base;
    ctx.current_offset = node->offset;
    ctx.depth = 1;
    ctx.big_endian = node->big_endian;
    build_rows (&ctx, node, node->def, node->rows);
    node->lazy = FALSE;
    node->expanded = TRUE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
slv_node_free (slv_node_t *node)
{
    if (node == NULL)
        return;
    if (node->children != NULL)
        g_ptr_array_free (node->children, TRUE);
    if (node->labels != NULL)
        g_hash_table_destroy (node->labels);
    if (node->buffer != NULL)
        g_bytes_unref (node->buffer);
    g_free (node->key);
    g_free (node->hint);
    g_free (node->text);
    g_free (node->legend);
    g_free (node->comment);
    g_free (node);
}

/* --------------------------------------------------------------------------------------------- */

/* calculator: evaluate text with the labels of the structure that contains scope */
gboolean
slv_eval_calc (const slv_eval_t *ev, const slv_node_t *scope, const char *text, gint64 *result,
               char **error)
{
    slv_expr_t *e;
    slv_eval_ctx_t ctx;
    const slv_node_t *st;
    gboolean ok;

    e = slv_expr_parse (text, error);
    if (e == NULL)
        return FALSE;

    st = nearest_struct (scope);
    memset (&ctx, 0, sizeof (ctx));
    ctx.ev = ev;
    if (st != NULL)
    {
        ctx.labels = st->labels;
        ctx.struct_base = st->offset;
        ctx.outer_base = st->outer_base;
    }
    ctx.current_offset = scope != NULL ? scope->offset : 0;
    ctx.current_size = scope != NULL && scope->size > 0 && scope->size <= 8 ? (int) scope->size : 0;
    ctx.big_endian = scope != NULL && scope->big_endian;
    ok = slv_expr_eval (e, &ctx, result, error);
    slv_expr_free (e);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */
