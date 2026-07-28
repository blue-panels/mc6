/*
   libshfs - continuing a transfer that stopped part way.

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

/**
 * \file
 * \brief Source: how much of a half-written file can be kept
 *
 * A transfer that stopped leaves a prefix, not rubbish - but only if somebody
 * checks that it really is a prefix. Appending to an unverified remnant
 * produces a file of the right size and the right date with garbage in the
 * middle, and nothing ever says so. Everything here exists to make that
 * check, and to find how much is good when part of it is not.
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "lib/global.h"
#include "lib/util.h"

#include "shfs.h"
#include "shfs-priv.h"

/*** file scope macro definitions ****************************************************************/

#define RESUME_BUF (64 * 1024)

/* Enough blocks to place the divergence closely, few enough to keep the reply
   small. */
#define RESUME_BLOCKS 1024

/* Floor on the block size; it gives way when honouring it would leave a single
   block. */
#define RESUME_MIN_BLOCK (4 * 1024)

/* The damage is almost always in the tail, so backing off by a quarter usually
   lands on good data within one or two steps. */
#define RESUME_BACKOFF_NUM 3
#define RESUME_BACKOFF_DEN 4

/* An exact boundary is not needed, any correct prefix will do; after three
   halvings the tail thrown away for nothing is under n/32. */
#define RESUME_BISECT_STEPS 3

/*** file scope variables ************************************************************************/

/* POSIX cksum: CRC-32 over the data, then over the byte count, complemented.
   GLib has no such thing, and it is the only digest a bare POSIX host is
   guaranteed to offer. */
static guint32 cksum_table[256];
static gboolean cksum_table_ready = FALSE;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
cksum_init (void)
{
    guint32 i;

    if (cksum_table_ready)
        return;

    for (i = 0; i < 256; i++)
    {
        guint32 c = i << 24;
        int k;

        for (k = 0; k < 8; k++)
            c = (c & 0x80000000u) != 0 ? (c << 1) ^ 0x04C11DB7u : (c << 1);

        cksum_table[i] = c;
    }

    cksum_table_ready = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static guint32
cksum_update (guint32 crc, const unsigned char *buf, gsize len)
{
    gsize i;

    for (i = 0; i < len; i++)
        crc = (crc << 8) ^ cksum_table[((crc >> 24) ^ buf[i]) & 0xff];

    return crc;
}

/* --------------------------------------------------------------------------------------------- */

static guint32
cksum_finish (guint32 crc, gint64 total)
{
    gint64 len = total;

    /* POSIX: the length is folded in one byte at a time, low byte first. */
    while (len > 0)
    {
        unsigned char c = (unsigned char) (len & 0xff);

        crc = cksum_update (crc, &c, 1);
        len >>= 8;
    }

    return ~crc;
}

/* --------------------------------------------------------------------------------------------- */

static GChecksumType
glib_algo (shfs_digest_algo_t algo)
{
    return algo == SHFS_DIGEST_SHA256 ? G_CHECKSUM_SHA256 : G_CHECKSUM_MD5;
}

/* --------------------------------------------------------------------------------------------- */

/** Whether the first @len bytes are the same on both sides. */
static gboolean
shfs_prefix_matches (shfs_conn_t *conn, const char *remote, const char *local, gint64 len,
                     shfs_digest_algo_t algo, gboolean *matched, GError **error)
{
    shfs_digest_t rd;
    shfs_digest_t ld;

    *matched = FALSE;

    if (len == 0)
    {
        *matched = TRUE;
        return TRUE;
    }

    if (!shfs_checksum_range (conn, remote, 0, len, algo, &rd, error))
        return FALSE;

    if (!shfs_local_digest_range (local, 0, len, algo, &ld, error))
        return FALSE;

    *matched = (strcmp (rd.hex, ld.hex) == 0);

    shfs_log_printf (SHFS_LOG_COMMANDS, "prefix of %" G_GINT64_FORMAT " bytes: %s", len,
                     *matched ? "matches" : "differs");

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Locate the divergence with one pass over each side.
 *
 * Requires perl on the host. Returns -1 when the answer could not be had,
 * which is not an error: the caller falls back to probing.
 */
static gint64
shfs_diverge_by_blocks (shfs_conn_t *conn, const char *remote, const char *local, gint64 n,
                        shfs_digest_algo_t algo)
{
    GPtrArray *rblocks = NULL;
    gint64 block;
    gint64 pos;
    guint i;
    gint64 result = -1;

    if ((conn->host_flags & SHELL_HAVE_PERL) == 0)
        return -1;

    /* The floor must give way at n/2: a single block that differs locates
       nothing. */
    block = (n + RESUME_BLOCKS - 1) / RESUME_BLOCKS;
    if (block < RESUME_MIN_BLOCK)
        block = RESUME_MIN_BLOCK;
    if (block > n / 2)
        block = n / 2;
    if (block < 1)
        block = 1;

    if (!shfs_block_digests (conn, remote, 0, n, block, algo, &rblocks, NULL))
        return -1;

    for (i = 0, pos = 0; i < rblocks->len && pos < n; i++, pos += block)
    {
        const shfs_digest_t *rd = (const shfs_digest_t *) g_ptr_array_index (rblocks, i);
        shfs_digest_t ld;
        gint64 this_block = MIN (block, n - pos);

        if (!shfs_local_digest_range (local, pos, this_block, algo, &ld, NULL))
        {
            result = -1;
            goto out;
        }

        if (strcmp (rd->hex, ld.hex) != 0)
        {
            result = pos;
            goto out;
        }
    }

    /* Every block agrees although the digest of the whole range disagreed:
       the helper is not printing what it claims, so neither answer can be
       trusted. Let the caller probe instead. */
    result = -1;

out:
    g_ptr_array_unref (rblocks);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Find some correct prefix by probing, for hosts without perl.
 *
 * Back off by a quarter until a probe agrees, then close the bracket by
 * halving a few times. The result is a length that is certainly good, not the
 * exact boundary.
 */
static gint64
shfs_diverge_by_probing (shfs_conn_t *conn, const char *remote, const char *local, gint64 n,
                         shfs_digest_algo_t algo)
{
    gint64 good = 0;
    gint64 bad = n;
    gint64 probe = n;
    int step;

    while (probe > 0)
    {
        gboolean matched;

        probe = probe * RESUME_BACKOFF_NUM / RESUME_BACKOFF_DEN;
        if (probe <= 0)
            break;

        if (!shfs_prefix_matches (conn, remote, local, probe, algo, &matched, NULL))
            return 0;

        if (matched)
        {
            good = probe;
            break;
        }

        bad = probe;
    }

    for (step = 0; step < RESUME_BISECT_STEPS && bad - good > 1; step++)
    {
        gint64 mid = good + (bad - good) / 2;
        gboolean matched;

        if (!shfs_prefix_matches (conn, remote, local, mid, algo, &matched, NULL))
            break;

        if (matched)
            good = mid;
        else
            bad = mid;
    }

    return good;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_local_digest_range (const char *path, gint64 offset, gint64 length, shfs_digest_algo_t algo,
                         shfs_digest_t *digest, GError **error)
{
    char buf[RESUME_BUF];
    GChecksum *sum = NULL;
    guint32 crc = 0;
    gint64 left = length;
    int fd;
    gboolean ok = TRUE;

    memset (digest, 0, sizeof (*digest));
    digest->algo = algo;

    fd = open (path, O_RDONLY);
    if (fd < 0)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_NOT_FOUND, _ ("shfs: cannot read %s: %s"), path,
                     unix_error_string (errno));
        return FALSE;
    }

    if (offset > 0 && lseek (fd, (off_t) offset, SEEK_SET) == (off_t) -1)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, _ ("shfs: cannot seek in %s: %s"), path,
                     unix_error_string (errno));
        close (fd);
        return FALSE;
    }

    if (algo == SHFS_DIGEST_CKSUM)
        cksum_init ();
    else
        sum = g_checksum_new (glib_algo (algo));

    while (left > 0)
    {
        gsize want = (gsize) MIN ((gint64) sizeof (buf), left);
        ssize_t n;

        n = read (fd, buf, want);
        if (n < 0)
        {
            g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, _ ("shfs: cannot read %s: %s"), path,
                         unix_error_string (errno));
            ok = FALSE;
            break;
        }
        if (n == 0)
        {
            g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED,
                         _ ("shfs: %s is shorter than the range asked for"), path);
            ok = FALSE;
            break;
        }

        if (algo == SHFS_DIGEST_CKSUM)
            crc = cksum_update (crc, (const unsigned char *) buf, (gsize) n);
        else
            g_checksum_update (sum, (const guchar *) buf, (gssize) n);

        left -= n;
    }

    close (fd);

    if (ok)
    {
        if (algo == SHFS_DIGEST_CKSUM)
            g_snprintf (digest->hex, sizeof (digest->hex), "%u", cksum_finish (crc, length));
        else
            g_strlcpy (digest->hex, g_checksum_get_string (sum), sizeof (digest->hex));
    }

    if (sum != NULL)
        g_checksum_free (sum);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

