/*
   Lexical scanner for the M-Commander
   Reads mc's .syntax files and tells what every byte of a text is

   Copyright (C) 1996-2025
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Paul Sheer, 1998
   Leonard den Ottolander <leonard den ottolander nl>, 2005, 2006
   Egmont Koblinger <egmont@gmail.com>, 2010
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013, 2014, 2021
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the M-Commander
   a fork of GNU Midnight Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file syntax.c
 *  \brief Source: lexical scanner driven by mc's .syntax files
 *
 *  Grown out of the editor's highlighting, which was its only consumer for
 *  thirty years.  See syntax.h for what the three objects are and why.
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/search.h"
#include "lib/strutil.h"
#include "lib/util.h"

#include "syntax.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* bytes between checkpoints */
#define SYNTAX_MARKER_DENSITY 512

#define RULE_ON_LEFT_BORDER   1
#define RULE_ON_RIGHT_BORDER  2

#define SYNTAX_TOKEN_STAR     '\001'
#define SYNTAX_TOKEN_PLUS     '\002'
#define SYNTAX_TOKEN_BRACKET  '\003'
#define SYNTAX_TOKEN_BRACE    '\004'

#define break_a                                                                                    \
    {                                                                                              \
        result = line;                                                                             \
        break;                                                                                     \
    }
#define check_a                                                                                    \
    {                                                                                              \
        if (*a == NULL)                                                                            \
        {                                                                                          \
            result = line;                                                                         \
            break;                                                                                 \
        }                                                                                          \
    }
#define check_not_a                                                                                \
    {                                                                                              \
        if (*a != NULL)                                                                            \
        {                                                                                          \
            result = line;                                                                         \
            break;                                                                                 \
        }                                                                                          \
    }

#define SYNTAX_KEYWORD(x) ((syntax_keyword_t *) (x))
#define CONTEXT_RULE(x)   ((context_rule_t *) (x))

#define ARGS_LEN          1024

/* color 0 is "whatever an uncolored byte gets"; the table starts at 1 */
#define SYNTAX_COLOR_NONE 0

/*** file scope type declarations ****************************************************************/

typedef struct
{
    GString *keyword;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    gboolean line_start;
    guint color;
} syntax_keyword_t;

typedef struct
{
    GString *left;
    unsigned char first_left;
    GString *right;
    unsigned char first_right;
    gboolean line_start_left;
    gboolean line_start_right;
    gboolean between_delimiters;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    char *keyword_first_chars;
    gboolean spelling;
    // first word is word[1]
    GPtrArray *keyword;
} context_rule_t;

/** A color as the .syntax file spells it, before it is interned. */
typedef struct
{
    const char *fg;
    const char *bg;
    const char *attrs;
} syntax_color_spec_t;

/** One symbolic color of a rule set: names, not an allocated pair. */
typedef struct
{
    char *fg;
    char *bg;
    char *attrs;
} syntax_color_t;

/** A resumable point in the walk.  State only, never bytes. */
struct syntax_rules_t
{
    int refs;
    char *type;
    GPtrArray *contexts;  // context_rule_t
    GTree *defines;
    GArray *colors;  // syntax_color_t, index 0 unused
    gboolean case_insensitive;

    gboolean line_local;
    guint ll_number_max;
    guint ll_number_color;
    guint ll_single_quote_color;
    guint ll_double_quote_color;
    char *ll_symbols;
    guint ll_symbols_color;
};

/** The running rule, wider than what callers see: borders are ours. */
typedef struct
{
    unsigned short keyword;
    off_t end;
    unsigned short context;
    unsigned short _context;
    unsigned char border;
} syntax_rule_t;

/** A resumable point in the walk.  State only, never bytes. */
typedef struct
{
    off_t offset;
    syntax_rule_t rule;
} syntax_checkpoint_t;

struct syntax_scanner_t
{
    syntax_rules_t *rules;
    syntax_get_byte_fn get_byte;
    void *data;
    off_t size;

    syntax_rule_t rule;
    off_t last;     // byte the rule above describes
    GArray *index;  // syntax_checkpoint_t, ascending by offset
};

struct syntax_palette_t
{
    GArray *pairs;  // int
    syntax_color_release_fn release;
    int normal;
};

/*** forward declarations (file scope functions) *************************************************/

static void syntax_rules_free (syntax_rules_t *r);
static void destroy_defines (GTree **defines);

/*** file scope variables ************************************************************************/

/* name of the included file the parser choked on, handed out by syntax_rules_load() */
static char *error_file_name = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
syntax_keyword_free (gpointer keyword)
{
    syntax_keyword_t *k = SYNTAX_KEYWORD (keyword);

    g_string_free (k->keyword, TRUE);
    g_free (k->whole_word_chars_left);
    g_free (k->whole_word_chars_right);
    g_free (k);
}

/* --------------------------------------------------------------------------------------------- */

static void
context_rule_free (gpointer rule)
{
    context_rule_t *r = CONTEXT_RULE (rule);

    g_string_free (r->left, TRUE);
    g_string_free (r->right, TRUE);
    g_free (r->whole_word_chars_left);
    g_free (r->whole_word_chars_right);
    g_free (r->keyword_first_chars);

    if (r->keyword != NULL)
        g_ptr_array_free (r->keyword, TRUE);

    g_free (r);
}

/* --------------------------------------------------------------------------------------------- */

