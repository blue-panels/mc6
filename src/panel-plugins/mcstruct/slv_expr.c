/*
   mcstruct - STL def-file expressions

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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"

#include "slv.h"
#include "slv_internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAX_EXPR_DEPTH 64

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const char *p;
    char *error;
    int depth;
} expr_parser_t;

/*** forward declarations (file scope functions) *************************************************/

static slv_expr_t *parse_or (expr_parser_t *ps);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static slv_expr_t *
expr_new (slv_expr_op_t op)
{
    slv_expr_t *e;

    e = g_new0 (slv_expr_t, 1);
    e->op = op;
    return e;
}

/* --------------------------------------------------------------------------------------------- */

static void
skip_ws (expr_parser_t *ps)
{
    while (*ps->p == ' ' || *ps->p == '\t')
        ps->p++;
}

/* --------------------------------------------------------------------------------------------- */

static void
set_error (expr_parser_t *ps, const char *msg)
{
    if (ps->error == NULL)
        ps->error = g_strdup (msg);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_ident_char (char c)
{
    return isalnum ((unsigned char) c) || c == '_' || (unsigned char) c >= 0x80;
}

/* --------------------------------------------------------------------------------------------- */

/* number: decimal, 0x.., 0.. octal, ...b binary */
gboolean
slv_parse_number (const char *s, gsize len, gint64 *out)
{
    gint64 v = 0;
    gsize i;

    if (len == 0)
        return FALSE;

    if (len > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        for (i = 2; i < len; i++)
        {
            int d;

            if (!isxdigit ((unsigned char) s[i]))
                return FALSE;
            d = isdigit ((unsigned char) s[i]) ? s[i] - '0'
                                               : (tolower ((unsigned char) s[i]) - 'a' + 10);
            v = v * 16 + d;
        }
        *out = v;
        return TRUE;
    }

    if (len > 1 && (s[len - 1] == 'b' || s[len - 1] == 'B'))
    {
        for (i = 0; i < len - 1; i++)
        {
            if (s[i] != '0' && s[i] != '1')
                return FALSE;
            v = v * 2 + (s[i] - '0');
        }
        *out = v;
        return TRUE;
    }

    if (len > 1 && s[0] == '0')
    {
        for (i = 1; i < len; i++)
        {
            if (s[i] < '0' || s[i] > '7')
                return FALSE;
            v = v * 8 + (s[i] - '0');
        }
        *out = v;
        return TRUE;
    }

    for (i = 0; i < len; i++)
    {
        if (!isdigit ((unsigned char) s[i]))
            return FALSE;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* STL 4.00: "PK" packs the first char into the most significant byte, so it
   compares equal to a little-endian read of the reversed bytes.  'PK' (5.00)
   is the value of the bytes as they lie in the file. */
static slv_expr_t *
parse_string (expr_parser_t *ps, char quote)
{
    slv_expr_t *e;
    gint64 v = 0;
    int n = 0;
    const char *s;

    s = ps->p + 1;
    while (*s != '\0' && *s != quote)
    {
        if (n >= 8)
        {
            set_error (ps, "string literal longer than 8 bytes");
            return NULL;
        }
        if (quote == '"')
            v = (v << 8) | (unsigned char) *s;
        else
            v |= ((gint64) (unsigned char) *s) << (8 * n);
        n++;
        s++;
    }
    if (*s != quote)
    {
        set_error (ps, "unterminated string literal");
        return NULL;
    }
    ps->p = s + 1;

    e = expr_new (SLV_EXPR_NUMBER);
    e->value = v;
    e->str_len = n;
    return e;
}

/* --------------------------------------------------------------------------------------------- */

static slv_expr_t *
parse_primary (expr_parser_t *ps)
{
    slv_expr_t *e = NULL;
    const char *start, *id_end;

    skip_ws (ps);

    if (*ps->p == '(')
    {
        if (++ps->depth > MAX_EXPR_DEPTH)
        {
            set_error (ps, "expression is nested too deep");
            return NULL;
        }
        ps->p++;
        e = parse_or (ps);
        ps->depth--;
        skip_ws (ps);
        if (*ps->p != ')')
        {
            set_error (ps, "missing ')'");
            slv_expr_free (e);
            return NULL;
        }
        ps->p++;
        return e;
    }

    if (*ps->p == '"' || *ps->p == '\'')
        return parse_string (ps, *ps->p);

    if (*ps->p == '@')
    {
        ps->p++;
        start = ps->p;
        while (is_ident_char (*ps->p))
            ps->p++;
        /* @w.be / @d.le */
        if (ps->p > start && ps->p[0] == '.' && (ps->p[1] == 'l' || ps->p[1] == 'b')
            && ps->p[2] == 'e' && !is_ident_char (ps->p[3]))
            ps->p += 3;
        e = expr_new (SLV_EXPR_AT);
        id_end = ps->p;
        /* @u32(expr): read at that file offset instead of the current one */
        {
            const char *q = ps->p;

            while (*q == ' ' || *q == '\t')
                q++;
            if (ps->p > start && *q == '(')
            {
                ps->p = q + 1;
                e->left = parse_or (ps);
                skip_ws (ps);
                if (e->left == NULL || *ps->p != ')')
                {
                    set_error (ps, "missing ')' after @type(");
                    slv_expr_free (e);
                    return NULL;
                }
                ps->p++;
            }
        }
        if (id_end > start)
        {
            char *id;
            slv_type_t type;
            int size;
            gboolean be, endian_set;

            id = g_ascii_strdown (start, id_end - start);
            if (slv_parse_type_id (id, &type, &size, &be, &endian_set))
            {
                e->type = type;
                e->size = size;
                e->big_endian = be;
                e->endian_set = endian_set;
            }
            else
            {
                /* @label is a label reference (seen in the original examples) */
                e->op = SLV_EXPR_LABEL;
                e->name = id;
                return e;
            }
            g_free (id);
        }
        return e;
    }

    if (*ps->p == '^')
    {
        ps->p++;
        e = expr_new (SLV_EXPR_OFFSET);
        e->offset_kind = SLV_OFFSET_FILE;
        if (*ps->p == '^')
        {
            ps->p++;
            e->offset_kind = SLV_OFFSET_OUTER;
        }
        else if (*ps->p == '.')
        {
            ps->p++;
            e->offset_kind = SLV_OFFSET_LOCAL;
        }
        if (*ps->p == '@')
        {
            ps->p++;
            e->at_current = TRUE;
            return e;
        }
        start = ps->p;
        while (is_ident_char (*ps->p))
            ps->p++;
        if (ps->p == start)
        {
            /* bare '^' = file offset of the current structure start */
            e->struct_start = TRUE;
            return e;
        }
        e->name = g_ascii_strdown (start, ps->p - start);
        return e;
    }

    if (*ps->p == '$')
    {
        ps->p++;
        start = ps->p;
        while (is_ident_char (*ps->p))
            ps->p++;
        if (ps->p - start == 4 && strncmp (start, "size", 4) == 0)
            return expr_new (SLV_EXPR_FILE_SIZE);
        if (ps->p - start == 3 && strncmp (start, "pos", 3) == 0)
        {
            e = expr_new (SLV_EXPR_OFFSET);
            e->offset_kind = SLV_OFFSET_FILE;
            e->at_current = TRUE;
            return e;
        }
        set_error (ps, "unknown $ variable");
        return NULL;
    }

    if (is_ident_char (*ps->p))
    {
        gint64 v;

        start = ps->p;
        while (is_ident_char (*ps->p))
            ps->p++;
        if (slv_parse_number (start, ps->p - start, &v))
        {
            e = expr_new (SLV_EXPR_NUMBER);
            e->value = v;
            return e;
        }
        if (isdigit ((unsigned char) *start))
        {
            set_error (ps, "bad number");
            return NULL;
        }
        e = expr_new (SLV_EXPR_LABEL);
        e->name = g_ascii_strdown (start, ps->p - start);
        return e;
    }

    set_error (ps, *ps->p == '\0' ? "unexpected end of expression" : "unexpected character");
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static slv_expr_t *
parse_unary (expr_parser_t *ps)
{
    slv_expr_t *e, *operand;
    slv_expr_op_t op;

    skip_ws (ps);
    if (*ps->p == '-')
        op = SLV_EXPR_NEG;
    else if (*ps->p == '!')
        op = SLV_EXPR_NOT;
    else if (*ps->p == '~')
        op = SLV_EXPR_BITNOT;
    else if (*ps->p == '+')
    {
        ps->p++;
        return parse_unary (ps);
    }
    else
        return parse_primary (ps);

    ps->p++;
    if (++ps->depth > MAX_EXPR_DEPTH)
    {
        set_error (ps, "expression is nested too deep");
        return NULL;
    }
    operand = parse_unary (ps);
    ps->depth--;
    if (operand == NULL)
        return NULL;
    e = expr_new (op);
    e->left = operand;
    return e;
}

/* --------------------------------------------------------------------------------------------- */

typedef struct
{
    const char *text;
    slv_expr_op_t op;
} binop_t;

static slv_expr_t *
parse_binary_level (expr_parser_t *ps, const binop_t *ops, int nops,
                    slv_expr_t *(*next) (expr_parser_t *) )
{
    slv_expr_t *left;

    left = next (ps);
    if (left == NULL)
        return NULL;

    for (;;)
    {
        int i;
        slv_expr_t *right, *e;

        skip_ws (ps);
        for (i = 0; i < nops; i++)
        {
            size_t l = strlen (ops[i].text);

            if (strncmp (ps->p, ops[i].text, l) == 0)
            {
                /* do not take '<' of '<<' or '=' of '==' for shorter operators */
                if ((ops[i].text[0] == '<' || ops[i].text[0] == '>') && l == 1
                    && (ps->p[1] == ops[i].text[0] || ps->p[1] == '='))
                    continue;
                if ((ops[i].text[0] == '&' || ops[i].text[0] == '|') && l == 1
                    && ps->p[1] == ops[i].text[0])
                    continue;
                break;
            }
        }
        if (i == nops)
            return left;
        ps->p += strlen (ops[i].text);
        right = next (ps);
        if (right == NULL)
        {
            slv_expr_free (left);
            return NULL;
        }
        e = expr_new (ops[i].op);
        e->left = left;
        e->right = right;
        left = e;
    }
}

/* --------------------------------------------------------------------------------------------- */

static slv_expr_t *
parse_mul (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "*", SLV_EXPR_MUL },
                                   { "/", SLV_EXPR_DIV },
                                   { "%", SLV_EXPR_MOD } };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_unary);
}

static slv_expr_t *
parse_add (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "+", SLV_EXPR_ADD }, { "-", SLV_EXPR_SUB } };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_mul);
}

static slv_expr_t *
parse_shift (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "<<", SLV_EXPR_SHL }, { ">>", SLV_EXPR_SHR } };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_add);
}

static slv_expr_t *
parse_rel (expr_parser_t *ps)
{
    static const binop_t ops[] = {
        { "<=", SLV_EXPR_LE }, { ">=", SLV_EXPR_GE }, { "<", SLV_EXPR_LT }, { ">", SLV_EXPR_GT }
    };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_shift);
}

static slv_expr_t *
parse_eq (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "==", SLV_EXPR_EQ }, { "!=", SLV_EXPR_NE } };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_rel);
}

static slv_expr_t *
parse_band (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "&", SLV_EXPR_BAND } };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_eq);
}

static slv_expr_t *
parse_bxor (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "^", SLV_EXPR_BXOR } };

    /* '^' as an operator only between operands; '^label' is handled in primary */
    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_band);
}

static slv_expr_t *
parse_bor (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "|", SLV_EXPR_BOR } };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_bxor);
}

static slv_expr_t *
parse_and (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "&&", SLV_EXPR_AND } };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_bor);
}

static slv_expr_t *
parse_or (expr_parser_t *ps)
{
    static const binop_t ops[] = { { "||", SLV_EXPR_OR } };

    return parse_binary_level (ps, ops, G_N_ELEMENTS (ops), parse_and);
}

