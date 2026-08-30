/*
   mcstruct: call() providers for def-file expressions

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

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

/* call('name', args...) in an expression gives a value; "* n Struct via
   call('name', args...)" gives the bytes a nested structure is read from.
   Built-in providers work on a range of the file; "exec:command" pipes the
   range through a program and is allowed only for trusted def-files. */

#include <config.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "slv.h"
#include "slv_internal.h"

/*** file scope macro definitions ****************************************************************/

#define CALL_MAX_BYTES (64 * 1024 * 1024)

/*** file scope functions ************************************************************************/

static GBytes *
read_range (const slv_eval_ctx_t *ctx, gint64 from, gint64 len, char **error)
{
    off_t size = ctx->ev->reader->size (ctx->ev->reader->ctx);
    unsigned char *buf;

    if (from < 0 || len < 0 || from > size || len > size - from)
    {
        *error = g_strdup_printf ("range %lld+%lld is outside the file", (long long) from,
                                  (long long) len);
        return NULL;
    }
    if (len > CALL_MAX_BYTES)
    {
        *error = g_strdup_printf ("range of %lld bytes is too large", (long long) len);
        return NULL;
    }
    buf = g_malloc ((gsize) len + 1);
    if (len > 0 && !slv_read_bytes (ctx->ev->reader, (off_t) from, buf, (gsize) len))
    {
        g_free (buf);
        *error = g_strdup ("read past end of file");
        return NULL;
    }
    return g_bytes_new_take (buf, (gsize) len);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
need_args (const char *name, guint nargs, guint want, char **error)
{
    if (nargs == want)
        return TRUE;
    *error = g_strdup_printf ("call('%s') takes %u arguments", name, want);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static guint16
crc16_ccitt (const unsigned char *b, gsize len)
{
    guint16 crc = 0xFFFF;
    gsize i;
    int k;

    for (i = 0; i < len; i++)
    {
        crc ^= (guint16) b[i] << 8;
        for (k = 0; k < 8; k++)
            crc = (crc & 0x8000) != 0 ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

/* --------------------------------------------------------------------------------------------- */

static guint32
adler32_of (const unsigned char *b, gsize len)
{
    guint32 a = 1, s = 0;
    gsize i;

    for (i = 0; i < len; i++)
    {
        a = (a + b[i]) % 65521;
        s = (s + a) % 65521;
    }
    return (s << 16) | a;
}

/* --------------------------------------------------------------------------------------------- */

/* the range on stdin of @command, its stdout back */
static GBytes *
exec_range (const slv_eval_ctx_t *ctx, const char *command, gint64 from, gint64 len, char **error)
{
    GBytes *in;
    const unsigned char *src;
    gsize src_len, sent = 0;
    GByteArray *out;
    GError *gerr = NULL;
    gchar *argv[4];
    GPid pid;
    int fd_in = -1, fd_out = -1, status = 0;
    unsigned char buf[65536];

    if (ctx->ev->trust < 1)
    {
        *error = g_strdup ("exec: providers are allowed for def-files in the user directory only");
        return NULL;
    }
    in = read_range (ctx, from, len, error);
    if (in == NULL)
        return NULL;
    src = g_bytes_get_data (in, &src_len);
    argv[0] = (gchar *) "/bin/sh";
    argv[1] = (gchar *) "-c";
    argv[2] = (gchar *) command;
    argv[3] = NULL;
    if (!g_spawn_async_with_pipes (NULL, argv, NULL,
                                   G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL, NULL,
                                   NULL, &pid, &fd_in, &fd_out, NULL, &gerr))
    {
        *error = g_strdup (gerr->message);
        g_error_free (gerr);
        g_bytes_unref (in);
        return NULL;
    }
    /* small inputs go in whole before stdout is read; a program that fills its output pipe
       before reading all of its input would block, so the input is capped */
    out = g_byte_array_new ();
    if (src_len > 65536)
        *error = g_strdup ("exec: input is limited to 64 KiB");
    while (*error == NULL && sent < src_len)
    {
        gssize w = write (fd_in, src + sent, src_len - sent);

        if (w < 0)
        {
            if (errno == EINTR)
                continue;
            *error = g_strdup_printf ("exec: %s", g_strerror (errno));
            break;
        }
        sent += (gsize) w;
    }
    close (fd_in);
    for (;;)
    {
        gssize r = read (fd_out, buf, sizeof (buf));

        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            break;
        if (out->len + (guint) r > CALL_MAX_BYTES)
        {
            if (*error == NULL)
                *error = g_strdup ("exec: output is too large");
            break;
        }
        g_byte_array_append (out, buf, (guint) r);
    }
    close (fd_out);
    waitpid (pid, &status, 0);
    g_spawn_close_pid (pid);
    g_bytes_unref (in);
    if (*error == NULL && (!WIFEXITED (status) || WEXITSTATUS (status) != 0))
        *error = g_strdup_printf ("'%s' failed", command);
    if (*error != NULL)
    {
        g_byte_array_free (out, TRUE);
        return NULL;
    }
    return g_byte_array_free_to_bytes (out);
}

/* --------------------------------------------------------------------------------------------- */

/* zlib (wbits 15), gzip (31) or raw deflate (-15) data of a range */
static GBytes *
inflate_range (const slv_eval_ctx_t *ctx, int wbits, gint64 from, gint64 len, char **error)
{
#ifdef HAVE_ZLIB
    GBytes *in = read_range (ctx, from, len, error);
    z_stream z;
    GByteArray *out;
    unsigned char buf[65536];
    int rc;

    if (in == NULL)
        return NULL;
    memset (&z, 0, sizeof (z));
    if (inflateInit2 (&z, wbits) != Z_OK)
    {
        *error = g_strdup ("inflateInit failed");
        g_bytes_unref (in);
        return NULL;
    }
    z.next_in = (Bytef *) g_bytes_get_data (in, NULL);
    z.avail_in = (uInt) g_bytes_get_size (in);
    out = g_byte_array_new ();
    do
    {
        z.next_out = buf;
        z.avail_out = sizeof (buf);
        rc = inflate (&z, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END)
        {
            *error = g_strdup_printf ("inflate: %s", z.msg != NULL ? z.msg : "bad data");
            break;
        }
        if (out->len + (sizeof (buf) - z.avail_out) > CALL_MAX_BYTES)
        {
            *error = g_strdup ("inflated data is too large");
            break;
        }
        g_byte_array_append (out, buf, (guint) (sizeof (buf) - z.avail_out));
    }
    while (rc != Z_STREAM_END && (z.avail_in > 0 || z.avail_out == 0));
    inflateEnd (&z);
    g_bytes_unref (in);
    if (*error != NULL)
    {
        g_byte_array_free (out, TRUE);
        return NULL;
    }
    return g_byte_array_free_to_bytes (out);
#else
    (void) ctx;
    (void) wbits;
    (void) from;
    (void) len;
    *error = g_strdup ("inflate: mcstruct was built without zlib");
    return NULL;
#endif
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* value providers:
   crc32 / crc16 / adler32 / sum8 / sum16 / sum32 (from, len): checksums of a range
   crc32z (from, len, hole_from, hole_len): crc32 with a field inside taken as zero
   find (from, len, value, width): offset of the little-endian value, -1 when absent
   exec:command (from, len): the range on stdin, a number on stdout */
gboolean
slv_call_value (const slv_eval_ctx_t *ctx, const char *name, const gint64 *args, guint nargs,
                gint64 *out, char **error)
{
    GBytes *data;
    const unsigned char *b;
    gsize len, i;

    *out = 0;
    if (g_str_has_prefix (name, "exec:"))
    {
        char *text;

        if (!need_args (name, nargs, 2, error))
            return FALSE;
        data = exec_range (ctx, name + 5, args[0], args[1], error);
        if (data == NULL)
            return FALSE;
        text = g_strndup (g_bytes_get_data (data, &len), len);
        *out = g_ascii_strtoll (g_strstrip (text), NULL, 0);
        g_free (text);
        g_bytes_unref (data);
        return TRUE;
    }
    if (strcmp (name, "find") == 0)
    {
        unsigned char pat[8];
        int width, k;

        if (!need_args (name, nargs, 4, error))
            return FALSE;
        width = (int) args[3];
        if (width < 1 || width > 8)
        {
            *error = g_strdup ("find: width must be 1..8");
            return FALSE;
        }
        for (k = 0; k < width; k++)
            pat[k] = (unsigned char) (args[2] >> (8 * k));
        data = read_range (ctx, args[0], args[1], error);
        if (data == NULL)
            return FALSE;
        b = g_bytes_get_data (data, &len);
        *out = -1;
        for (i = 0; i + width <= len; i++)
            if (memcmp (b + i, pat, width) == 0)
            {
                *out = args[0] + (gint64) i;
                break;
            }
        g_bytes_unref (data);
        return TRUE;
    }
    /* crc32z (from, len, hole_from, hole_len): crc32 with the hole taken as zero bytes */
    if (strcmp (name, "crc32z") == 0)
    {
        unsigned char *b2;
        gint64 h0, h1;

        if (!need_args (name, nargs, 4, error))
            return FALSE;
        data = read_range (ctx, args[0], args[1], error);
        if (data == NULL)
            return FALSE;
        b2 = g_bytes_unref_to_data (data, &len);
        h0 = MAX (args[2] - args[0], 0);
        h1 = MIN (args[2] + args[3] - args[0], (gint64) len);
        if (h0 < h1)
            memset (b2 + h0, 0, (gsize) (h1 - h0));
        *out = slv_crc32 (0xFFFFFFFFu, b2, len) ^ 0xFFFFFFFFu;
        g_free (b2);
        return TRUE;
    }
    if (strcmp (name, "crc32") == 0 || strcmp (name, "crc16") == 0 || strcmp (name, "adler32") == 0
        || strcmp (name, "sum8") == 0 || strcmp (name, "sum16") == 0 || strcmp (name, "sum32") == 0)
    {
        guint64 sum = 0;

        if (!need_args (name, nargs, 2, error))
            return FALSE;
        data = read_range (ctx, args[0], args[1], error);
        if (data == NULL)
            return FALSE;
        b = g_bytes_get_data (data, &len);
        if (strcmp (name, "crc32") == 0)
            *out = slv_crc32 (0xFFFFFFFFu, b, len) ^ 0xFFFFFFFFu;
        else if (strcmp (name, "crc16") == 0)
            *out = crc16_ccitt (b, len);
        else if (strcmp (name, "adler32") == 0)
            *out = adler32_of (b, len);
        else
        {
            for (i = 0; i < len; i++)
                sum += b[i];
            *out = (gint64) (sum & (name[3] == '8' ? 0xFF : name[4] == '1' ? 0xFFFF : 0xFFFFFFFFu));
        }
        g_bytes_unref (data);
        return TRUE;
    }
    *error = g_strdup_printf ("unknown call provider '%s'", name);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* byte providers, for "via":
   raw (from, len): the range as it is
   inflate / gunzip / deflate (from, len): zlib, gzip or raw deflate data
   xor (from, len, key): every byte xor key
   rol (from, len, bits): every byte rotated left
   exec:command (from, len): the range on stdin, the bytes on stdout */
GBytes *
slv_call_bytes (const slv_eval_ctx_t *ctx, const char *name, const gint64 *args, guint nargs,
                char **error)
{
    GBytes *data;

    if (g_str_has_prefix (name, "exec:"))
    {
        if (!need_args (name, nargs, 2, error))
            return NULL;
        return exec_range (ctx, name + 5, args[0], args[1], error);
    }
    if (strcmp (name, "raw") == 0)
    {
        if (!need_args (name, nargs, 2, error))
            return NULL;
        return read_range (ctx, args[0], args[1], error);
    }
    if (strcmp (name, "inflate") == 0 || strcmp (name, "gunzip") == 0
        || strcmp (name, "deflate") == 0)
    {
        if (!need_args (name, nargs, 2, error))
            return NULL;
        return inflate_range (ctx,
                              name[0] == 'i'       ? 15
                                  : name[0] == 'g' ? 31
                                                   : -15,
                              args[0], args[1], error);
    }
    if (strcmp (name, "xor") == 0 || strcmp (name, "rol") == 0)
    {
        unsigned char *b;
        gsize len, i;
        int k;

        if (!need_args (name, nargs, 3, error))
            return NULL;
        data = read_range (ctx, args[0], args[1], error);
        if (data == NULL)
            return NULL;
        b = g_bytes_unref_to_data (data, &len);
        k = (int) (args[2] & 7);
        for (i = 0; i < len; i++)
            b[i] = name[0] == 'x' ? b[i] ^ (unsigned char) args[2]
                                  : (unsigned char) ((b[i] << k) | (b[i] >> (8 - k)));
        return g_bytes_new_take (b, len);
    }
    *error = g_strdup_printf ("unknown byte provider '%s'", name);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
