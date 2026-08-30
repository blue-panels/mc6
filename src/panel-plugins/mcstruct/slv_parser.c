/*
   mcstruct - STL def-file parser

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

/*** file scope type declarations ****************************************************************/

typedef struct
{
    slv_file_t *file;
    slv_def_t *def;    /* definition being filled */
    GPtrArray *target; /* item list receiving new items */
    slv_item_t *if_stack[SLV_MAX_IF_DEPTH];
    int if_depth;
    char *pending_label;
    int line;
    int include_depth;
    const char *include_name;     /* errors inside an included file */
    const char *include_chain[8]; /* paths being included, against cycles */
} parser_t;

/*** forward declarations (file scope functions) *************************************************/

static void parse_line (parser_t *ps, const char *line);

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void add_error (parser_t *ps, const char *fmt, ...) G_GNUC_PRINTF (2, 3);

static void
add_error (parser_t *ps, const char *fmt, ...)
{
    slv_error_t *err;
    va_list ap;

    err = g_new0 (slv_error_t, 1);
    err->line = ps->line;
    va_start (ap, fmt);
    err->message = g_strdup_vprintf (fmt, ap);
    va_end (ap);
    if (ps->include_name != NULL)
    {
        char *m = g_strdup_printf ("%s: %s", ps->include_name, err->message);

        g_free (err->message);
        err->message = m;
    }
    g_ptr_array_add (ps->file->errors, err);
}

/* --------------------------------------------------------------------------------------------- */

static void
error_free (gpointer p)
{
    slv_error_t *err = p;

    g_free (err->message);
    g_free (err);
}

/* --------------------------------------------------------------------------------------------- */

static void
item_free (gpointer p)
{
    slv_item_t *it = p;

    if (it == NULL)
        return;
    g_free (it->label);
    g_free (it->name);
    g_free (it->comment);
    g_free (it->legend);
    g_free (it->bits);
    g_free (it->target);
    slv_expr_free (it->follower);
    slv_expr_free (it->rows);
    slv_expr_free (it->jump);
    slv_expr_free (it->range_from);
    slv_expr_free (it->range_to);
    slv_expr_free (it->expected);
    slv_expr_free (it->title_at);
    if (it->branches != NULL)
    {
        guint i;

        for (i = 0; i < it->branches->len; i++)
        {
            slv_branch_t *br = g_ptr_array_index (it->branches, i);

            slv_expr_free (br->cond);
            g_ptr_array_free (br->items, TRUE);
            g_free (br);
        }
        g_ptr_array_free (it->branches, TRUE);
    }
    g_free (it);
}

/* --------------------------------------------------------------------------------------------- */

static void
def_free (gpointer p)
{
    slv_def_t *def = p;

    if (def == NULL)
        return;
    g_free (def->name);
    g_free (def->key);
    if (def->items != NULL)
        g_ptr_array_free (def->items, TRUE);
    if (def->entries != NULL)
    {
        guint i;

        for (i = 0; i < def->entries->len; i++)
            g_free (g_array_index (def->entries, slv_legend_entry_t, i).text);
        g_array_free (def->entries, TRUE);
    }
    g_free (def);
}

/* --------------------------------------------------------------------------------------------- */

static char *
strip (const char *s, gsize len)
{
    while (len > 0 && isspace ((unsigned char) *s))
    {
        s++;
        len--;
    }
    while (len > 0 && isspace ((unsigned char) s[len - 1]))
        len--;
    return g_strndup (s, len);
}

/* --------------------------------------------------------------------------------------------- */

/* split "code ; comment" honouring double quotes */
static void
split_comment (const char *line, char **code, char **comment)
{
    const char *p;
    gboolean quoted = FALSE;

    for (p = line; *p != '\0'; p++)
    {
        if (*p == '"')
            quoted = !quoted;
        else if (*p == ';' && !quoted)
            break;
    }
    *code = strip (line, p - line);
    *comment = *p == ';' ? strip (p + 1, strlen (p + 1)) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* next whitespace separated token; a token starting with '"' runs to the closing quote */
static char *
next_token (const char **pp)
{
    const char *p = *pp, *start;

    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '\0')
    {
        *pp = p;
        return NULL;
    }
    start = p;
    if (*p == '"')
    {
        p++;
        while (*p != '\0' && *p != '"')
            p++;
        if (*p == '"')
            p++;
    }
    while (*p != '\0' && *p != ' ' && *p != '\t')
        p++;
    *pp = p;
    return g_strndup (start, p - start);
}

/* --------------------------------------------------------------------------------------------- */

static char *
rest_of_line (const char *p)
{
    return strip (p, strlen (p));
}

/* --------------------------------------------------------------------------------------------- */

static char *
make_key (const char *name)
{
    char *k = g_utf8_strdown (name, -1);
    char *w = k;
    const char *r;

    /* collapse inner whitespace */
    for (r = k; *r != '\0'; r++)
        if (*r != ' ' && *r != '\t')
            *w++ = *r;
    *w = '\0';
    return k;
}

/* --------------------------------------------------------------------------------------------- */