gint64
shfs_resume_probe (shfs_conn_t *conn, const char *remote, const char *local, gint64 source_size,
                   gint64 dest_size, GError **error)
{
    shfs_digest_algo_t algo;
    gboolean matched;
    gint64 n = dest_size;

    mc_return_val_if_error (error, -1);

    if (conn == NULL || !conn->alive)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_CLOSED, "%s", _ ("shell: connection is closed"));
        return -1;
    }

    if (n <= 0)
        return 0;

    shfs_log_printf (SHFS_LOG_COMMANDS,
                     "resume asked: destination %" G_GINT64_FORMAT
                     " bytes, source %" G_GINT64_FORMAT,
                     n, source_size);

    /* Nothing here truncates, so a destination at least as long as the source
       cannot be resumed: writing from zero would leave the old tail past the
       end. */
    if (n >= source_size)
    {
        shfs_log_printf (SHFS_LOG_ERRORS,
                         "cannot continue: the destination is not shorter than the source");
        return -1;
    }

    algo = shfs_conn_digest_algos (conn);
    if ((algo & SHFS_DIGEST_SHA256) != 0)
        algo = SHFS_DIGEST_SHA256;
    else if ((algo & SHFS_DIGEST_MD5) != 0)
        algo = SHFS_DIGEST_MD5;
    else
        algo = SHFS_DIGEST_CKSUM;

    shfs_log_printf (SHFS_LOG_COMMANDS,
                     "resume probe: %" G_GINT64_FORMAT " of %" G_GINT64_FORMAT " bytes present,"
                     " checking with %s",
                     n, source_size,
                     algo == SHFS_DIGEST_SHA256    ? "sha256"
                         : algo == SHFS_DIGEST_MD5 ? "md5"
                                                   : "cksum");

    if (!shfs_prefix_matches (conn, remote, local, n, algo, &matched, error))
        return -1;

    if (matched)
        return n;

    {
        gint64 k;

        k = shfs_diverge_by_blocks (conn, remote, local, n, algo);
        if (k < 0)
            k = shfs_diverge_by_probing (conn, remote, local, n, algo);

        shfs_log_printf (SHFS_LOG_COMMANDS, "diverges; keeping %" G_GINT64_FORMAT " bytes", k);

        return k;
    }
}

/* --------------------------------------------------------------------------------------------- */