static gint
mc_defines_destroy (gpointer key, gpointer value, gpointer data)
{
    (void) data;

    g_free (key);
    g_strfreev ((char **) value);

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Back to the state a fresh rule set is in.
 *
 * Used where the parser decides the rules it just read are not worth keeping:
 * a lone context with a lone keyword means the file describes nothing.
 */

static void
syntax_rules_clear (syntax_rules_t *r)
{
    guint i;

    if (r->defines != NULL)
        destroy_defines (&r->defines);
    if (r->contexts != NULL)
    {
        g_ptr_array_free (r->contexts, TRUE);
        r->contexts = NULL;
    }

    for (i = 1; i < r->colors->len; i++)
    {
        syntax_color_t *c = &g_array_index (r->colors, syntax_color_t, i);

        g_free (c->fg);
        g_free (c->bg);
        g_free (c->attrs);
    }
    g_array_set_size (r->colors, 1);

    MC_PTR_FREE (r->ll_symbols);
    r->line_local = FALSE;
    r->ll_number_max = 0;
    r->ll_number_color = SYNTAX_COLOR_NONE;
    r->ll_single_quote_color = SYNTAX_COLOR_NONE;
    r->ll_double_quote_color = SYNTAX_COLOR_NONE;
    r->ll_symbols_color = SYNTAX_COLOR_NONE;
}

/* --------------------------------------------------------------------------------------------- */
/** Completely destroys the defines tree */

static void
destroy_defines (GTree **defines)
{
    g_tree_foreach (*defines, mc_defines_destroy, NULL);
    g_tree_destroy (*defines);
    *defines = NULL;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Index of a color in the rule set's table, adding it if new.
 *
 * Kept symbolically on purpose: a rule set outlives the skin it is looked at
 * through, and is shared by consumers that draw in different ways.
 */

static guint
syntax_intern_color (syntax_rules_t *r, const syntax_color_spec_t *color)
{
    guint i;
    syntax_color_t c;
    char *attrs;

    if ((color->fg == NULL || *color->fg == '\0') && (color->bg == NULL || *color->bg == '\0')
        && (color->attrs == NULL || *color->attrs == '\0'))
        return SYNTAX_COLOR_NONE;

    /* get_args() mangles the + signs of an attribute list; unmangle them before
       anything else, so that what a rule set hands out is the name as the file
       spelled it and two rules that ask for the same attributes meet here */
    attrs = g_strdup (color->attrs);
    if (attrs != NULL)
    {
        char *p;

        for (p = attrs; (p = strchr (p, SYNTAX_TOKEN_PLUS)) != NULL;)
            *p++ = '+';
    }

    for (i = 1; i < r->colors->len; i++)
    {
        const syntax_color_t *e = &g_array_index (r->colors, syntax_color_t, i);

        if (g_strcmp0 (e->fg, color->fg) == 0 && g_strcmp0 (e->bg, color->bg) == 0
            && g_strcmp0 (e->attrs, attrs) == 0)
        {
            g_free (attrs);
            return i;
        }
    }

    c.fg = g_strdup (color->fg);
    c.bg = g_strdup (color->bg);
    c.attrs = attrs;
    g_array_append_val (r->colors, c);

    return r->colors->len - 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
syntax_rules_free (syntax_rules_t *r)
{
    guint i;

    if (r->defines != NULL)
        destroy_defines (&r->defines);
    if (r->contexts != NULL)
        g_ptr_array_free (r->contexts, TRUE);

    for (i = 0; i < r->colors->len; i++)
    {
        syntax_color_t *c = &g_array_index (r->colors, syntax_color_t, i);

        g_free (c->fg);
        g_free (c->bg);
        g_free (c->attrs);
    }
    g_array_free (r->colors, TRUE);

    g_free (r->ll_symbols);
    g_free (r->type);
    g_free (r);
}

/* --------------------------------------------------------------------------------------------- */

static syntax_rules_t *
syntax_rules_new (void)
{
    syntax_rules_t *r;
    syntax_color_t none = { NULL, NULL, NULL };

    r = g_new0 (syntax_rules_t, 1);
    r->refs = 1;
    r->colors = g_array_new (FALSE, FALSE, sizeof (syntax_color_t));
    g_array_append_val (r->colors, none);  // index 0 is "no color"
    r->ll_number_color = SYNTAX_COLOR_NONE;
    r->ll_single_quote_color = SYNTAX_COLOR_NONE;
    r->ll_double_quote_color = SYNTAX_COLOR_NONE;
    r->ll_symbols_color = SYNTAX_COLOR_NONE;

    return r;
}

/* --------------------------------------------------------------------------------------------- */

/** Wrapper for case insensitive mode */
inline static int
xx_tolower (gboolean case_insensitive, int c)
{
    return case_insensitive ? tolower (c) : c;
}

/* --------------------------------------------------------------------------------------------- */

static void
subst_defines (GTree *defines, char **argv, char **argv_end)
{
    for (; *argv != NULL && argv < argv_end; argv++)
    {
        char **t;

        t = g_tree_lookup (defines, *argv);
        if (t != NULL)
        {
            int argc, count;
            char **p;

            // Count argv array members
            argc = g_strv_length (argv + 1);

            // Count members of definition array
            count = g_strv_length (t);

            p = argv + count + argc;
            // Buffer overflow or infinitive loop in define
            if (p >= argv_end)
                break;

            // Move rest of argv after definition members
            while (argc >= 0)
                *p-- = argv[argc-- + 1];

            // Copy definition members to argv
            for (p = argv; *t != NULL; *p++ = *t++)
                ;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static off_t
compare_word_to_right (const syntax_scanner_t *sc, off_t i, const GString *text,
                       const char *whole_left, const char *whole_right, gboolean line_start)
{
    const unsigned char *p, *q;
    int c, d, j;

    c = sc->get_byte (sc->data, i - 1);
    c = xx_tolower (sc->rules->case_insensitive, c);
    if ((line_start && c != '\n') || (whole_left != NULL && strchr (whole_left, c) != NULL))
        return -1;

    for (p = (const unsigned char *) text->str, q = p + text->len; p < q; p++, i++)
    {
        switch (*p)
        {
        case SYNTAX_TOKEN_STAR:
            p++;
            while (TRUE)
            {
                c = sc->get_byte (sc->data, i);
                c = xx_tolower (sc->rules->case_insensitive, c);
                if (*p == '\0' && whole_right != NULL && strchr (whole_right, c) == NULL)
                    break;
                if (c == *p)
                    break;
                if (c == '\n')
                    return -1;
                i++;
            }
            break;
        case SYNTAX_TOKEN_PLUS:
            p++;
            j = 0;
            while (TRUE)
            {
                c = sc->get_byte (sc->data, i);
                c = xx_tolower (sc->rules->case_insensitive, c);
                if (c == *p)
                {
                    j = i;
                    if (p[0] == text->str[0]
                        && p[1] == '\0')  // handle eg '+' and @+@ keywords properly
                        break;
                }
                if (j != 0
                    && strchr ((const char *) p + 1, c)
                        != NULL)  // c exists further down, so it will get matched later
                    break;
                if (whiteness (c) || (whole_right != NULL && strchr (whole_right, c) == NULL))
                {
                    if (*p == '\0')
                    {
                        i--;
                        break;
                    }
                    if (j == 0)
                        return -1;
                    i = j;
                    break;
                }
                i++;
            }
            break;
        case SYNTAX_TOKEN_BRACKET:
            p++;
            c = -1;
            while (TRUE)
            {
                d = c;
                c = sc->get_byte (sc->data, i);
                c = xx_tolower (sc->rules->case_insensitive, c);
                for (j = 0; p[j] != SYNTAX_TOKEN_BRACKET && p[j] != '\0'; j++)
                    if (c == p[j])
                        goto found_char2;
                break;
            found_char2:
                i++;
            }
            i--;
            while (*p != SYNTAX_TOKEN_BRACKET && p <= q)
                p++;
            if (p > q)
                return -1;
            if (p[1] == d)
                i--;
            break;
        case SYNTAX_TOKEN_BRACE:
            p++;
            c = sc->get_byte (sc->data, i);
            c = xx_tolower (sc->rules->case_insensitive, c);
            for (; *p != SYNTAX_TOKEN_BRACE && *p != '\0'; p++)
                if (c == *p)
                    goto found_char3;
            return -1;
        found_char3:
            while (*p != SYNTAX_TOKEN_BRACE && p < q)
                p++;
            break;
        default:
            c = sc->get_byte (sc->data, i);
            if (*p != xx_tolower (sc->rules->case_insensitive, c))
                return -1;
        }
    }

    if (whole_right == NULL)
        return i;

    c = sc->get_byte (sc->data, i);
    c = xx_tolower (sc->rules->case_insensitive, c);
    return strchr (whole_right, c) != NULL ? -1 : i;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
xx_strchr (gboolean case_insensitive, const unsigned char *s, int char_byte)
{
    while (*s >= '\005' && xx_tolower (case_insensitive, *s) != char_byte)
        s++;

    return (const char *) s;
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_rules_going_right (syntax_scanner_t *sc, off_t i)
{
    context_rule_t *r;
    int c;
    gboolean contextchanged = FALSE;
    gboolean found_left = FALSE, found_right = FALSE;
    gboolean keyword_foundleft = FALSE, keyword_foundright = FALSE;
    gboolean is_end;
    off_t end = 0;
    syntax_rule_t _rule = sc->rule;

    c = sc->get_byte (sc->data, i);
    c = xx_tolower (sc->rules->case_insensitive, c);
    if (c == 0)
        return;

    is_end = (sc->rule.end == i);

    // check to turn off a keyword
    if (_rule.keyword != 0)
    {
        if (sc->get_byte (sc->data, i - 1) == '\n')
            _rule.keyword = 0;
        if (is_end)
        {
            _rule.keyword = 0;
            keyword_foundleft = TRUE;
        }
    }

    // check to turn off a context
    if (_rule.context != 0 && _rule.keyword == 0)
    {
        off_t e;

        r = CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, _rule.context));
        if (r->first_right == c && (sc->rule.border & RULE_ON_RIGHT_BORDER) == 0
            && r->right->len != 0
            && (e = compare_word_to_right (sc, i, r->right, r->whole_word_chars_left,
                                           r->whole_word_chars_right, r->line_start_right))
                > 0)
        {
            _rule.end = e;
            found_right = TRUE;
            _rule.border = RULE_ON_RIGHT_BORDER;
            if (r->between_delimiters)
                _rule.context = 0;
        }
        else if (is_end && (sc->rule.border & RULE_ON_RIGHT_BORDER) != 0)
        {
            // always turn off a context at 4
            found_left = TRUE;
            _rule.border = 0;
            if (!keyword_foundleft)
                _rule.context = 0;
        }
        else if (is_end && (sc->rule.border & RULE_ON_LEFT_BORDER) != 0)
        {
            // never turn off a context at 2
            found_left = TRUE;
            _rule.border = 0;
        }
    }

    // check to turn on a keyword
    if (_rule.keyword == 0)
    {
        const char *p;

        r = CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, _rule.context));
        p = r->keyword_first_chars;

        if (p != NULL)
            while (*(p = xx_strchr (sc->rules->case_insensitive, (const unsigned char *) p + 1, c))
                   != '\0')
            {
                syntax_keyword_t *k;
                int count;
                off_t e = -1;

                count = p - r->keyword_first_chars;
                k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, count));
                if (k->keyword != 0)
                    e = compare_word_to_right (sc, i, k->keyword, k->whole_word_chars_left,
                                               k->whole_word_chars_right, k->line_start);
                if (e > 0)
                {
                    /* when both context and keyword terminate with a newline,
                       the context overflows to the next line and colorizes it incorrectly */
                    if (e > i + 1 && _rule._context != 0
                        && k->keyword->str[k->keyword->len - 1] == '\n')
                    {
                        r = CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, _rule._context));
                        if (r->right != NULL && r->right->len != 0
                            && r->right->str[r->right->len - 1] == '\n')
                            e--;
                    }

                    end = e;
                    _rule.end = e;
                    _rule.keyword = count;
                    keyword_foundright = TRUE;
                    break;
                }
            }
    }

    // check to turn on a context
    if (_rule.context == 0)
    {
        if (!found_left && is_end)
        {
            if ((sc->rule.border & RULE_ON_RIGHT_BORDER) != 0)
            {
                _rule.border = 0;
                _rule.context = 0;
                contextchanged = TRUE;
                _rule.keyword = 0;
            }
            else if ((sc->rule.border & RULE_ON_LEFT_BORDER) != 0)
            {
                r = CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, _rule._context));
                _rule.border = 0;
                if (r->between_delimiters)
                {
                    _rule.context = _rule._context;
                    contextchanged = TRUE;
                    _rule.keyword = 0;

                    if (r->first_right == c)
                    {
                        off_t e = -1;

                        if (r->right->len != 0)
                            e = compare_word_to_right (sc, i, r->right, r->whole_word_chars_left,
                                                       r->whole_word_chars_right,
                                                       r->line_start_right);
                        if (e >= end)
                        {
                            _rule.end = e;
                            found_right = TRUE;
                            _rule.border = RULE_ON_RIGHT_BORDER;
                            _rule.context = 0;
                        }
                    }
                }
            }
        }

        if (!found_right)
        {
            size_t count;

            for (count = 1; count < sc->rules->contexts->len; count++)
            {
                r = CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, count));
                if (r->first_left == c)
                {
                    off_t e = -1;

                    if (r->left->len != 0)
                        e = compare_word_to_right (sc, i, r->left, r->whole_word_chars_left,
                                                   r->whole_word_chars_right, r->line_start_left);
                    if (e >= end && (_rule.keyword == 0 || keyword_foundright))
                    {
                        _rule.end = e;
                        _rule.border = RULE_ON_LEFT_BORDER;
                        _rule._context = count;
                        if (!r->between_delimiters && _rule.keyword == 0)
                        {
                            _rule.context = count;
                            contextchanged = TRUE;
                        }
                        break;
                    }
                }
            }
        }
    }

    // check again to turn on a keyword if the context switched
    if (contextchanged && _rule.keyword == 0)
    {
        const char *p;

        r = CONTEXT_RULE (g_ptr_array_index (sc->rules->contexts, _rule.context));
        p = r->keyword_first_chars;

        while (*(p = xx_strchr (sc->rules->case_insensitive, (const unsigned char *) p + 1, c))
               != '\0')
        {
            syntax_keyword_t *k;
            int count;
            off_t e = -1;

            count = p - r->keyword_first_chars;
            k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, count));

            if (k->keyword->len != 0)
                e = compare_word_to_right (sc, i, k->keyword, k->whole_word_chars_left,
                                           k->whole_word_chars_right, k->line_start);
            if (e > 0)
            {
                _rule.end = e;
                _rule.keyword = count;
                break;
            }
        }
    }

    sc->rule = _rule;
}

