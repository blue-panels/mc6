/*
   mcstruct - turn a typed value back into the bytes of a field

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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lib/global.h"

#include "slv.h"
#include "slv_internal.h"
#include "slv_edit.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAX_EDIT_BYTES 4096

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
put_uint (unsigned char *b, int size, guint64 v, gboolean big_endian)
{
    int i;

    for (i = 0; i < size; i++)
    {
        int shift = big_endian ? (size - 1 - i) * 8 : i * 8;

        b[i] = (unsigned char) ((v >> shift) & 0xFF);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_int (const char *tok, gboolean allow_negative, gint64 *out)
{
    gboolean neg = FALSE;
    gint64 v;

    if (*tok == '-')
    {
        if (!allow_negative)
            return FALSE;
        neg = TRUE;
        tok++;
    }
    else if (*tok == '+')
        tok++;
    if (*tok == '\0')
        return FALSE;
    if (!slv_parse_number (tok, strlen (tok), &v))
        return FALSE;
    *out = neg ? -v : v;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* plain hex digits, with or without 0x */
static gboolean
parse_hex (const char *tok, guint64 *out)
{
    guint64 v = 0;
    int n = 0;

    if (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X'))
        tok += 2;
    if (*tok == '\0')
        return FALSE;
    for (; *tok != '\0'; tok++)
    {
        int d;

        if (!isxdigit ((unsigned char) *tok))
            return FALSE;
        d = isdigit ((unsigned char) *tok) ? *tok - '0' : tolower ((unsigned char) *tok) - 'a' + 10;
        v = (v << 4) | (guint64) d;
        if (++n > 16)
            return FALSE;
    }
    *out = v;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
fits (guint64 v, int size, gboolean is_signed, gint64 sv)
{
    if (size >= 8)
        return TRUE;
    if (is_signed)
    {
        gint64 lim = (gint64) 1 << (size * 8 - 1);

        return sv >= -lim && sv < lim;
    }
    return v < ((guint64) 1 << (size * 8));
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
encode_numbers (const slv_item_t *item, gboolean big_endian, gsize total, const char *text,
                unsigned char *out, char **error)
{
    char **toks;
    int size = item->size;
    gsize count = total / size, i, n = 0;

    toks = g_strsplit_set (text, " \t,", -1);
    for (i = 0; toks[i] != NULL; i++)
        if (toks[i][0] != '\0')
            n++;
    if (n != count)
    {
        *error = g_strdup_printf ("expected %u value(s), got %u", (unsigned) count, (unsigned) n);
        g_strfreev (toks);
        return FALSE;
    }

    n = 0;
    for (i = 0; toks[i] != NULL; i++)
    {
        const char *tok = toks[i];
        guint64 v = 0;
        gint64 sv = 0;
        gboolean ok;

        if (tok[0] == '\0')
            continue;
        switch (item->type)
        {
        case SLV_TYPE_INT:
            ok = parse_int (tok, TRUE, &sv) && fits ((guint64) sv, size, TRUE, sv);
            v = (guint64) sv;
            break;
        case SLV_TYPE_UINT:
            ok = parse_int (tok, FALSE, &sv) && fits ((guint64) sv, size, FALSE, sv);
            v = (guint64) sv;
            break;
        default: /* hex, big-endian hex, jump */
            ok = parse_hex (tok, &v) && fits (v, size, FALSE, 0);
            break;
        }
        if (!ok)
        {
            *error =
                g_strdup_printf ("bad value '%s' for %s", tok, slv_type_name (item->type, size));
            g_strfreev (toks);
            return FALSE;
        }
        put_uint (out + n * size, size, v, big_endian);
        n++;
    }
    g_strfreev (toks);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
encode_bits (const slv_item_t *item, const char *text, unsigned char *out, char **error)
{
    GString *bits = g_string_new (NULL);
    const char *p;
    guint64 v = 0;
    gboolean ok = TRUE;

    for (p = text; *p != '\0'; p++)
        if (*p == '0' || *p == '1')
            g_string_append_c (bits, *p);
        else if (*p != '.' && *p != ' ' && *p != '_')
        {
            ok = FALSE;
            break;
        }

    if (ok && bits->len == (gsize) item->size * 8)
    {
        for (p = bits->str; *p != '\0'; p++)
            v = (v << 1) | (guint64) (*p - '0');
    }
    else if (parse_hex (text, &v) && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')
             && fits (v, item->size, FALSE, 0))
        ok = TRUE;
    else
    {
        *error = g_strdup_printf ("expected %d bits or a 0x value", item->size * 8);
        ok = FALSE;
    }
    g_string_free (bits, TRUE);
    if (ok)
        put_uint (out, item->size, v, FALSE);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
encode_string (const slv_item_t *item, gsize total, const char *text, unsigned char *out,
               char **error)
{
    gsize len = strlen (text);

    memset (out, 0, total);
    switch (item->type)
    {
    case SLV_TYPE_PSTRING:
        if (total == 0 || len > total - 1 || len > 255)
        {
            *error = g_strdup_printf ("string longer than %u bytes",
                                      (unsigned) (total > 0 ? MIN (total - 1, 255) : 0));
            return FALSE;
        }
        out[0] = (unsigned char) len;
        memcpy (out + 1, text, len);
        return TRUE;
    default: /* char, C string */
        if (len > total)
        {
            *error = g_strdup_printf ("string longer than %u bytes", (unsigned) total);
            return FALSE;
        }
        memcpy (out, text, len);
        return TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
encode_unicode (const slv_item_t *item, gsize total, const char *text, unsigned char *out,
                char **error)
{
    memset (out, 0, total);
    if (!g_utf8_validate (text, -1, NULL))
    {
        *error = g_strdup ("text is not valid UTF-8");
        return FALSE;
    }
    if (item->type == SLV_TYPE_STR8)
    {
        gsize len = strlen (text);

        if (len > total)
        {
            *error = g_strdup_printf ("string longer than %u bytes", (unsigned) total);
            return FALSE;
        }
        memcpy (out, text, len);
        return TRUE;
    }
    {
        glong units = 0, i;
        gunichar2 *u;

        u = g_utf8_to_utf16 (text, -1, NULL, &units, NULL);
        if (u == NULL)
        {
            *error = g_strdup ("cannot convert to UTF-16");
            return FALSE;
        }
        if ((gsize) units * 2 > total)
        {
            *error = g_strdup_printf ("string longer than %u UTF-16 units", (unsigned) (total / 2));
            g_free (u);
            return FALSE;
        }
        for (i = 0; i < units; i++)
        {
            out[2 * i] = (unsigned char) (u[i] & 0xFF);
            out[2 * i + 1] = (unsigned char) (u[i] >> 8);
        }
        g_free (u);
        return TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
encode_float (const slv_item_t *item, gsize total, const char *text, unsigned char *out,
              char **error)
{
    char **toks;
    gsize count = total / item->size, i, n = 0;

    toks = g_strsplit_set (text, " \t,", -1);
    for (i = 0; toks[i] != NULL; i++)
    {
        char *end;
        double d;

        if (toks[i][0] == '\0')
            continue;
        errno = 0;
        d = g_ascii_strtod (toks[i], &end);
        if (*end != '\0' || errno != 0 || n >= count)
        {
            *error = g_strdup_printf ("bad float '%s'", toks[i]);
            g_strfreev (toks);
            return FALSE;
        }
        if (item->size == 4)
        {
            float f = (float) d;

            memcpy (out + n * 4, &f, 4);
        }
        else
            memcpy (out + n * 8, &d, 8);
        n++;
    }
    g_strfreev (toks);
    if (n != count)
    {
        *error = g_strdup_printf ("expected %u value(s)", (unsigned) count);
        return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
encode_ptr (const slv_item_t *item, gsize total, const char *text, unsigned char *out, char **error)
{
    char **toks;
    gsize count = total / item->size, i, n = 0;

    toks = g_strsplit_set (text, " \t,", -1);
    for (i = 0; toks[i] != NULL; i++)
    {
        char *colon;
        guint64 seg, off;
        int off_size = item->size == 4 ? 2 : 4;

        if (toks[i][0] == '\0')
            continue;
        colon = strchr (toks[i], ':');
        if (colon == NULL || n >= count)
            goto bad;
        *colon = '\0';
        if (!parse_hex (toks[i], &seg) || !parse_hex (colon + 1, &off) || !fits (seg, 2, FALSE, 0)
            || !fits (off, off_size, FALSE, 0))
            goto bad;
        put_uint (out + n * item->size, off_size, off, FALSE);
        put_uint (out + n * item->size + off_size, 2, seg, FALSE);
        n++;
        continue;
    bad:
        *error = g_strdup_printf ("expected SEG:OFF in hex, got '%s'", toks[i]);
        g_strfreev (toks);
        return FALSE;
    }
    g_strfreev (toks);
    if (n != count)
    {
        *error = g_strdup_printf ("expected %u value(s)", (unsigned) count);
        return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_datetime (const char *text, struct tm *tm)
{
    int y, mo, d, h = 0, mi = 0, s = 0, n;

    memset (tm, 0, sizeof (*tm));
    n = sscanf (text, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s);
    if (n < 3 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59
        || s < 0 || s > 59)
        return FALSE;
    tm->tm_year = y - 1900;
    tm->tm_mon = mo - 1;
    tm->tm_mday = d;
    tm->tm_hour = h;
    tm->tm_min = mi;
    tm->tm_sec = s;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
encode_time (const slv_item_t *item, gboolean big_endian, gsize total, const char *text,
             unsigned char *out, char **error)
{
    struct tm tm;
    guint64 v;

    if (total != 4)
    {
        *error = g_strdup ("only a single time value can be edited");
        return FALSE;
    }
    if (!parse_datetime (text, &tm))
    {
        *error = g_strdup ("expected YYYY-MM-DD HH:MM:SS");
        return FALSE;
    }
    if (item->type == SLV_TYPE_TIME_DOS)
    {
        if (tm.tm_year + 1900 < 1980 || tm.tm_year + 1900 > 2107)
        {
            *error = g_strdup ("DOS dates run from 1980 to 2107");
            return FALSE;
        }
        v = ((guint64) ((tm.tm_year + 1900 - 1980) << 9 | (tm.tm_mon + 1) << 5 | tm.tm_mday) << 16)
            | (guint64) (tm.tm_hour << 11 | tm.tm_min << 5 | tm.tm_sec / 2);
    }
    else
    {
        time_t t = timegm (&tm);

        if (t == (time_t) -1 || t < 0 || t > 0xFFFFFFFFLL)
        {
            *error = g_strdup ("time is outside the 32-bit range");
            return FALSE;
        }
        v = (guint64) t;
    }
    put_uint (out, 4, v, big_endian);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
slv_node_editable (const slv_node_t *node)
{
    const slv_item_t *item;

    if (node == NULL || node->item == NULL || node->size <= 0 || node->size > MAX_EDIT_BYTES)
        return FALSE;
    if (node->kind != SLV_NODE_FIELD && node->kind != SLV_NODE_JUMP)
        return FALSE;
    item = node->item;
    switch (item->type)
    {
    case SLV_TYPE_MSBIN:
    case SLV_TYPE_CHECK:
    case SLV_TYPE_VARINT:
    case SLV_TYPE_LEB128:
    case SLV_TYPE_ZVARINT:
    case SLV_TYPE_STR16Z:
        return FALSE;
    case SLV_TYPE_FLOAT:
        return item->size != 10;
    case SLV_TYPE_TIME_DOS:
    case SLV_TYPE_TIME_UNIX:
        return node->size == 4;
    case SLV_TYPE_TIME_FILE:
        return FALSE;
    default:
        return TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* the bytes of a string field as text; NULL when they are not text */
static char *
string_edit_text (const slv_item_t *item, const unsigned char *raw, gsize size)
{
    gsize start = 0, len;
    char *s;

    if (item->type == SLV_TYPE_STR16)
    {
        glong n = (glong) (size / 2), i;

        for (i = 0; i < n; i++)
            if (raw[2 * i] == 0 && raw[2 * i + 1] == 0)
                break;
        s = g_utf16_to_utf8 ((const gunichar2 *) raw, i, NULL, NULL, NULL);
        if (s == NULL)
            return NULL;
    }
    else
    {
        if (item->type == SLV_TYPE_PSTRING)
        {
            if (size == 0)
                return g_strdup ("");
            start = 1;
            len = MIN ((gsize) raw[0], size - 1);
        }
        else
        {
            len = 0;
            while (len < size && raw[len] != '\0')
                len++;
        }
        s = g_strndup ((const char *) raw + start, len);
        if (!g_utf8_validate (s, -1, NULL))
        {
            g_free (s);
            return NULL;
        }
    }
    for (len = 0; s[len] != '\0'; len++)
        if ((unsigned char) s[len] < 0x20 || s[len] == 0x7F)
        {
            g_free (s);
            return NULL;
        }
    return s;
}

/* --------------------------------------------------------------------------------------------- */

char *
slv_node_edit_text (const slv_node_t *node, const unsigned char *raw)
{
    if (node == NULL || node->text == NULL)
        return g_strdup ("");
    if (raw != NULL && node->item != NULL)
        switch (node->item->type)
        {
        case SLV_TYPE_CHAR:
        case SLV_TYPE_CSTRING:
        case SLV_TYPE_PSTRING:
        case SLV_TYPE_STR8:
        case SLV_TYPE_STR16:
            return string_edit_text (node->item, raw, (gsize) node->size);
        case SLV_TYPE_FLOAT:
        {
            gint64 iv;
            double dv;

            /* enough digits to read the same value back */
            return slv_format_value_endian (node->item, node->big_endian, raw, (gsize) node->size,
                                            node->item->size == 4 ? "%.9g" : "%.17g", &iv, &dv);
        }
        default:
            break;
        }
    return g_strdup (node->text);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
slv_node_encode (const slv_node_t *node, const char *text, unsigned char *out, char **error)
{
    const slv_item_t *item;
    gsize total;
    char *stripped;
    gboolean ok;

    *error = NULL;
    if (!slv_node_editable (node))
    {
        *error = g_strdup ("this field cannot be edited");
        return FALSE;
    }
    item = node->item;
    total = (gsize) node->size;
    stripped = g_strstrip (g_strdup (text));

    switch (item->type)
    {
    case SLV_TYPE_CHAR:
    case SLV_TYPE_PSTRING:
    case SLV_TYPE_CSTRING:
        ok = encode_string (item, total, text, out, error);
        break;
    case SLV_TYPE_BITS:
        ok = encode_bits (item, stripped, out, error);
        break;
    case SLV_TYPE_FLOAT:
        ok = encode_float (item, total, stripped, out, error);
        break;
    case SLV_TYPE_PTR:
        ok = encode_ptr (item, total, stripped, out, error);
        break;
    case SLV_TYPE_TIME_DOS:
    case SLV_TYPE_TIME_UNIX:
        ok = encode_time (item, node->big_endian, total, stripped, out, error);
        break;
    case SLV_TYPE_STR8:
    case SLV_TYPE_STR16:
        ok = encode_unicode (item, total, text, out, error);
        break;
    default:
        ok = encode_numbers (item, node->big_endian, total, stripped, out, error);
        break;
    }
    g_free (stripped);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */
