/*
   mcstruct - file reader with an overlay of unsaved byte edits

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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "slv.h"
#include "slv_reader.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    off_t offset;
    unsigned char value;
} change_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
change_cmp (gconstpointer a, gconstpointer b)
{
    const change_t *ca = a, *cb = b;

    return ca->offset < cb->offset ? -1 : ca->offset > cb->offset ? 1 : 0;
}

/* --------------------------------------------------------------------------------------------- */

/* index of the change at offset, or -1 */
static int
find_change (const slv_file_reader_t *fr, off_t offset)
{
    guint lo = 0, hi = fr->changes->len;

    while (lo < hi)
    {
        guint mid = (lo + hi) / 2;
        const change_t *c = &g_array_index (fr->changes, change_t, mid);

        if (c->offset == offset)
            return (int) mid;
        if (c->offset < offset)
            lo = mid + 1;
        else
            hi = mid;
    }
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static gssize
reader_read (void *ctx, off_t offset, void *buf, gsize len)
{
    slv_file_reader_t *fr = ctx;
    gssize got;
    guint i;

    if (offset < 0 || offset >= fr->size)
        return 0;
    if ((off_t) len > fr->size - offset)
        len = (gsize) (fr->size - offset);

    got = 0;
    while ((gsize) got < len)
    {
        gssize n = pread (fr->fd, (unsigned char *) buf + got, len - (gsize) got, offset + got);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            break;
        got += n;
    }

    /* overlay the pending edits */
    for (i = 0; i < fr->changes->len; i++)
    {
        const change_t *c = &g_array_index (fr->changes, change_t, i);

        if (c->offset < offset)
            continue;
        if (c->offset >= offset + got)
            break;
        ((unsigned char *) buf)[c->offset - offset] = c->value;
    }
    return got;
}

/* --------------------------------------------------------------------------------------------- */

static off_t
reader_size (void *ctx)
{
    slv_file_reader_t *fr = ctx;

    return fr->size;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

slv_file_reader_t *
slv_file_reader_open (const char *path, GError **error)
{
    slv_file_reader_t *fr;
    struct stat st;
    int fd;

    fd = open (path, O_RDONLY | O_CLOEXEC);
    if (fd < 0 || fstat (fd, &st) != 0)
    {
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), "%s: %s", path,
                     g_strerror (errno));
        if (fd >= 0)
            close (fd);
        return NULL;
    }

    fr = g_new0 (slv_file_reader_t, 1);
    fr->path = g_strdup (path);
    fr->fd = fd;
    fr->size = st.st_size;
    fr->changes = g_array_new (FALSE, FALSE, sizeof (change_t));
    fr->reader.read = reader_read;
    fr->reader.size = reader_size;
    fr->reader.ctx = fr;
    return fr;
}

/* --------------------------------------------------------------------------------------------- */

void
slv_file_reader_free (slv_file_reader_t *fr)
{
    if (fr == NULL)
        return;
    if (fr->fd >= 0)
        close (fr->fd);
    g_array_free (fr->changes, TRUE);
    g_free (fr->path);
    g_free (fr);
}

/* --------------------------------------------------------------------------------------------- */

void
slv_file_reader_set_byte (slv_file_reader_t *fr, off_t offset, unsigned char value)
{
    change_t c;
    int idx;
    unsigned char orig = 0;

    if (offset < 0 || offset >= fr->size)
        return;

    idx = find_change (fr, offset);
    /* writing the original byte back removes the edit */
    if (pread (fr->fd, &orig, 1, offset) == 1 && orig == value)
    {
        if (idx >= 0)
            g_array_remove_index (fr->changes, idx);
        return;
    }
    if (idx >= 0)
    {
        g_array_index (fr->changes, change_t, idx).value = value;
        return;
    }
    c.offset = offset;
    c.value = value;
    g_array_append_val (fr->changes, c);
    g_array_sort (fr->changes, change_cmp);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
slv_file_reader_is_changed (const slv_file_reader_t *fr, off_t offset)
{
    return find_change (fr, offset) >= 0;
}

/* --------------------------------------------------------------------------------------------- */

guint
slv_file_reader_change_count (const slv_file_reader_t *fr)
{
    return fr->changes->len;
}

/* --------------------------------------------------------------------------------------------- */

void
slv_file_reader_discard (slv_file_reader_t *fr)
{
    g_array_set_size (fr->changes, 0);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
slv_file_reader_save (slv_file_reader_t *fr, GError **error)
{
    int fd;
    guint i;

    if (fr->changes->len == 0)
        return TRUE;

    fd = open (fr->path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
    {
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), "%s: %s", fr->path,
                     g_strerror (errno));
        return FALSE;
    }

    for (i = 0; i < fr->changes->len; i++)
    {
        const change_t *c = &g_array_index (fr->changes, change_t, i);

        if (pwrite (fd, &c->value, 1, c->offset) != 1)
        {
            g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                         "write at 0x%llx: %s", (unsigned long long) c->offset, g_strerror (errno));
            close (fd);
            /* keep the unwritten tail as pending */
            g_array_remove_range (fr->changes, 0, i);
            return FALSE;
        }
    }
    close (fd);
    g_array_set_size (fr->changes, 0);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
