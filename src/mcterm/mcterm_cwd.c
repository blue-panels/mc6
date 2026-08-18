/*
   Midnight Commander - mcterm cwd synchronization.

   Tracks the shell's working directory via OSC 7 and syncs it with
   the file-manager panel when switching between panel and terminal mode.

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

#include <config.h>
#include <string.h>

#include "lib/global.h"

#include "mcterm.h"
#include "mcterm_cwd.h"

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/* Return TRUE if host (not NUL-terminated, length len) is the local machine. */
static gboolean
osc7_host_is_local (const char *host, size_t len)
{
    const char *local;

    if (len == 0)
        return TRUE; /* empty host == localhost */
    if (len == 9 && memcmp (host, "localhost", 9) == 0)
        return TRUE;
    local = g_get_host_name ();
    return (local != NULL && strlen (local) == len && memcmp (host, local, len) == 0);
}

/* Split the session token off the end of an OSC 7 payload.
 * With a token expected, a payload that does not carry ours is not ours to read: it is the
 * output of some command, or a shell on the far side of ssh. Returns the length of the path
 * part, or -1 when the payload must be ignored. */
static gssize
osc7_strip_token (const char *path, const char *token)
{
    const char *mark;
    size_t plen;

    plen = strlen (path);

    if (token == NULL)
        return (gssize) plen;

    mark = strrchr (path, '?');
    if (mark == NULL
        || strncmp (mark, MCTERM_OSC7_TOKEN_PREFIX, sizeof (MCTERM_OSC7_TOKEN_PREFIX) - 1) != 0)
        return -1;

    if (strcmp (mark + sizeof (MCTERM_OSC7_TOKEN_PREFIX) - 1, token) != 0)
        return -1;

    return (gssize) (mark - path);
}

/* Decode a local path from OSC 7 payload "7;file://[host]/path[?mc=token]".
 * Returns NULL for non-local hostnames, for non-file:// URIs, and - when a token is expected -
 * for a payload that does not carry it.
 * Tolerates unencoded paths (most shells do not percent-encode $PWD). */
char *
mcterm_osc7_uri_to_path (const char *osc7_raw, const char *token)
{
    const char *uri;
    const char *host_start;
    const char *path;
    gssize path_len;
    char *decoded;
    char *raw_path;

    if (osc7_raw == NULL || strncmp (osc7_raw, "7;", 2) != 0)
        return NULL;

    uri = osc7_raw + 2;
    if (strncmp (uri, "file://", 7) != 0)
        return NULL;

    host_start = uri + 7;
    if (*host_start == '/')
    {
        path = host_start; /* empty hostname: file:///path */
    }
    else
    {
        path = strchr (host_start, '/');
        if (path == NULL)
            return NULL;
        if (!osc7_host_is_local (host_start, (size_t) (path - host_start)))
            return NULL;
    }

    path_len = osc7_strip_token (path, token);
    if (path_len < 0)
        return NULL;

    raw_path = g_strndup (path, (gsize) path_len);
    /* Decode percent-encoded sequences; leave unencoded chars as-is. */
    decoded = g_uri_unescape_string (raw_path, "/");
    g_free (raw_path);

    return decoded;
}

/* --------------------------------------------------------------------------------------------- */

static char *
mcterm_cwd_from_osc7 (WMcTerm *t)
{
    return mcterm_osc7_uri_to_path (mcterm_osc7_raw (t), mcterm_osc7_token (t));
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
mcterm_cwd_on_exit (WMcTerm *t, const char *panel_cwd)
{
    char *path;

    path = mcterm_cwd_from_osc7 (t);
    if (path == NULL)
        return NULL;

    if (panel_cwd != NULL && strcmp (path, panel_cwd) == 0)
    {
        g_free (path);
        return NULL;
    }

    return path;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_cwd_differs (WMcTerm *t, const char *panel_cwd)
{
    char *path;

    path = mcterm_cwd_on_exit (t, panel_cwd);
    if (path == NULL)
        return FALSE;
    g_free (path);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