static slv_item_t *
item_new (parser_t *ps, slv_item_kind_t kind)
{
    slv_item_t *it;

    it = g_new0 (slv_item_t, 1);
    it->kind = kind;
    it->line = ps->line;
    it->label = ps->pending_label;
    ps->pending_label = NULL;
    return it;
}

/* --------------------------------------------------------------------------------------------- */

static void
add_item (parser_t *ps, slv_item_t *it)
{
    if (ps->target == NULL)
    {
        add_error (ps, "field outside of a structure");
        item_free (it);
        return;
    }
    /* "=expr" as the title: read the name from the file */
    if ((it->kind == SLV_ITEM_FIELD || it->kind == SLV_ITEM_JUMP) && it->name != NULL
        && it->name[0] == '=')
    {
        char *err = NULL;

        if (ps->file->version_major < 5)
            add_error (ps, "'=expr' titles need STL 5.00");
        it->title_at = slv_expr_parse (it->name + 1, &err);
        if (it->title_at == NULL)
        {
            add_error (ps, "bad title expression '%s': %s", it->name + 1, err);
            g_free (err);
        }
    }
    g_ptr_array_add (ps->target, it);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
is_label_token (const char *tok)
{
    gsize len = strlen (tok);

    if (len < 2 || tok[len - 1] != ':')
        return FALSE;
    if (isdigit ((unsigned char) tok[0]))
        return FALSE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
looks_like_follower (const char *tok)
{
    gint64 v;

    if (tok[0] == '\'' || tok[0] == '`' || tok[0] == '@' || tok[0] == '^' || tok[0] == '('
        || tok[0] == '"' || tok[0] == '-' || tok[0] == '$')
        return TRUE;
    return slv_parse_number (tok, strlen (tok), &v) || isdigit ((unsigned char) tok[0]);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_follower (parser_t *ps, slv_item_t *it, const char *tok)
{
    char *err = NULL;

    if (tok[0] == '\'' || tok[0] == '`')
    {
        it->follower_kind = SLV_FOLLOWER_LEGEND;
        it->legend = make_key (tok + 1);
        return;
    }
    if (it->type == SLV_TYPE_BITS && isdigit ((unsigned char) tok[0]))
    {
        it->follower_kind = SLV_FOLLOWER_BITS;
        it->bits = g_ascii_strup (tok, -1);
        return;
    }
    it->follower_kind = SLV_FOLLOWER_EXPR;
    it->follower = slv_expr_parse (tok, &err);
    if (it->follower == NULL)
    {
        add_error (ps, "bad follower '%s': %s", tok, err);
        g_free (err);
        return;
    }
    if (it->follower->op == SLV_EXPR_NUMBER && it->follower->value == 0)
        it->hidden = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_jump_target (parser_t *ps, slv_item_t *it, const char *tok)
{
    const char *eq;
    char *target, *err = NULL, *br;

    eq = strchr (tok, '=');
    if (eq == NULL)
    {
        add_error (ps, "jump needs 'target=expression'");
        return;
    }
    target = g_strndup (tok, eq - tok);
    it->jump = slv_expr_parse (eq + 1, &err);
    if (it->jump == NULL)
    {
        add_error (ps, "bad jump expression '%s': %s", eq + 1, err);
        g_free (err);
    }
    if (target[0] == ':')
    {
        it->target_is_table = TRUE;
        br = strchr (target, '[');
        if (br != NULL)
        {
            char *end = strrchr (br, ']');

            if (end != NULL)
                *end = '\0';
            it->rows = slv_expr_parse (br + 1, &err);
            if (it->rows == NULL)
            {
                add_error (ps, "bad row count '%s': %s", br + 1, err);
                g_free (err);
            }
            *br = '\0';
        }
        it->target = make_key (target + 1);
    }
    else
        it->target = make_key (target);
    g_free (target);
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_nested_name (slv_item_t *it, const char *rest)
{
    char *name = rest_of_line (rest);

    if (name[0] == ':')
    {
        it->target_is_table = TRUE;
        it->target = make_key (name + 1);
    }
    else
        it->target = make_key (name);
    it->name = g_strdup (name);
    g_free (name);
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_field_line (parser_t *ps, const char *code, char *comment)
{
    const char *p = code;
    char *tok, *id;
    slv_item_t *it;
    gboolean in_table = ps->def != NULL && ps->def->kind == SLV_DEF_TABLE;

    tok = next_token (&p);
    if (tok == NULL)
    {
        g_free (comment);
        return;
    }

    if (is_label_token (tok))
    {
        g_free (ps->pending_label);
        ps->pending_label = g_ascii_strdown (tok, strlen (tok) - 1);
        g_free (tok);
        tok = next_token (&p);
        if (tok == NULL)
        {
            g_free (comment);
            return;
        }
    }

    /* remark: ':' followed by text, no space needed */
    if (tok[0] == ':')
    {
        char *text;

        it = item_new (ps, SLV_ITEM_REMARK);
        text = g_strconcat (tok + 1, p, (char *) NULL);
        it->name = rest_of_line (text);
        g_free (text);
        it->comment = comment;
        g_free (tok);
        add_item (ps, it);
        return;
    }

    /* '=' expr: a computed value as a row; '=x' hex, '=t' unix time (5.00) */
    if (tok[0] == '=')
    {
        char *operand;
        int skip = 1;

        it = item_new (ps, SLV_ITEM_VALUE);
        if (ps->file->version_major < 5)
            add_error (ps, "'=' needs STL 5.00");
        if (tok[1] == 'x' || tok[1] == 't')
        {
            it->direction = tok[1] == 'x' ? 1 : 2;
            skip = 2;
        }
        if (tok[skip] != '\0')
            operand = g_strdup (tok + skip);
        else
            operand = next_token (&p);
        if (operand == NULL)
            add_error (ps, "'=' needs an expression");
        else
        {
            char *err = NULL;

            it->follower_kind = SLV_FOLLOWER_EXPR;
            it->follower = slv_expr_parse (operand, &err);
            if (it->follower == NULL)
            {
                add_error (ps, "bad expression '%s': %s", operand, err);
                g_free (err);
            }
            g_free (operand);
        }
        it->name = rest_of_line (p);
        it->comment = comment;
        g_free (tok);
        add_item (ps, it);
        return;
    }

    /* offset operations: '+', '-', '.', operand may be attached */
    if (tok[0] == '+' || tok[0] == '-' || tok[0] == '.')
    {
        char *operand;
        gboolean absolute = tok[0] == '.' && tok[1] == '.';

        it = item_new (ps, tok[0] == '.' ? SLV_ITEM_SEEK : SLV_ITEM_SKIP);
        it->direction = tok[0] == '-' ? -1 : absolute ? 2 : 1;
        if (absolute && ps->file->version_major < 5)
            add_error (ps, "'..' needs STL 5.00");
        if (tok[absolute ? 2 : 1] != '\0')
            operand = g_strdup (tok + (absolute ? 2 : 1));
        else
            operand = next_token (&p);
        if (operand == NULL)
            add_error (ps, "'%c' needs an operand", tok[0]);
        else
        {
            char *err = NULL;

            it->follower_kind = SLV_FOLLOWER_EXPR;
            it->follower = slv_expr_parse (operand, &err);
            if (it->follower == NULL)
            {
                add_error (ps, "bad operand '%s': %s", operand, err);
                g_free (err);
            }
            g_free (operand);
        }
        it->name = rest_of_line (p);
        it->comment = comment;
        g_free (tok);
        add_item (ps, it);
        return;
    }

    /* nested structure: '*' count name */
    if (tok[0] == '*')
    {
        char *count;

        it = item_new (ps, SLV_ITEM_NESTED);
        count = tok[1] != '\0' ? g_strdup (tok + 1) : next_token (&p);
        if (count == NULL)
            add_error (ps, "'*' needs a count and a structure name");
        else
        {
            it->type = SLV_TYPE_NONE;
            set_follower (ps, it, count);
            g_free (count);
            parse_nested_name (it, p);
            if (it->target == NULL || it->target[0] == '\0')
                add_error (ps, "'*' needs a structure name");
        }
        it->comment = comment;
        g_free (tok);
        add_item (ps, it);
        return;
    }

    /* typed field */
    id = g_ascii_strdown (tok, -1);
    g_free (tok);
    {
        slv_type_t type;
        int size;
        gboolean be, endian_set;
        char *colon;
        int max_len = 0, view = 0;

        /* table column 'c:max.view' */
        colon = strchr (id, ':');
        if (colon != NULL)
        {
            *colon = '\0';
            max_len = atoi (colon + 1);
            if (strchr (colon + 1, '.') != NULL)
                view = atoi (strchr (colon + 1, '.') + 1);
        }

        if (!slv_parse_type_id (id, &type, &size, &be, &endian_set))
        {
            add_error (ps, "unknown type '%s'", id);
            g_free (id);
            g_free (comment);
            return;
        }
        if (type == SLV_TYPE_PTR && endian_set)
            add_error (ps, "'%s': pointers are always little-endian", id);
        if (ps->file->version_major < 5
            && (size == 8 || strchr (id, '.') != NULL || type == SLV_TYPE_STR8
                || type == SLV_TYPE_STR16 || type == SLV_TYPE_CHECK || type == SLV_TYPE_VARINT
                || type == SLV_TYPE_LEB128))
            add_error (ps, "'%s' needs STL 5.00", id);

        it = item_new (ps,
                       type == SLV_TYPE_JUMP        ? SLV_ITEM_JUMP
                           : type == SLV_TYPE_CHECK ? SLV_ITEM_CHECK
                                                    : SLV_ITEM_FIELD);
        it->type = type;
        it->size = size;
        it->big_endian = be;
        it->endian_set = endian_set;
        it->view_width = view;
        it->comment = comment;

        if (type == SLV_TYPE_CHECK)
        {
            /* crc32 from..to [== expr] [name] */
            char *range, *dots, *err = NULL;

            it->check_kind = strcmp (id, "crc32") == 0 ? 0 : strcmp (id, "sum8") == 0 ? 1 : 2;
            range = next_token (&p);
            dots = range != NULL ? strstr (range, "..") : NULL;
            if (dots == NULL)
                add_error (ps, "%s needs a range 'from..to'", id);
            else
            {
                *dots = '\0';
                it->range_from = slv_expr_parse (range, &err);
                if (it->range_from == NULL)
                {
                    add_error (ps, "bad range start '%s': %s", range, err);
                    g_free (err);
                    err = NULL;
                }
                it->range_to = slv_expr_parse (dots + 2, &err);
                if (it->range_to == NULL)
                {
                    add_error (ps, "bad range end '%s': %s", dots + 2, err);
                    g_free (err);
                    err = NULL;
                }
            }
            g_free (range);
            {
                const char *save = p;

                tok = next_token (&p);
                if (tok != NULL && strcmp (tok, "==") == 0)
                {
                    char *val = next_token (&p);

                    if (val == NULL)
                        add_error (ps, "'==' needs a value");
                    else
                    {
                        it->expected = slv_expr_parse (val, &err);
                        if (it->expected == NULL)
                        {
                            add_error (ps, "bad expected value '%s': %s", val, err);
                            g_free (err);
                        }
                        g_free (val);
                    }
                }
                else
                    p = save;
                g_free (tok);
            }
            it->name = rest_of_line (p);
            g_free (id);
            add_item (ps, it);
            return;
        }

        if (colon != NULL)
        {
            const char *save = p;

            it->follower_kind = SLV_FOLLOWER_EXPR;
            it->follower = slv_expr_parse (colon + 1, NULL);
            if (it->follower == NULL)
            {
                char *num = g_strdup_printf ("%d", max_len);

                it->follower = slv_expr_parse (num, NULL);
                g_free (num);
            }
            /* c:11 'legend title */
            tok = next_token (&p);
            if (tok != NULL && (tok[0] == '\'' || tok[0] == '`'))
                it->legend = make_key (tok + 1);
            else
                p = save;
            g_free (tok);
            it->name = rest_of_line (p);
        }
        else if (type == SLV_TYPE_JUMP)
        {
            tok = next_token (&p);
            if (tok == NULL)
                add_error (ps, "jump needs 'target=expression'");
            else
            {
                parse_jump_target (ps, it, tok);
                g_free (tok);
            }
            it->name = rest_of_line (p);
        }
        else
        {
            const char *save = p;

            tok = next_token (&p);
            if (tok == NULL)
            {
                if (!in_table)
                    add_error (ps, "field needs a follower");
                it->follower_kind = SLV_FOLLOWER_EXPR;
                it->follower = slv_expr_parse ("1", NULL);
            }
            else if (in_table && !looks_like_follower (tok))
            {
                it->follower_kind = SLV_FOLLOWER_EXPR;
                it->follower = slv_expr_parse ("1", NULL);
                p = save;
            }
            else
                set_follower (ps, it, tok);
            g_free (tok);
            it->name = rest_of_line (p);
        }
        g_free (id);
        add_item (ps, it);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_legend_line (parser_t *ps, const char *code)
{
    const char *p = code;
    char *tok;
    slv_legend_entry_t ent;
    slv_expr_t *e;
    char *err = NULL;

    memset (&ent, 0, sizeof (ent));

    tok = next_token (&p);
    if (tok == NULL)
        return;
    e = slv_expr_parse (tok, &err);
    if (e == NULL || e->op != SLV_EXPR_NUMBER)
    {
        add_error (ps, "legend entry must start with a number: '%s'", tok);
        g_free (err);
        slv_expr_free (e);
        g_free (tok);
        return;
    }
    ent.min = e->value;
    ent.max = ps->def->kind == SLV_DEF_BIT_LEGEND ? e->value : e->value;
    slv_expr_free (e);
    g_free (tok);

    /* optional second number */
    {
        const char *save = p;

        tok = next_token (&p);
        if (tok != NULL && looks_like_follower (tok)
            && !(tok[0] == '-' && !isdigit ((unsigned char) tok[1])))
        {
            e = slv_expr_parse (tok, NULL);
            if (e != NULL && e->op == SLV_EXPR_NUMBER)
                ent.max = e->value;
            else
                p = save;
            slv_expr_free (e);
        }
        else
            p = save;
        g_free (tok);
    }

    ent.text = rest_of_line (p);
    g_array_append_val (ps->def->entries, ent);
}

/* --------------------------------------------------------------------------------------------- */

static void
start_def (parser_t *ps, const char *code)
{
    slv_def_t *def;
    const char *name = code + 1;
    slv_def_kind_t kind = SLV_DEF_STRUCT;
    gboolean hidden = FALSE;

    if (*name == '\'')
    {
        kind = SLV_DEF_VALUE_LEGEND;
        name++;
    }
    else if (*name == '`')
    {
        kind = SLV_DEF_BIT_LEGEND;
        name++;
    }
    else if (*name == ':')
    {
        kind = SLV_DEF_TABLE;
        name++;
    }
    else if (*name == '_')
        hidden = TRUE;

    if (ps->if_depth != 0)
    {
        add_error (ps, "#if without #fi before new definition");
        ps->if_depth = 0;
    }
    g_free (ps->pending_label);
    ps->pending_label = NULL;

    if (ps->file->defs->len >= SLV_MAX_STRUCTS)
    {
        add_error (ps, "too many definitions");
        ps->def = NULL;
        ps->target = NULL;
        return;
    }

    def = g_new0 (slv_def_t, 1);
    def->kind = kind;
    def->hidden = hidden;
    def->line = ps->line;
    def->name = strip (name, strlen (name));
    def->key = make_key (def->name);
    if (def->key[0] == '\0')
        add_error (ps, "empty definition name");
    if (kind == SLV_DEF_STRUCT || kind == SLV_DEF_TABLE)
        def->items = g_ptr_array_new_with_free_func (item_free);
    else
        def->entries = g_array_new (FALSE, FALSE, sizeof (slv_legend_entry_t));

    if (g_hash_table_lookup (ps->file->by_key, def->key) != NULL)
        add_error (ps, "duplicate definition '%s'", def->name);
    else
        g_hash_table_insert (ps->file->by_key, def->key, def);
    g_ptr_array_add (ps->file->defs, def);
    ps->def = def;
    ps->target = def->items;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
directive_is (const char *code, const char *name)
{
    gsize l = strlen (name);

    if (g_ascii_strncasecmp (code, name, l) != 0)
        return FALSE;
    return code[l] == '\0' || code[l] == ' ' || code[l] == '\t' || code[l] == '(';
}

/* --------------------------------------------------------------------------------------------- */

static slv_expr_t *
parse_directive_expr (parser_t *ps, const char *code, const char *name)
{
    slv_expr_t *e;
    char *err = NULL;
    const char *p = code + strlen (name);

    e = slv_expr_parse (p, &err);
    if (e == NULL)
    {
        add_error (ps, "bad condition '%s': %s", p, err);
        g_free (err);
    }
    return e;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
push_block (parser_t *ps, slv_item_t *it)
{
    if (ps->if_depth >= SLV_MAX_IF_DEPTH)
    {
        add_error (ps, "blocks nested too deep");
        return FALSE;
    }
    ps->if_stack[ps->if_depth++] = it;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
pop_block (parser_t *ps)
{
    ps->if_depth--;
    if (ps->if_depth == 0)
        ps->target = ps->def->items;
    else
    {
        slv_item_t *outer = ps->if_stack[ps->if_depth - 1];
        slv_branch_t *last = g_ptr_array_index (outer->branches, outer->branches->len - 1);

        ps->target = last->items;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void parse_text (parser_t *ps, const char *text, gsize len);

/* --------------------------------------------------------------------------------------------- */

/* def-files that are not UTF-8 are taken as cp866, the code page of the original program */
static char *
def_text_to_utf8 (char *data, gsize *len)
{
    char *conv;
    gsize out_len = 0;

    if (g_utf8_validate (data, *len, NULL))
        return data;
    conv = g_convert (data, *len, "UTF-8", "CP866", NULL, &out_len, NULL);
    if (conv == NULL)
        return data;
    g_free (data);
    *len = out_len;
    return conv;
}

static void
parse_include (parser_t *ps, const char *arg)
{
    char *name, *path = NULL, *data = NULL;
    gsize len = 0;
    const char *saved_name;
    int saved_line, i;

    name = g_strstrip (g_strdup (arg));
    if (name[0] == '"' && name[strlen (name) - 1] == '"' && strlen (name) >= 2)
    {
        name[strlen (name) - 1] = '\0';
        memmove (name, name + 1, strlen (name));
    }
    if (name[0] == '\0')
    {
        add_error (ps, "#include needs a file name");
        g_free (name);
        return;
    }
    if (ps->include_depth >= (int) G_N_ELEMENTS (ps->include_chain))
    {
        add_error (ps, "#include nested too deep");
        g_free (name);
        return;
    }
    /* only files next to the def-file: no absolute paths, no '..' */
    if (g_path_is_absolute (name) || strstr (name, "..") != NULL || ps->file->path == NULL)
    {
        add_error (ps, "#include '%s': only a name relative to the def-file", name);
        g_free (name);
        return;
    }
    {
        char *dir = g_path_get_dirname (ps->file->path);

        path = g_build_filename (dir, name, (char *) NULL);
        g_free (dir);
    }
    for (i = 0; i < ps->include_depth; i++)
        if (strcmp (ps->include_chain[i], path) == 0)
        {
            add_error (ps, "#include '%s' includes itself", name);
            g_free (path);
            g_free (name);
            return;
        }
    if (strcmp (path, ps->file->path) == 0 || !g_file_get_contents (path, &data, &len, NULL))
    {
        add_error (ps, "cannot read '%s'", path);
        g_free (path);
        g_free (name);
        return;
    }
    data = def_text_to_utf8 (data, &len);

    saved_name = ps->include_name;
    saved_line = ps->line;
    ps->include_name = name;
    ps->include_chain[ps->include_depth] = path;
    ps->include_depth++;
    ps->line = 0;
    parse_text (ps, data, len);
    ps->include_depth--;
    ps->include_name = saved_name;
    ps->line = saved_line;
    g_free (data);
    g_free (path);
    g_free (name);
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_directive (parser_t *ps, const char *code)
{
    slv_item_t *it;
    slv_branch_t *br;
    gboolean v5 = ps->file->version_major >= 5;

    if (directive_is (code, "#include"))
    {
        if (!v5)
            add_error (ps, "#include needs STL 5.00");
        else
            parse_include (ps, code + 8);
        return;
    }

    if (ps->def == NULL || ps->def->kind != SLV_DEF_STRUCT)
    {
        add_error (ps, "directive outside of a structure");
        return;
    }

    if (directive_is (code, "#endian"))
    {
        const char *arg = code + 7;

        while (*arg == ' ' || *arg == '\t')
            arg++;
        if (!v5)
            add_error (ps, "#endian needs STL 5.00");
        it = item_new (ps, SLV_ITEM_ENDIAN);
        if (g_ascii_strcasecmp (arg, "big") == 0 || g_ascii_strcasecmp (arg, "be") == 0)
            it->big_endian = TRUE;
        else if (g_ascii_strcasecmp (arg, "little") == 0 || g_ascii_strcasecmp (arg, "le") == 0)
            it->big_endian = FALSE;
        else
            add_error (ps, "#endian needs 'little' or 'big'");
        g_ptr_array_add (ps->target, it);
        return;
    }

    if (directive_is (code, "#set"))
    {
        const char *arg = code + 4;
        const char *eq;
        char *err = NULL;

        if (!v5)
            add_error (ps, "#set needs STL 5.00");
        eq = strchr (arg, '=');
        if (eq == NULL)
        {
            add_error (ps, "#set needs 'label = expression'");
            return;
        }
        it = item_new (ps, SLV_ITEM_SET);
        it->label = g_ascii_strdown (arg, eq - arg);
        g_strstrip (it->label);
        if (it->label[0] == '\0' || isdigit ((unsigned char) it->label[0]))
            add_error (ps, "#set needs a label name");
        it->follower_kind = SLV_FOLLOWER_EXPR;
        it->follower = slv_expr_parse (eq + 1, &err);
        if (it->follower == NULL)
        {
            add_error (ps, "bad #set expression '%s': %s", eq + 1, err);
            g_free (err);
        }
        g_ptr_array_add (ps->target, it);
        return;
    }

    if (directive_is (code, "#repeat"))
    {
        const char *arg = code + 7;

        while (*arg == ' ' || *arg == '\t')
            arg++;
        if (!v5)
            add_error (ps, "#repeat needs STL 5.00");
        it = item_new (ps, SLV_ITEM_REPEAT);
        it->branches = g_ptr_array_new ();
        br = g_new0 (slv_branch_t, 1);
        if (g_ascii_strncasecmp (arg, "while", 5) == 0 && (arg[5] == ' ' || arg[5] == '\t'))
        {
            it->repeat_while = TRUE;
            arg += 5;
        }
        br->cond = parse_directive_expr (ps, arg, "");
        br->items = g_ptr_array_new_with_free_func (item_free);
        g_ptr_array_add (it->branches, br);
        g_ptr_array_add (ps->target, it);
        if (push_block (ps, it))
            ps->target = br->items;
        return;
    }

    if (directive_is (code, "#if"))
    {
        it = item_new (ps, SLV_ITEM_IF);
        it->branches = g_ptr_array_new ();
        br = g_new0 (slv_branch_t, 1);
        br->cond = parse_directive_expr (ps, code, "#if");
        br->items = g_ptr_array_new_with_free_func (item_free);
        g_ptr_array_add (it->branches, br);
        g_ptr_array_add (ps->target, it);
        if (push_block (ps, it))
            ps->target = br->items;
        return;
    }

    if (ps->if_depth == 0)
    {
        add_error (ps, "'%s' without #if", code);
        return;
    }
    it = ps->if_stack[ps->if_depth - 1];

    if (directive_is (code, "#end"))
    {
        if (it->kind != SLV_ITEM_REPEAT)
        {
            add_error (ps, "#end without #repeat");
            return;
        }
        pop_block (ps);
        return;
    }

    if (it->kind != SLV_ITEM_IF)
    {
        add_error (ps, "'%s' inside #repeat", code);
        return;
    }

    if (directive_is (code, "#elseif") || directive_is (code, "#elif"))
    {
        br = g_new0 (slv_branch_t, 1);
        br->cond =
            parse_directive_expr (ps, code, directive_is (code, "#elif") ? "#elif" : "#elseif");
        br->items = g_ptr_array_new_with_free_func (item_free);
        g_ptr_array_add (it->branches, br);
        ps->target = br->items;
        /* STL 4.00: #elseif is "#else #if" and needs its own #fi */
        if (!v5)
            (void) push_block (ps, it);
        return;
    }
    if (directive_is (code, "#else"))
    {
        br = g_new0 (slv_branch_t, 1);
        br->items = g_ptr_array_new_with_free_func (item_free);
        g_ptr_array_add (it->branches, br);
        ps->target = br->items;
        return;
    }
    if (directive_is (code, "#fi") || directive_is (code, "#endif"))
    {
        pop_block (ps);
        return;
    }
    add_error (ps, "unknown directive '%s'", code);
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_line (parser_t *ps, const char *line)
{
    char *code, *comment;

    split_comment (line, &code, &comment);
    if (code[0] == '\0')
    {
        g_free (code);
        g_free (comment);
        return;
    }

    if (ps->file->version_major == 0)
    {
        if (g_ascii_strncasecmp (code, "STL", 3) == 0 && (code[3] == ' ' || code[3] == '\t'))
        {
            const char *v = code + 3;

            while (*v == ' ' || *v == '\t')
                v++;
            ps->file->version_major = atoi (v);
            if (strchr (v, '.') != NULL)
                ps->file->version_minor = atoi (strchr (v, '.') + 1);
            if (ps->file->version_major < 3 || ps->file->version_major > 5)
                add_error (ps, "unsupported def-file version '%s'", v);
        }
        else
        {
            add_error (ps, "first line must be 'STL n.nn'");
            ps->file->version_major = 4;
        }
        if (ps->file->version_major != 0)
        {
            g_free (code);
            g_free (comment);
            return;
        }
    }

    if (ps->include_depth != 0 && g_ascii_strncasecmp (code, "STL", 3) == 0
        && (code[3] == ' ' || code[3] == '\t'))
    {
        g_free (code);
        g_free (comment);
        return;
    }

    if (code[0] == '/')
        start_def (ps, code);
    else if (code[0] == '#')
        parse_directive (ps, code);
    else if (ps->def == NULL)
        add_error (ps, "text before the first definition");
    else if (ps->def->kind == SLV_DEF_VALUE_LEGEND || ps->def->kind == SLV_DEF_BIT_LEGEND)
        parse_legend_line (ps, code);
    else
    {
        parse_field_line (ps, code, comment);
        comment = NULL;
    }
    g_free (code);
    g_free (comment);
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_text (parser_t *ps, const char *text, gsize len)
{
    const char *p, *end = text + len;

    for (p = text; p < end;)
    {
        const char *nl = memchr (p, '\n', end - p);
        gsize l = nl != NULL ? (gsize) (nl - p) : (gsize) (end - p);
        char *line;

        if (l > 0 && p[l - 1] == '\r')
            l--;
        line = g_strndup (p, l);
        ps->line++;
        if (ps->line > SLV_MAX_DEF_LINES)
        {
            add_error (ps, "def-file longer than %d lines", SLV_MAX_DEF_LINES);
            g_free (line);
            break;
        }
        if (ps->include_depth == 0)
            g_ptr_array_add (ps->file->lines, line);
        parse_line (ps, line);
        if (ps->include_depth != 0)
            g_free (line);
        p = nl != NULL ? nl + 1 : end;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
type_size_suffix (const char *s, int *size)
{
    if (strcmp (s, "8") == 0)
        *size = 1;
    else if (strcmp (s, "16") == 0)
        *size = 2;
    else if (strcmp (s, "32") == 0)
        *size = 4;
    else if (strcmp (s, "64") == 0)
        *size = 8;
    else
        return FALSE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* id is lower-cased; accepts an optional ".le" / ".be" suffix */
gboolean
slv_parse_type_id (const char *id, slv_type_t *type, int *size, gboolean *big_endian,
                   gboolean *endian_set)
{
    char base[16];
    const char *dot;
    gsize len;

    *big_endian = FALSE;
    *endian_set = FALSE;
    dot = strchr (id, '.');
    len = dot != NULL ? (gsize) (dot - id) : strlen (id);
    if (len == 0 || len >= sizeof (base))
        return FALSE;
    memcpy (base, id, len);
    base[len] = '\0';
    if (dot != NULL)
    {
        if (strcmp (dot, ".be") == 0)
            *big_endian = TRUE;
        else if (strcmp (dot, ".le") != 0)
            return FALSE;
        *endian_set = TRUE;
    }

    if (strcmp (base, "crc32") == 0 || strcmp (base, "sum8") == 0 || strcmp (base, "sum16") == 0)
    {
        *type = SLV_TYPE_CHECK;
        *size = 0;
        return TRUE;
    }
    if (strcmp (base, "s8") == 0)
    {
        *type = SLV_TYPE_STR8;
        *size = 1;
        return TRUE;
    }
    if (strcmp (base, "v") == 0)
    {
        *type = SLV_TYPE_VARINT;
        *size = 1;
        return TRUE;
    }
    if (strcmp (base, "vl") == 0)
    {
        *type = SLV_TYPE_LEB128;
        *size = 1;
        return TRUE;
    }
    if (strcmp (base, "s16") == 0)
    {
        *type = SLV_TYPE_STR16;
        *size = 2;
        return TRUE;
    }

    if (len == 1)
    {
        switch (base[0])
        {
        case 'c':
            *type = SLV_TYPE_CHAR;
            *size = 1;
            return TRUE;
        case 'b':
            *type = SLV_TYPE_HEX;
            *size = 1;
            return TRUE;
        case 'w':
            *type = SLV_TYPE_HEX;
            *size = 2;
            return TRUE;
        case 'd':
            *type = SLV_TYPE_HEX;
            *size = 4;
            return TRUE;
        case 'q':
            *type = SLV_TYPE_HEX;
            *size = 8;
            return TRUE;
        default:
            return FALSE;
        }
    }

    if (strcmp (base, "sp") == 0)
    {
        *type = SLV_TYPE_PSTRING;
        *size = 1;
        return TRUE;
    }
    if (strcmp (base, "sc") == 0)
    {
        *type = SLV_TYPE_CSTRING;
        *size = 1;
        return TRUE;
    }
    if (strcmp (base, "td") == 0)
    {
        *type = SLV_TYPE_TIME_DOS;
        *size = 4;
        return TRUE;
    }
    if (strcmp (base, "tu") == 0)
    {
        *type = SLV_TYPE_TIME_UNIX;
        *size = 4;
        return TRUE;
    }
    if (strcmp (base, "p32") == 0)
    {
        *type = SLV_TYPE_PTR;
        *size = 4;
        return TRUE;
    }
    if (strcmp (base, "p48") == 0)
    {
        *type = SLV_TYPE_PTR;
        *size = 6;
        return TRUE;
    }
    if (strcmp (base, "f80") == 0)
    {
        *type = SLV_TYPE_FLOAT;
        *size = 10;
        return TRUE;
    }

    switch (base[0])
    {
    case 'i':
        *type = SLV_TYPE_INT;
        break;
    case 'u':
        *type = SLV_TYPE_UINT;
        break;
    case 't':
        *type = SLV_TYPE_BITS;
        break;
    case 'm':
        *type = SLV_TYPE_BE_HEX;
        *big_endian = TRUE;
        *endian_set = TRUE;
        break;
    case 'f':
        *type = SLV_TYPE_FLOAT;
        break;
    case 'e':
        *type = SLV_TYPE_MSBIN;
        break;
    case 'j':
        *type = SLV_TYPE_JUMP;
        break;
    default:
        return FALSE;
    }
    if (!type_size_suffix (base + 1, size))
        return FALSE;
    if ((*type == SLV_TYPE_FLOAT || *type == SLV_TYPE_MSBIN) && *size < 4)
        return FALSE;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

slv_file_t *
slv_file_parse (const char *text, gsize len, const char *path)
{
    parser_t ps;
    slv_file_t *file;

    file = g_new0 (slv_file_t, 1);
    file->path = g_strdup (path);
    file->defs = g_ptr_array_new_with_free_func (def_free);
    file->by_key = g_hash_table_new (g_str_hash, g_str_equal);
    file->errors = g_ptr_array_new_with_free_func (error_free);
    file->lines = g_ptr_array_new_with_free_func (g_free);

    memset (&ps, 0, sizeof (ps));
    ps.file = file;

    if (len > SLV_MAX_DEF_SIZE)
    {
        ps.line = 0;
        add_error (&ps, "def-file larger than %d bytes", SLV_MAX_DEF_SIZE);
        return file;
    }

    parse_text (&ps, text, len);

    if (ps.if_depth != 0)
        add_error (&ps, "#if without #fi at end of file");
    if (ps.pending_label != NULL)
    {
        add_error (&ps, "label '%s' without a field", ps.pending_label);
        g_free (ps.pending_label);
    }
    return file;
}

/* --------------------------------------------------------------------------------------------- */

slv_file_t *
slv_file_load (const char *path, GError **error)
{
    char *data = NULL;
    gsize len = 0;
    slv_file_t *file;

    if (!g_file_get_contents (path, &data, &len, error))
        return NULL;

    data = def_text_to_utf8 (data, &len);
    file = slv_file_parse (data, len, path);
    g_free (data);
    return file;
}

/* --------------------------------------------------------------------------------------------- */

void
slv_file_free (slv_file_t *file)
{
    if (file == NULL)
        return;
    g_hash_table_destroy (file->by_key);
    g_ptr_array_free (file->defs, TRUE);
    g_ptr_array_free (file->errors, TRUE);
    g_ptr_array_free (file->lines, TRUE);
    g_free (file->path);
    g_free (file);
}

/* --------------------------------------------------------------------------------------------- */

const slv_def_t *
slv_file_lookup (const slv_file_t *file, const char *name)
{
    char *key = make_key (name);
    const slv_def_t *def;

    def = g_hash_table_lookup (file->by_key, key);
    g_free (key);
    return def;
}

/* --------------------------------------------------------------------------------------------- */

slv_def_t *
slv_file_first_struct (const slv_file_t *file)
{
    guint i;

    for (i = 0; i < file->defs->len; i++)
    {
        slv_def_t *def = g_ptr_array_index (file->defs, i);

        if (def->kind == SLV_DEF_STRUCT && !def->hidden)
            return def;
    }
    for (i = 0; i < file->defs->len; i++)
    {
        slv_def_t *def = g_ptr_array_index (file->defs, i);

        if (def->kind == SLV_DEF_STRUCT)
            return def;
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