/* --------------------------------------------------------------------------------------------- */

static gint64
read_le (const unsigned char *b, int size)
{
    gint64 v = 0;
    int i;

    for (i = size - 1; i >= 0; i--)
        v = (v << 8) | b[i];
    return v;
}

static gint64
read_be (const unsigned char *b, int size)
{
    gint64 v = 0;
    int i;

    for (i = 0; i < size; i++)
        v = (v << 8) | b[i];
    return v;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
eval_at (const slv_expr_t *e, const slv_eval_ctx_t *ctx, gint64 *out, char **error)
{
    unsigned char buf[8];
    int size = e->size;
    off_t at = ctx->current_offset;

    if (e->left != NULL)
    {
        gint64 v;

        if (!slv_expr_eval (e->left, ctx, &v, error))
            return FALSE;
        at = (off_t) v;
    }
    if (size == 0)
        size = ctx->current_size > 0 ? ctx->current_size : 1;
    if (size > 8)
        size = 8;
    if (!slv_read_bytes (ctx->ev->reader, at, buf, size))
    {
        *error = g_strdup_printf ("read past end of file at 0x%llx", (unsigned long long) at);
        return FALSE;
    }
    if (e->endian_set ? e->big_endian : ctx->big_endian)
        *out = read_be (buf, size);
    else
        *out = read_le (buf, size);
    if (e->type == SLV_TYPE_INT && size < 8)
    {
        gint64 sign = (gint64) 1 << (size * 8 - 1);

        *out = (*out ^ sign) - sign;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
eval_offset (const slv_expr_t *e, const slv_eval_ctx_t *ctx, gint64 *out, char **error)
{
    off_t file_off;

    if (e->struct_start)
    {
        *out = ctx->struct_base;
        return TRUE;
    }
    if (e->at_current)
        file_off = ctx->current_offset;
    else
    {
        const slv_label_t *lab;

        lab = slv_ctx_lookup_label (ctx, e->name);
        if (lab == NULL)
        {
            *error = g_strdup_printf ("unknown label '%s'", e->name);
            return FALSE;
        }
        file_off = lab->offset;
    }

    switch (e->offset_kind)
    {
    case SLV_OFFSET_FILE:
        *out = file_off;
        break;
    case SLV_OFFSET_LOCAL:
        *out = file_off - ctx->struct_base;
        break;
    case SLV_OFFSET_OUTER:
    default:
        *out = file_off - ctx->outer_base;
        break;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

slv_expr_t *
slv_expr_parse (const char *text, char **error)
{
    expr_parser_t ps = { NULL, NULL, 0 };
    slv_expr_t *e;
    const char *t = text;
    gsize len;
    char *unquoted = NULL;

    /* an expression with spaces is written in quotes; strip them */
    while (*t == ' ' || *t == '\t')
        t++;
    len = strlen (t);
    while (len > 0 && (t[len - 1] == ' ' || t[len - 1] == '\t'))
        len--;
    if (len >= 2 && t[0] == '"' && t[len - 1] == '"' && strchr (t + 1, '"') == t + len - 1
        && (memchr (t, ' ', len) != NULL || memchr (t, '\t', len) != NULL))
    {
        unquoted = g_strndup (t + 1, len - 2);
        t = unquoted;
    }

    ps.p = t;
    ps.error = NULL;
    e = parse_or (&ps);
    if (e != NULL)
    {
        skip_ws (&ps);
        if (*ps.p != '\0')
        {
            set_error (&ps, "trailing characters in expression");
            slv_expr_free (e);
            e = NULL;
        }
    }
    if (e == NULL && unquoted != NULL && len <= 10)
    {
        /* not an expression: a packed literal that happens to contain a space */
        g_free (ps.error);
        ps.error = NULL;
        ps.depth = 0;
        ps.p = text;
        skip_ws (&ps);
        e = parse_string (&ps, '"');
        if (e != NULL)
        {
            skip_ws (&ps);
            if (*ps.p != '\0')
            {
                slv_expr_free (e);
                e = NULL;
                set_error (&ps, "trailing characters in expression");
            }
        }
    }
    g_free (unquoted);
    if (e == NULL)
    {
        if (error != NULL)
            *error = ps.error;
        else
            g_free (ps.error);
        return NULL;
    }
    g_free (ps.error);
    return e;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
slv_expr_constant (const slv_expr_t *e, gint64 *value)
{
    if (e == NULL || e->op != SLV_EXPR_NUMBER)
        return FALSE;
    *value = e->value;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
slv_expr_free (slv_expr_t *e)
{
    if (e == NULL)
        return;
    slv_expr_free (e->left);
    slv_expr_free (e->right);
    g_free (e->name);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
slv_expr_eval (const slv_expr_t *e, const slv_eval_ctx_t *ctx, gint64 *out, char **error)
{
    gint64 l = 0, r = 0;

    switch (e->op)
    {
    case SLV_EXPR_NUMBER:
        *out = e->value;
        return TRUE;
    case SLV_EXPR_LABEL:
    {
        const slv_label_t *lab;

        lab = slv_ctx_lookup_label (ctx, e->name);
        if (lab == NULL)
        {
            *error = g_strdup_printf ("unknown label '%s'", e->name);
            return FALSE;
        }
        *out = lab->value;
        return TRUE;
    }
    case SLV_EXPR_AT:
        return eval_at (e, ctx, out, error);
    case SLV_EXPR_OFFSET:
        return eval_offset (e, ctx, out, error);
    case SLV_EXPR_FILE_SIZE:
        *out = ctx->ev->reader->size (ctx->ev->reader->ctx);
        return TRUE;
    default:
        break;
    }

    if (!slv_expr_eval (e->left, ctx, &l, error))
        return FALSE;

    switch (e->op)
    {
    case SLV_EXPR_NEG:
        *out = -l;
        return TRUE;
    case SLV_EXPR_NOT:
        *out = l == 0 ? 1 : 0;
        return TRUE;
    case SLV_EXPR_BITNOT:
        *out = ~l;
        return TRUE;
    case SLV_EXPR_AND:
        if (l == 0)
        {
            *out = 0;
            return TRUE;
        }
        if (!slv_expr_eval (e->right, ctx, &r, error))
            return FALSE;
        *out = r != 0 ? 1 : 0;
        return TRUE;
    case SLV_EXPR_OR:
        if (l != 0)
        {
            *out = 1;
            return TRUE;
        }
        if (!slv_expr_eval (e->right, ctx, &r, error))
            return FALSE;
        *out = r != 0 ? 1 : 0;
        return TRUE;
    default:
        break;
    }

    if (!slv_expr_eval (e->right, ctx, &r, error))
        return FALSE;

    switch (e->op)
    {
    case SLV_EXPR_ADD:
        *out = l + r;
        break;
    case SLV_EXPR_SUB:
        *out = l - r;
        break;
    case SLV_EXPR_MUL:
        *out = l * r;
        break;
    case SLV_EXPR_DIV:
    case SLV_EXPR_MOD:
        if (r == 0)
        {
            *error = g_strdup ("division by zero");
            return FALSE;
        }
        if (r == -1)
            *out = e->op == SLV_EXPR_DIV ? (gint64) (0 - (guint64) l) : 0;
        else
            *out = e->op == SLV_EXPR_DIV ? l / r : l % r;
        break;
    case SLV_EXPR_SHL:
        *out = (r < 0 || r > 63) ? 0 : (gint64) ((guint64) l << r);
        break;
    case SLV_EXPR_SHR:
        *out = (r < 0 || r > 63) ? 0 : (gint64) ((guint64) l >> r);
        break;
    case SLV_EXPR_BAND:
        *out = l & r;
        break;
    case SLV_EXPR_BOR:
        *out = l | r;
        break;
    case SLV_EXPR_BXOR:
        *out = l ^ r;
        break;
    case SLV_EXPR_EQ:
        *out = l == r;
        break;
    case SLV_EXPR_NE:
        *out = l != r;
        break;
    case SLV_EXPR_LT:
        *out = l < r;
        break;
    case SLV_EXPR_GT:
        *out = l > r;
        break;
    case SLV_EXPR_LE:
        *out = l <= r;
        break;
    case SLV_EXPR_GE:
        *out = l >= r;
        break;
    default:
        *error = g_strdup ("bad expression node");
        return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

char *
slv_expr_to_string (const slv_expr_t *e)
{
    static const char *const names[] = {
        [SLV_EXPR_ADD] = "+", [SLV_EXPR_SUB] = "-",  [SLV_EXPR_MUL] = "*",  [SLV_EXPR_DIV] = "/",
        [SLV_EXPR_MOD] = "%", [SLV_EXPR_SHL] = "<<", [SLV_EXPR_SHR] = ">>", [SLV_EXPR_BAND] = "&",
        [SLV_EXPR_BOR] = "|", [SLV_EXPR_BXOR] = "^", [SLV_EXPR_AND] = "&&", [SLV_EXPR_OR] = "||",
        [SLV_EXPR_EQ] = "==", [SLV_EXPR_NE] = "!=",  [SLV_EXPR_LT] = "<",   [SLV_EXPR_GT] = ">",
        [SLV_EXPR_LE] = "<=", [SLV_EXPR_GE] = ">=",
    };
    char *l, *r, *s;

    if (e == NULL)
        return g_strdup ("");

    switch (e->op)
    {
    case SLV_EXPR_NUMBER:
        return g_strdup_printf ("%lld", (long long) e->value);
    case SLV_EXPR_LABEL:
        return g_strdup (e->name);
    case SLV_EXPR_AT:
        if (e->size == 0)
            return g_strdup ("@");
        return g_strdup_printf ("@%s", slv_type_name (e->type, e->size));
    case SLV_EXPR_OFFSET:
        return g_strdup_printf ("^%s%s%s",
                                e->offset_kind == SLV_OFFSET_LOCAL       ? "."
                                    : e->offset_kind == SLV_OFFSET_OUTER ? "^"
                                                                         : "",
                                e->at_current ? "@" : "", e->name != NULL ? e->name : "");
    case SLV_EXPR_FILE_SIZE:
        return g_strdup ("$size");
    case SLV_EXPR_NEG:
    case SLV_EXPR_NOT:
    case SLV_EXPR_BITNOT:
        l = slv_expr_to_string (e->left);
        s = g_strdup_printf ("%c%s",
                             e->op == SLV_EXPR_NEG       ? '-'
                                 : e->op == SLV_EXPR_NOT ? '!'
                                                         : '~',
                             l);
        g_free (l);
        return s;
    default:
        l = slv_expr_to_string (e->left);
        r = slv_expr_to_string (e->right);
        s = g_strdup_printf ("(%s %s %s)", l, names[e->op], r);
        g_free (l);
        g_free (r);
        return s;
    }
}

/* --------------------------------------------------------------------------------------------- */
