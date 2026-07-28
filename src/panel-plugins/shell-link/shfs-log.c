/*
   libshfs - protocol logging.

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
 * \brief Source: what the library said to the remote shell and heard back
 *
 * A helper script that misbehaves on an unusual host produces a mangled
 * listing and nothing else. This writes down what was actually sent and what
 * came back, which is the only way to tell a broken script from a broken
 * parser without sitting at the remote machine.
 *
 * Off unless switched on: nothing is opened, nothing is written.
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "lib/global.h"
#include "lib/util.h"  // unix_error_string()

#include "shfs.h"

/*** file scope variables ************************************************************************/

static shfs_log_level_t log_level = SHFS_LOG_OFF;
static char *log_path = NULL;
static FILE *log_fp = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
shfs_log_stop (void)
{
    if (log_fp != NULL)
    {
        fclose (log_fp);
        log_fp = NULL;
    }

    g_free (log_path);
    log_path = NULL;
    log_level = SHFS_LOG_OFF;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
shfs_log_set (shfs_log_level_t level, const char *path, GError **error)
{
    shfs_log_stop ();

    if (level == SHFS_LOG_OFF)
        return TRUE;

    if (path == NULL || path[0] == '\0')
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, "%s", _ ("shfs log: no file name given"));
        return FALSE;
    }

    log_fp = fopen (path, "a");
    if (log_fp == NULL)
    {
        g_set_error (error, SHFS_ERROR, SHFS_ERR_FAILED, _ ("shfs log: cannot open %s: %s"), path,
                     unix_error_string (errno));
        return FALSE;
    }

    /* Line buffered: a crash still leaves everything written before it on disk. */
    setvbuf (log_fp, NULL, _IOLBF, 0);

    log_path = g_strdup (path);
    log_level = level;

    shfs_log_printf (SHFS_LOG_ERRORS, "--- log opened at level %d ---", (int) level);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

shfs_log_level_t
shfs_log_get_level (void)
{
    return log_level;
}

/* --------------------------------------------------------------------------------------------- */

const char *
shfs_log_get_path (void)
{
    return log_path;
}

/* --------------------------------------------------------------------------------------------- */

void
shfs_log_printf (shfs_log_level_t level, const char *fmt, ...)
{
    va_list ap;
    GDateTime *now;
    char *stamp;

    if (log_fp == NULL || level > log_level)
        return;

    now = g_date_time_new_now_local ();
    stamp = g_date_time_format (now, "%Y-%m-%d %H:%M:%S");
    g_date_time_unref (now);

    fprintf (log_fp, "%s ", stamp != NULL ? stamp : "?");
    g_free (stamp);

    va_start (ap, fmt);
    vfprintf (log_fp, fmt, ap);
    va_end (ap);

    fputc ('\n', log_fp);
}

/* --------------------------------------------------------------------------------------------- */

void
shfs_log_blob (shfs_log_level_t level, const char *tag, const char *data, gsize len)
{
    GString *out;
    gsize i;
    gsize shown;
    const gsize limit = 4096;

    if (log_fp == NULL || level > log_level || data == NULL)
        return;

    shown = MIN (len, limit);
    out = g_string_sized_new (shown + 32);

    for (i = 0; i < shown; i++)
    {
        unsigned char c = (unsigned char) data[i];

        if (c == '\n')
            g_string_append (out, "\n    ");
        else if (c == '\t' || (c >= 0x20 && c < 0x7f))
            g_string_append_c (out, (char) c);
        else
            g_string_append_printf (out, "\\x%02x", c);
    }

    if (len > shown)
        g_string_append_printf (out, "\n    ... %" G_GSIZE_FORMAT " more bytes", len - shown);

    shfs_log_printf (level, "%s (%" G_GSIZE_FORMAT " bytes)\n    %s", tag, len, out->str);

    g_string_free (out, TRUE);
}

/* --------------------------------------------------------------------------------------------- */