/* --------------------------------------------------------------------------------------------- */
/**
   Returns 0 on error/eof or a count of the number of bytes read
   including the newline. Result must be free'd.
   In case of an error, *line will not be modified.
 */

static size_t
read_one_line (char **line, FILE *f)
{
    GString *p;
    size_t r = 0;

    // not reallocate string too often
    p = g_string_sized_new (64);

    while (TRUE)
    {
        int c;

        c = fgetc (f);
        if (c == EOF)
        {
            if (ferror (f))
            {
                if (errno == EINTR)
                    continue;
                r = 0;
            }
            break;
        }
        r++;

        // handle all of \r\n, \r, \n correctly.
        if (c == '\n')
            break;
        if (c == '\r')
        {
            c = fgetc (f);
            if (c == '\n')
                r++;
            else
                ungetc (c, f);
            break;
        }

        g_string_append_c (p, c);
    }
    if (r != 0)
        *line = g_string_free (p, FALSE);
    else
        g_string_free (p, TRUE);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

static char *
convert (char *s)
{
    char *r, *p;

    p = r = s;
    while (*s)
    {
        switch (*s)
        {
        case '\\':
            s++;
            switch (*s)
            {
            case ' ':
                *p = ' ';
                s--;
                break;
            case 'n':
                *p = '\n';
                break;
            case 'r':
                *p = '\r';
                break;
            case 't':
                *p = '\t';
                break;
            case 's':
                *p = ' ';
                break;
            case '*':
                *p = '*';
                break;
            case '\\':
                *p = '\\';
                break;
            case '[':
            case ']':
                *p = SYNTAX_TOKEN_BRACKET;
                break;
            case '{':
            case '}':
                *p = SYNTAX_TOKEN_BRACE;
                break;
            case 0:
                *p = *s;
                return r;
            default:
                *p = *s;
                break;
            }
            break;
        case '*':
            *p = SYNTAX_TOKEN_STAR;
            break;
        case '+':
            *p = SYNTAX_TOKEN_PLUS;
            break;
        default:
            *p = *s;
            break;
        }
        s++;
        p++;
    }
    *p = '\0';
    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
get_args (char *l, char **args, int args_size)
{
    int argc = 0;

    while (argc < args_size)
    {
        char *p = l;

        while (*p != '\0' && whiteness (*p))
            p++;
        if (*p == '\0')
            break;
        for (l = p + 1; *l != '\0' && !whiteness (*l); l++)
            ;
        if (*l != '\0')
            *l++ = '\0';
        args[argc++] = convert (p);
    }
    args[argc] = (char *) NULL;
    return argc;
}

/* --------------------------------------------------------------------------------------------- */

static int
read_line_local_color (syntax_rules_t *r, char ***args)
{
    syntax_color_spec_t color;
    char **a = *args;

    if (*a == NULL)
        return -1;

    color.fg = *a++;
    color.bg = *a;
    if (*a != NULL)
        a++;
    color.attrs = *a;
    if (*a != NULL)
        a++;
    *args = a;

    return (int) syntax_intern_color (r, &color);
}

/* --------------------------------------------------------------------------------------------- */

static FILE *
open_include_file (const char *filename)
{
    FILE *f;

    g_free (error_file_name);
    error_file_name = g_strdup (filename);
    if (g_path_is_absolute (filename))
        return fopen (filename, "r");

    g_free (error_file_name);
    error_file_name =
        g_build_filename (mc_config_get_data_path (), EDIT_SYNTAX_DIR, filename, (char *) NULL);
    f = fopen (error_file_name, "r");
    if (f != NULL)
        return f;

    g_free (error_file_name);
    error_file_name =
        g_build_filename (mc_global.share_data_dir, EDIT_SYNTAX_DIR, filename, (char *) NULL);

    return fopen (error_file_name, "r");
}

/* --------------------------------------------------------------------------------------------- */

inline static void
xx_lowerize_line (gboolean case_insensitive, char *line, size_t len)
{
    if (case_insensitive)
    {
        size_t i;

        for (i = 0; i < len; ++i)
            line[i] = tolower (line[i]);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** returns line number on error */

static int
edit_read_syntax_rules (syntax_rules_t *r, FILE *f, char **args, int args_size)
{
    FILE *g = NULL;
    syntax_color_spec_t color;
    char last_fg[BUF_TINY / 2] = "";
    char last_bg[BUF_TINY / 2] = "";
    char last_attrs[BUF_TINY] = "";
    char whole_right[BUF_MEDIUM];
    char whole_left[BUF_MEDIUM];
    char *l = NULL;
    int save_line = 0, line = 0;
    context_rule_t *c = NULL;
    gboolean no_words = TRUE;
    int result = 0;

    args[0] = NULL;
    r->case_insensitive = FALSE;
    r->line_local = FALSE;
    r->ll_number_max = 0;
    r->ll_number_color = SYNTAX_COLOR_NONE;
    r->ll_single_quote_color = SYNTAX_COLOR_NONE;
    r->ll_double_quote_color = SYNTAX_COLOR_NONE;
    MC_PTR_FREE (r->ll_symbols);
    r->ll_symbols_color = SYNTAX_COLOR_NONE;

    strcpy (whole_left, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");
    strcpy (whole_right, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");

    r->contexts = g_ptr_array_new_with_free_func (context_rule_free);

    if (r->defines == NULL)
        r->defines = g_tree_new ((GCompareFunc) strcmp);

    while (TRUE)
    {
        char **a;
        size_t len;
        int argc;

        line++;
        l = NULL;

        len = read_one_line (&l, f);
        if (len == 0)
        {
            if (g == NULL)
                break;

            fclose (f);
            f = g;
            g = NULL;
            line = save_line + 1;
            MC_PTR_FREE (error_file_name);
            MC_PTR_FREE (l);
            len = read_one_line (&l, f);
            if (len == 0)
                break;
        }

        xx_lowerize_line (r->case_insensitive, l, len);

        argc = get_args (l, args, args_size);
        a = args + 1;
        if (args[0] == NULL)
        {
            // do nothing
        }
        else if (strcmp (args[0], "line-local") == 0)
        {
            if (argc != 1 || r->contexts->len != 0)
                break_a;
            r->line_local = TRUE;
        }
        else if (strcmp (args[0], "number") == 0)
        {
            char *end;
            guint64 max;
            int syntax_color;

            if (!r->line_local || *a == NULL)
                break_a;

            errno = 0;
            max = g_ascii_strtoull (*a++, &end, 10);
            if (errno != 0 || *end != '\0' || max == 0 || max > G_MAXUINT)
                break_a;

            syntax_color = read_line_local_color (r, &a);
            if (syntax_color < 0)
                break_a;

            r->ll_number_max = (guint) max;
            r->ll_number_color = syntax_color;
            check_not_a;
        }
        else if (strcmp (args[0], "string") == 0)
        {
            char quote_char;
            int syntax_color;

            if (!r->line_local || *a == NULL || (*a)[1] != '\0')
                break_a;
            quote_char = **a;
            if (quote_char != '\'' && quote_char != '"')
                break_a;
            a++;

            syntax_color = read_line_local_color (r, &a);
            if (syntax_color < 0)
                break_a;

            if (quote_char == '\'')
                r->ll_single_quote_color = syntax_color;
            else
                r->ll_double_quote_color = syntax_color;
            check_not_a;
        }
        else if (strcmp (args[0], "symbols") == 0)
        {
            int syntax_color;

            if (!r->line_local || *a == NULL || **a == '\0')
                break_a;
            MC_PTR_FREE (r->ll_symbols);
            r->ll_symbols = g_strdup (*a++);

            syntax_color = read_line_local_color (r, &a);
            if (syntax_color < 0)
                break_a;

            r->ll_symbols_color = syntax_color;
            check_not_a;
        }
        else if (strcmp (args[0], "include") == 0)
        {
            if (g != NULL || argc != 2)
            {
                result = line;
                break;
            }
            g = f;
            f = open_include_file (args[1]);
            if (f == NULL)
            {
                MC_PTR_FREE (error_file_name);
                result = line;
                break;
            }
            save_line = line;
            line = 0;
        }
        else if (strcmp (args[0], "caseinsensitive") == 0)
        {
            r->case_insensitive = TRUE;
        }
        else if (strcmp (args[0], "wholechars") == 0)
        {
            check_a;
            if (strcmp (*a, "left") == 0)
            {
                a++;
                g_strlcpy (whole_left, *a, sizeof (whole_left));
            }
            else if (strcmp (*a, "right") == 0)
            {
                a++;
                g_strlcpy (whole_right, *a, sizeof (whole_right));
            }
            else
            {
                g_strlcpy (whole_left, *a, sizeof (whole_left));
                g_strlcpy (whole_right, *a, sizeof (whole_right));
            }
            a++;
            check_not_a;
        }
        else if (strcmp (args[0], "context") == 0)
        {
            syntax_keyword_t *k;

            if (r->line_local)
                break_a;
            check_a;
            if (r->contexts->len == 0)
            {
                // first context is the default
                if (strcmp (*a, "default") != 0)
                    break_a;

                a++;
                c = g_new0 (context_rule_t, 1);
                g_ptr_array_add (r->contexts, c);
                c->left = g_string_new (" ");
                c->right = g_string_new (" ");
            }
            else
            {
                // Start new context.
                c = g_new0 (context_rule_t, 1);
                g_ptr_array_add (r->contexts, c);
                if (strcmp (*a, "exclusive") == 0)
                {
                    a++;
                    c->between_delimiters = TRUE;
                }
                check_a;
                if (strcmp (*a, "whole") == 0)
                {
                    a++;
                    c->whole_word_chars_left = g_strdup (whole_left);
                    c->whole_word_chars_right = g_strdup (whole_right);
                }
                else if (strcmp (*a, "wholeleft") == 0)
                {
                    a++;
                    c->whole_word_chars_left = g_strdup (whole_left);
                }
                else if (strcmp (*a, "wholeright") == 0)
                {
                    a++;
                    c->whole_word_chars_right = g_strdup (whole_right);
                }
                check_a;
                if (strcmp (*a, "linestart") == 0)
                {
                    a++;
                    c->line_start_left = TRUE;
                }
                check_a;
                c->left = g_string_new (*a++);
                check_a;
                if (strcmp (*a, "linestart") == 0)
                {
                    a++;
                    c->line_start_right = TRUE;
                }
                check_a;
                c->right = g_string_new (*a++);
                c->first_left = c->left->str[0];
                c->first_right = c->right->str[0];
            }
            c->keyword = g_ptr_array_new_with_free_func (syntax_keyword_free);
            k = g_new0 (syntax_keyword_t, 1);
            g_ptr_array_add (c->keyword, k);
            no_words = FALSE;
            subst_defines (r->defines, a, &args[ARGS_LEN]);
            color.fg = *a;
            if (*a != NULL)
                a++;
            color.bg = *a;
            if (*a != NULL)
                a++;
            color.attrs = *a;
            if (*a != NULL)
                a++;
            g_strlcpy (last_fg, color.fg != NULL ? color.fg : "", sizeof (last_fg));
            g_strlcpy (last_bg, color.bg != NULL ? color.bg : "", sizeof (last_bg));
            g_strlcpy (last_attrs, color.attrs != NULL ? color.attrs : "", sizeof (last_attrs));
            k->color = syntax_intern_color (r, &color);
            k->keyword = g_string_new (" ");
            check_not_a;
        }
        else if (strcmp (args[0], "spellcheck") == 0)
        {
            if (r->line_local)
                break_a;
            if (c == NULL)
            {
                result = line;
                break;
            }
            c->spelling = TRUE;
        }
        else if (strcmp (args[0], "keyword") == 0)
        {
            context_rule_t *last_rule;
            syntax_keyword_t *k;

            if (r->line_local)
                break_a;
            if (no_words)
                break_a;
            check_a;
            last_rule = CONTEXT_RULE (g_ptr_array_index (r->contexts, r->contexts->len - 1));
            k = g_new0 (syntax_keyword_t, 1);
            g_ptr_array_add (last_rule->keyword, k);
            if (strcmp (*a, "whole") == 0)
            {
                a++;
                k->whole_word_chars_left = g_strdup (whole_left);
                k->whole_word_chars_right = g_strdup (whole_right);
            }
            else if (strcmp (*a, "wholeleft") == 0)
            {
                a++;
                k->whole_word_chars_left = g_strdup (whole_left);
            }
            else if (strcmp (*a, "wholeright") == 0)
            {
                a++;
                k->whole_word_chars_right = g_strdup (whole_right);
            }
            check_a;
            if (strcmp (*a, "linestart") == 0)
            {
                a++;
                k->line_start = TRUE;
            }
            check_a;
            if (strcmp (*a, "whole") == 0)
                break_a;

            k->keyword = g_string_new (*a++);
            subst_defines (r->defines, a, &args[ARGS_LEN]);
            color.fg = *a;
            if (*a != NULL)
                a++;
            color.bg = *a;
            if (*a != NULL)
                a++;
            color.attrs = *a;
            if (*a != NULL)
                a++;
            if (color.fg == NULL)
                color.fg = last_fg;
            if (color.bg == NULL)
                color.bg = last_bg;
            if (color.attrs == NULL)
                color.attrs = last_attrs;
            k->color = syntax_intern_color (r, &color);
            check_not_a;
        }
        else if (*(args[0]) == '#')
        {
            // do nothing for comment
        }
        else if (strcmp (args[0], "file") == 0)
        {
            break;
        }
        else if (strcmp (args[0], "define") == 0)
        {
            char *key = *a++;
            char **argv;

            if (argc < 3)
                break_a;
            argv = g_tree_lookup (r->defines, key);
            if (argv != NULL)
                mc_defines_destroy (NULL, argv, NULL);
            else
                key = g_strdup (key);

            argv = g_new (char *, argc - 1);
            g_tree_insert (r->defines, key, argv);
            while (*a != NULL)
                *argv++ = g_strdup (*a++);
            *argv = NULL;
        }
        else
        {
            // anything else is an error
            break_a;
        }
        MC_PTR_FREE (l);
    }
    MC_PTR_FREE (l);

    if (r->contexts->len == 0)
    {
        g_ptr_array_free (r->contexts, TRUE);
        r->contexts = NULL;
    }

    if (result == 0)
    {
        size_t i;
        GString *first_chars;

        if (r->contexts == NULL)
            return r->line_local ? 0 : line;

        first_chars = g_string_sized_new (32);

        // collect first character of keywords
        for (i = 0; i < r->contexts->len; i++)
        {
            size_t j;

            g_string_set_size (first_chars, 0);
            c = CONTEXT_RULE (g_ptr_array_index (r->contexts, i));

            g_string_append_c (first_chars, (char) 1);
            for (j = 1; j < c->keyword->len; j++)
            {
                syntax_keyword_t *k;

                k = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, j));
                g_string_append_c (first_chars, k->keyword->str[0]);
            }

            c->keyword_first_chars = g_strndup (first_chars->str, first_chars->len);
        }

        g_string_free (first_chars, TRUE);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* returns -1 on file error, line number on error in file syntax */
static int
edit_read_syntax_file (syntax_rules_t *r, GPtrArray *pnames, const char *syntax_file,
                       const char *editor_file, const char *first_line, const char *type)
{
    FILE *f, *g = NULL;
    char *args[ARGS_LEN], *l = NULL;
    long line = 0;
    int result = 0;
    gboolean found = FALSE;

    f = fopen (syntax_file, "r");
    if (f == NULL)
    {
        char *global_syntax_file;

        global_syntax_file =
            g_build_filename (mc_global.share_data_dir, EDIT_SYNTAX_FILE, (char *) NULL);
        f = fopen (global_syntax_file, "r");
        g_free (global_syntax_file);
        if (f == NULL)
            return -1;
    }

    args[0] = NULL;
    while (TRUE)
    {
        line++;
        MC_PTR_FREE (l);
        if (read_one_line (&l, f) == 0)
            break;
        (void) get_args (l, args, ARGS_LEN - 1);  // Final NULL
        if (args[0] == NULL)
            continue;

        // Looking for 'include ...' lines before first 'file ...' ones
        if (!found && strcmp (args[0], "include") == 0)
        {
            if (args[1] == NULL || (g = open_include_file (args[1])) == NULL)
            {
                result = line;
                break;
            }
            goto found_type;
        }

        // looking for 'file ...' lines only
        if (strcmp (args[0], "file") != 0)
            continue;

        found = TRUE;

        // must have two args or report error
        if (args[1] == NULL || args[2] == NULL)
        {
            result = line;
            break;
        }

        if (pnames != NULL)
        {
            // 1: just collecting a list of names of rule sets
            g_ptr_array_add (pnames, g_strdup (args[2]));
        }
        else if (type != NULL)
        {
            // 2: rule set was explicitly specified by the caller
            if (strcmp (type, args[2]) == 0)
                goto found_type;
        }
        else if (editor_file != NULL && r != NULL)
        {
            // 3: auto-detect rule set from regular expressions
            gboolean q;

            q = mc_search (args[1], NULL, editor_file, MC_SEARCH_T_REGEX);
            // does filename match arg 1 ?
            if (!q && args[3] != NULL)
            {
                // does first line match arg 3 ?
                q = mc_search (args[3], NULL, first_line, MC_SEARCH_T_REGEX);
            }
            if (q)
            {
                int line_error;
                char *syntax_type;

            found_type:
                syntax_type = args[2];
                line_error = edit_read_syntax_rules (r, g ? g : f, args, ARGS_LEN - 1);
                if (line_error != 0)
                {
                    if (error_file_name == NULL)  // an included file
                        result = line + line_error;
                    else
                        result = line_error;
                }
                else
                {
                    g_free (r->type);
                    r->type = g_strdup (syntax_type);
                    // if there are no rules then turn off syntax highlighting for speed
                    if (g == NULL && r->contexts != NULL && r->contexts->len == 1)
                    {
                        context_rule_t *r0;

                        r0 = CONTEXT_RULE (g_ptr_array_index (r->contexts, 0));
                        if (r0->keyword->len == 1 && !r0->spelling)
                        {
                            syntax_rules_clear (r);
                            break;
                        }
                    }
                }

                if (g == NULL)
                    break;

                fclose (g);
                g = NULL;
            }
        }
    }
    g_free (l);
    fclose (f);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_line_local_reset (syntax_line_local_state_t *state, off_t line_start)
{
    state->number_end = line_start;
    state->quote = '\0';
    state->quote_escaped = FALSE;
    state->number_overflow = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

guint
syntax_line_local_color (const syntax_rules_t *r, syntax_line_local_state_t *state,
                         syntax_get_byte_fn get_byte, void *data, off_t byte_index)
{
    const int c = get_byte (data, byte_index);

    if (c == '\n')
    {
        state->quote = '\0';
        state->quote_escaped = FALSE;
        state->number_end = byte_index;
        state->number_overflow = FALSE;
        return SYNTAX_COLOR_NONE;
    }

    if (state->number_overflow)
    {
        if (g_ascii_isdigit (c))
            return SYNTAX_COLOR_NONE;
        state->number_overflow = FALSE;
    }

    if (state->quote != '\0')
    {
        const guint color =
            state->quote == '\'' ? r->ll_single_quote_color : r->ll_double_quote_color;

        if (c == '\\' && !state->quote_escaped)
            state->quote_escaped = TRUE;
        else
        {
            if (c == state->quote && !state->quote_escaped)
                state->quote = '\0';
            state->quote_escaped = FALSE;
        }

        return color;
    }

    if (c == '\'' || c == '"')
    {
        state->quote = (unsigned char) c;
        state->quote_escaped = FALSE;
        return c == '\'' ? r->ll_single_quote_color : r->ll_double_quote_color;
    }

    if (byte_index < state->number_end)
        return r->ll_number_color;

    if (g_ascii_isdigit (c))
    {
        off_t end = byte_index;
        guint count = 0;

        do
        {
            end++;
            count++;
        }
        while (g_ascii_isdigit (get_byte (data, end)) && count <= r->ll_number_max);

        if (count <= r->ll_number_max)
        {
            state->number_end = end;
            return r->ll_number_color;
        }

        state->number_overflow = TRUE;
    }

    if (r->ll_symbols != NULL && strchr (r->ll_symbols, c) != NULL)
        return r->ll_symbols_color;

    return SYNTAX_COLOR_NONE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Newest checkpoint at or before the wanted byte, NULL if the walk starts over.
 *
 * The index is append-only and sorted, and moving backwards does not disturb it:
 * the state at an offset is a function of the bytes before it, so a checkpoint
 * stays true until the source itself changes.  (It used to be a stack that any
 * backward step popped, which made a jump back cost a rescan of everything above
 * it.)
 */

static const syntax_checkpoint_t *
syntax_checkpoint_find (const syntax_scanner_t *sc, off_t target)
{
    guint lo = 0, hi = sc->index->len;

    while (lo < hi)
    {
        const guint mid = (lo + hi) / 2;

        if (g_array_index (sc->index, syntax_checkpoint_t, mid).offset <= target)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo == 0 ? NULL : &g_array_index (sc->index, syntax_checkpoint_t, lo - 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
syntax_index_truncate (syntax_scanner_t *sc, off_t from)
{
    while (sc->index->len != 0
           && g_array_index (sc->index, syntax_checkpoint_t, sc->index->len - 1).offset >= from)
        g_array_set_size (sc->index, sc->index->len - 1);
}

/* --------------------------------------------------------------------------------------------- */

static void
syntax_get_rule (syntax_scanner_t *sc, off_t byte_index)
{
    off_t i;

    if (byte_index < sc->last)
    {
        const syntax_checkpoint_t *cp;

        cp = syntax_checkpoint_find (sc, byte_index);
        if (cp != NULL)
        {
            sc->rule = cp->rule;
            sc->last = cp->offset;
        }
        else
        {
            memset (&sc->rule, 0, sizeof (sc->rule));
            sc->last = -2;  // so that the walk below starts at -1, as it does from scratch
        }
    }

    for (i = sc->last + 1; i <= byte_index; i++)
    {
        off_t d = SYNTAX_MARKER_DENSITY;

        apply_rules_going_right (sc, i);

        if (sc->index->len != 0)
            d += g_array_index (sc->index, syntax_checkpoint_t, sc->index->len - 1).offset;

        if (i > d)
        {
            syntax_checkpoint_t cp;

            cp.offset = i;
            cp.rule = sc->rule;
            g_array_append_val (sc->index, cp);
        }
    }

    sc->last = byte_index;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
syntax_rules_load (const char *syntax_file, const syntax_select_t *sel, syntax_rules_t **rules,
                   char **error_file)
{
    syntax_rules_t *r;
    int res;

    if (error_file != NULL)
        *error_file = NULL;
    *rules = NULL;

    r = syntax_rules_new ();
    res = edit_read_syntax_file (r, NULL, syntax_file, sel->filename,
                                 sel->first_line != NULL ? sel->first_line : "", sel->type);

    if (res != 0 || (r->contexts == NULL && !r->line_local))
    {
        if (error_file != NULL)
        {
            *error_file = error_file_name;
            error_file_name = NULL;
        }
        syntax_rules_free (r);
        return res != 0 ? res : -1;
    }

    MC_PTR_FREE (error_file_name);
    *rules = r;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

int
syntax_rules_list_types (const char *syntax_file, GPtrArray *names)
{
    syntax_rules_t *r;
    int res;

    r = syntax_rules_new ();
    res = edit_read_syntax_file (r, names, syntax_file, NULL, "", NULL);
    syntax_rules_free (r);
    MC_PTR_FREE (error_file_name);

    return res;
}

/* --------------------------------------------------------------------------------------------- */

syntax_rules_t *
syntax_rules_ref (syntax_rules_t *r)
{
    if (r != NULL)
        r->refs++;

    return r;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_rules_unref (syntax_rules_t *r)
{
    if (r != NULL && --r->refs == 0)
        syntax_rules_free (r);
}

/* --------------------------------------------------------------------------------------------- */

const char *
syntax_rules_type (const syntax_rules_t *r)
{
    return r == NULL ? NULL : r->type;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
syntax_rules_is_line_local (const syntax_rules_t *r)
{
    return r != NULL && r->line_local;
}

/* --------------------------------------------------------------------------------------------- */

guint
syntax_rules_color_count (const syntax_rules_t *r)
{
    return r == NULL ? 0 : r->colors->len;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_rules_color_spec (const syntax_rules_t *r, guint color, const char **fg, const char **bg,
                         const char **attrs)
{
    const syntax_color_t *c;

    *fg = NULL;
    *bg = NULL;
    *attrs = NULL;

    if (r == NULL || color >= r->colors->len)
        return;

    c = &g_array_index (r->colors, syntax_color_t, color);
    *fg = c->fg;
    *bg = c->bg;
    *attrs = c->attrs;
}

/* --------------------------------------------------------------------------------------------- */

guint
syntax_rules_color_of (const syntax_rules_t *r, syntax_state_t st)
{
    const context_rule_t *c;
    const syntax_keyword_t *k;

    if (r == NULL || r->contexts == NULL || st.context >= r->contexts->len)
        return SYNTAX_COLOR_NONE;

    c = CONTEXT_RULE (g_ptr_array_index (r->contexts, st.context));
    if (c->keyword == NULL || st.keyword >= c->keyword->len)
        return SYNTAX_COLOR_NONE;

    k = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, st.keyword));

    return k->color;
}

/* --------------------------------------------------------------------------------------------- */

syntax_scanner_t *
syntax_scanner_new (syntax_rules_t *rules, syntax_get_byte_fn get_byte, void *data, off_t size)
{
    syntax_scanner_t *sc;

    if (rules == NULL || get_byte == NULL)
        return NULL;

    sc = g_new0 (syntax_scanner_t, 1);
    sc->rules = syntax_rules_ref (rules);
    sc->get_byte = get_byte;
    sc->data = data;
    sc->size = size;
    sc->index = g_array_new (FALSE, FALSE, sizeof (syntax_checkpoint_t));
    sc->last = -2;  // nothing walked yet; the first step is byte -1

    return sc;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_scanner_free (syntax_scanner_t *sc)
{
    if (sc == NULL)
        return;

    syntax_rules_unref (sc->rules);
    g_array_free (sc->index, TRUE);
    g_free (sc);
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_scanner_set_size (syntax_scanner_t *sc, off_t size)
{
    if (sc != NULL)
        sc->size = size;
}

/* --------------------------------------------------------------------------------------------- */

const syntax_rules_t *
syntax_scanner_rules (const syntax_scanner_t *sc)
{
    return sc == NULL ? NULL : sc->rules;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_scanner_reset (syntax_scanner_t *sc)
{
    if (sc == NULL)
        return;

    memset (&sc->rule, 0, sizeof (sc->rule));
    sc->last = -2;
    g_array_set_size (sc->index, 0);
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_notify_insert (syntax_scanner_t *sc, off_t pos, gboolean inclusive)
{
    if (sc == NULL)
        return;

    if (inclusive ? sc->last >= pos : sc->last > pos)
        sc->last++;
    syntax_index_truncate (sc, pos);
    sc->size++;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_notify_delete (syntax_scanner_t *sc, off_t pos, gboolean inclusive)
{
    if (sc == NULL)
        return;

    if (inclusive ? sc->last >= pos : sc->last > pos)
        sc->last--;
    syntax_index_truncate (sc, pos);
    if (sc->size > 0)
        sc->size--;
}

/* --------------------------------------------------------------------------------------------- */

syntax_state_t
syntax_state_at (syntax_scanner_t *sc, off_t byte_index)
{
    syntax_state_t st = { 0, 0 };

    if (sc == NULL || sc->rules->contexts == NULL || byte_index >= sc->size)
        return st;

    syntax_get_rule (sc, byte_index);
    st.context = sc->rule.context;
    st.keyword = sc->rule.keyword;

    return st;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_runs_for_range (syntax_scanner_t *sc, off_t from, off_t to, GArray *runs)
{
    off_t i;
    guint cur = 0;
    guint32 len = 0;
    const GPtrArray *contexts;

    if (sc == NULL || runs == NULL || from < 0)
        return;

    contexts = sc->rules->contexts;
    if (contexts == NULL)
        return;

    if (to > sc->size)
        to = sc->size;

    for (i = from; i < to; i++)
    {
        const context_rule_t *c;
        guint color;

        syntax_get_rule (sc, i);
        c = CONTEXT_RULE (g_ptr_array_index (contexts, sc->rule.context));
        color = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, sc->rule.keyword))->color;

        if (color != cur && len != 0)
        {
            syntax_run_t r = { len, cur };

            g_array_append_val (runs, r);
            len = 0;
        }

        cur = color;
        len++;
    }

    if (len != 0)
    {
        syntax_run_t r = { len, cur };

        g_array_append_val (runs, r);
    }
}

/* --------------------------------------------------------------------------------------------- */

syntax_palette_t *
syntax_palette_new (const syntax_rules_t *rules, syntax_color_alloc_fn alloc,
                    syntax_color_release_fn release, void *backend_data, int normal)
{
    syntax_palette_t *p;
    guint i;

    if (rules == NULL || alloc == NULL)
        return NULL;

    p = g_new0 (syntax_palette_t, 1);
    p->normal = normal;
    p->release = release;
    p->pairs = g_array_sized_new (FALSE, FALSE, sizeof (int), rules->colors->len);

    for (i = 0; i < rules->colors->len; i++)
    {
        const syntax_color_t *c = &g_array_index (rules->colors, syntax_color_t, i);
        int pair;

        pair = i == SYNTAX_COLOR_NONE ? normal : alloc (backend_data, c->fg, c->bg, c->attrs);
        g_array_append_val (p->pairs, pair);
    }

    return p;
}

/* --------------------------------------------------------------------------------------------- */

void
syntax_palette_free (syntax_palette_t *p)
{
    if (p == NULL)
        return;

    if (p->release != NULL)
    {
        guint i;

        for (i = 0; i < p->pairs->len; i++)
            if (i != SYNTAX_COLOR_NONE)
                p->release (g_array_index (p->pairs, int, i));
    }

    g_array_free (p->pairs, TRUE);
    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */

int
syntax_palette_get (const syntax_palette_t *p, guint color)
{
    if (p == NULL)
        return 0;
    if (color >= p->pairs->len)
        return p->normal;

    return g_array_index (p->pairs, int, color);
}

/* --------------------------------------------------------------------------------------------- */
