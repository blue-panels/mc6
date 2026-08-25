/*
   shfs-tool - exercise libshfs without the file manager around it.

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
 * \brief Source: a command line front end for libshfs
 *
 * The library is meant to be usable without a panel, a widget or a terminal
 * of the file manager's kind. This program is the proof of that and the place
 * to reproduce a problem when it is unclear whether the fault is in the
 * protocol or in the plugin that drives it.
 *
 *   shfs-tool --host 10.0.0.1 --user eee -D 123
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "lib/global.h"

#include "shfs.h"

/*** file scope variables ************************************************************************/

static char *opt_host = NULL;
static char *opt_user = NULL;
static char *opt_password = NULL;
static int opt_port = 0;
static char *opt_mkdir = NULL;
static char *opt_list = NULL;
static char *opt_rmdir = NULL;
static char *opt_get = NULL;
static gboolean opt_verbose = FALSE;
static gboolean opt_helpers = FALSE;
static char *opt_helper = NULL;
static gboolean opt_dump = FALSE;
static char *opt_log = NULL;
static char *opt_log_level = NULL;
static char *opt_digest = NULL;
static char *opt_algo = NULL;
static gint64 opt_offset = 0;
static gint64 opt_length = -1;
static gint64 opt_max = 0;
static char *opt_resume = NULL;
static char *opt_local = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
usage (const char *argv0)
{
    printf ("Usage: %s --host HOST [options]\n"
            "\n"
            "  --host HOST        host to connect to\n"
            "  --user NAME        user name (default: the local one)\n"
            "  --password PASS    password; asked for if a host wants one and this is absent\n"
            "  --port N           port\n"
            "\n"
            "  -l PATH            list a directory\n"
            "  -D PATH            create a directory\n"
            "  -R PATH            remove a directory\n"
            "  -g PATH            read a file and write it to stdout\n"
            "  --max N            with -g: ask the far end for at most N bytes\n"
            "\n"
            "  --helpers          list the helper scripts and where each comes from\n"
            "  --helper NAME      act on one helper\n"
            "  --dump             with --helper: print the text that would run\n"
            "\n"
            "  --digest FILE      digest a local file the way the remote helpers do\n"
            "  --algo NAME        cksum, md5 or sha256 (default: sha256)\n"
            "  --offset N         with --digest: start here\n"
            "  --length N         with --digest: this many bytes (default: to the end)\n"
            "\n"
            "  --resume-probe P   how many bytes of --local match remote path P\n"
            "  --local FILE       the local side for --resume-probe\n"
            "\n"
            "  --log FILE         write the conversation to FILE\n"
            "  --log-level LEVEL  off, errors, commands or traffic (default: commands)\n"
            "\n"
            "  -v                 report progress\n"
            "\n"
            "--helpers and --dump do not connect: --host only picks whose\n"
            "per-host overrides to look at, and may be left out.\n"
            "\n"
            "Exits 0 when the operation succeeded.\n",
            argv0);
}

/* --------------------------------------------------------------------------------------------- */

static shfs_hostkey_action_t
cb_hostkey (shfs_hostkey_status_t status, const char *host, const char *fingerprint,
            void *user_data)
{
    char answer[16];

    (void) user_data;

    if (status == SHFS_HOSTKEY_MISMATCH)
        fprintf (stderr, "The key of %s does not match the one on record: %s\n", host, fingerprint);
    else
        fprintf (stderr, "%s is not known. Its key is %s\n", host, fingerprint);

    fprintf (stderr, "Accept? [y]es / [o]nce / [N]o: ");
    fflush (stderr);

    if (fgets (answer, sizeof (answer), stdin) == NULL)
        return SHFS_HOSTKEY_REJECT;

    if (answer[0] == 'y' || answer[0] == 'Y')
        return SHFS_HOSTKEY_TRUST_STORE;
    if (answer[0] == 'o' || answer[0] == 'O')
        return SHFS_HOSTKEY_TRUST_ONCE;

    return SHFS_HOSTKEY_REJECT;
}

/* --------------------------------------------------------------------------------------------- */

static char *
cb_password (const char *host, const char *user, gboolean retry, void *user_data)
{
    char *p;

    (void) user_data;

    if (retry)
        fprintf (stderr, "The stored password was refused.\n");

    p = getpass (g_strdup_printf ("Password for %s@%s: ", user, host));

    return (p != NULL && p[0] != '\0') ? g_strdup (p) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
cb_passphrase (const char *keyfile, void *user_data)
{
    char *p;

    (void) user_data;

    p = getpass (g_strdup_printf ("Passphrase for %s: ", keyfile));

    return (p != NULL && p[0] != '\0') ? g_strdup (p) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
cb_status (const char *text, void *user_data)
{
    (void) user_data;

    if (opt_verbose)
        fprintf (stderr, "%s\n", text);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_args (int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++)
    {
        const char *a = argv[i];
        const char *next = (i + 1 < argc) ? argv[i + 1] : NULL;

        if (strcmp (a, "-v") == 0)
            opt_verbose = TRUE;
        else if (strcmp (a, "--helpers") == 0)
            opt_helpers = TRUE;
        else if (strcmp (a, "--dump") == 0)
            opt_dump = TRUE;
        else if (next == NULL)
            return FALSE;
        else if (strcmp (a, "--host") == 0)
            opt_host = argv[++i];
        else if (strcmp (a, "--user") == 0)
            opt_user = argv[++i];
        else if (strcmp (a, "--password") == 0)
            opt_password = argv[++i];
        else if (strcmp (a, "--port") == 0)
            opt_port = atoi (argv[++i]);
        else if (strcmp (a, "-l") == 0)
            opt_list = argv[++i];
        else if (strcmp (a, "-D") == 0)
            opt_mkdir = argv[++i];
        else if (strcmp (a, "-R") == 0)
            opt_rmdir = argv[++i];
        else if (strcmp (a, "-g") == 0)
            opt_get = argv[++i];
        else if (strcmp (a, "--helper") == 0)
            opt_helper = argv[++i];
        else if (strcmp (a, "--digest") == 0)
            opt_digest = argv[++i];
        else if (strcmp (a, "--algo") == 0)
            opt_algo = argv[++i];
        else if (strcmp (a, "--offset") == 0)
            opt_offset = g_ascii_strtoll (argv[++i], NULL, 10);
        else if (strcmp (a, "--length") == 0)
            opt_length = g_ascii_strtoll (argv[++i], NULL, 10);
        else if (strcmp (a, "--max") == 0)
            opt_max = g_ascii_strtoll (argv[++i], NULL, 10);
        else if (strcmp (a, "--resume-probe") == 0)
            opt_resume = argv[++i];
        else if (strcmp (a, "--local") == 0)
            opt_local = argv[++i];
        else if (strcmp (a, "--log") == 0)
            opt_log = argv[++i];
        else if (strcmp (a, "--log-level") == 0)
            opt_log_level = argv[++i];
        else
            return FALSE;
    }

    return (opt_host != NULL || opt_helpers || opt_helper != NULL || opt_digest != NULL);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Digest a local file the way the remote side would.
 *
 * Resume compares local and remote digests as strings, so the output must
 * match cksum(1), md5sum(1) and sha256sum(1).
 */
static int
do_digest (const char *path)
{
    shfs_digest_algo_t algo = SHFS_DIGEST_SHA256;
    shfs_digest_t d;
    GError *error = NULL;
    gint64 length = opt_length;

    if (opt_algo != NULL)
    {
        if (strcmp (opt_algo, "cksum") == 0)
            algo = SHFS_DIGEST_CKSUM;
        else if (strcmp (opt_algo, "md5") == 0)
            algo = SHFS_DIGEST_MD5;
        else if (strcmp (opt_algo, "sha256") == 0)
            algo = SHFS_DIGEST_SHA256;
        else
        {
            fprintf (stderr, "unknown algorithm '%s'\n", opt_algo);
            return 2;
        }
    }

    if (length < 0)
    {
        struct stat st;

        if (stat (path, &st) != 0)
        {
            fprintf (stderr, "%s: cannot stat\n", path);
            return 1;
        }

        length = (gint64) st.st_size - opt_offset;
        if (length < 0)
            length = 0;
    }

    if (!shfs_local_digest_range (path, opt_offset, length, algo, &d, &error))
    {
        fprintf (stderr, "%s\n", error != NULL ? error->message : "failed");
        g_clear_error (&error);
        return 1;
    }

    printf ("%s\n", d.hex);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
source_name (shfs_helper_source_t s)
{
    switch (s)
    {
    case SHFS_HELPER_USER:
        return "user";
    case SHFS_HELPER_SYSTEM:
        return "system";
    default:
        return "built-in";
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
do_helpers (const char *hostname)
{
    GPtrArray *list;
    guint i;

    list = shfs_helpers_list (hostname);

    printf ("%-14s %-9s %4s %7s  %s\n", "HELPER", "SOURCE", "REV", "BYTES", "PATH");

    for (i = 0; i < list->len; i++)
    {
        const shfs_helper_t *h = (const shfs_helper_t *) g_ptr_array_index (list, i);
        char rev[32];

        if (h->version < h->expected_version)
            snprintf (rev, sizeof (rev), "%d<%d", h->version, h->expected_version);
        else
            snprintf (rev, sizeof (rev), "%d", h->version);

        printf ("%-14s %-9s %4s %7" G_GSIZE_FORMAT "  %s\n", h->name, source_name (h->source), rev,
                h->size, h->path != NULL ? h->path : "");
    }

    printf ("\nREV shown as \"a<b\" means the file was written for revision a "
            "and this build expects b.\n");

    shfs_helpers_free (list);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
do_helper_dump (const char *hostname, const char *name)
{
    shfs_helper_source_t src;
    char *content;

    content = shfs_helper_content (hostname, name, &src);
    if (content == NULL)
    {
        fprintf (stderr, "%s: no such helper\n", name);
        return 1;
    }

    if (opt_verbose)
        fprintf (stderr, "%s: %s\n", name, source_name (src));

    fputs (content, stdout);
    g_free (content);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
setup_log (void)
{
    shfs_log_level_t level = SHFS_LOG_COMMANDS;
    GError *error = NULL;

    if (opt_log_level != NULL)
    {
        if (strcmp (opt_log_level, "off") == 0)
            level = SHFS_LOG_OFF;
        else if (strcmp (opt_log_level, "errors") == 0)
            level = SHFS_LOG_ERRORS;
        else if (strcmp (opt_log_level, "commands") == 0)
            level = SHFS_LOG_COMMANDS;
        else if (strcmp (opt_log_level, "traffic") == 0)
            level = SHFS_LOG_TRAFFIC;
        else
        {
            fprintf (stderr, "unknown log level '%s'\n", opt_log_level);
            return FALSE;
        }
    }

    if (opt_log == NULL)
        return TRUE;

    if (!shfs_log_set (level, opt_log, &error))
    {
        fprintf (stderr, "log: %s\n", error != NULL ? error->message : "failed");
        g_clear_error (&error);
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
do_list (shfs_conn_t *conn, const char *path)
{
    GPtrArray *entries;
    GError *error = NULL;
    guint i;

    entries = shfs_list_dir (conn, path, &error);
    if (entries == NULL)
    {
        fprintf (stderr, "list %s: %s\n", path, error != NULL ? error->message : "failed");
        g_clear_error (&error);
        return 1;
    }

    for (i = 0; i < entries->len; i++)
    {
        const shfs_entry_t *e = (const shfs_entry_t *) g_ptr_array_index (entries, i);

        printf ("%c %5u %5u %8" G_GINT64_FORMAT "  %s%s%s\n", S_ISDIR (e->st.st_mode) ? 'd' : '-',
                (unsigned int) e->st.st_uid, (unsigned int) e->st.st_gid, (gint64) e->st.st_size,
                e->name, e->linkname != NULL ? " -> " : "", e->linkname != NULL ? e->linkname : "");
    }

    printf ("%u entries\n", entries->len);
    shfs_entries_free (entries);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
do_get (shfs_conn_t *conn, const char *path)
{
    GError *error = NULL;
    gint64 remaining = 0;
    char buf[64 * 1024];

    if (!shfs_get_begin (conn, path, 0, opt_max, &remaining, &error))
    {
        fprintf (stderr, "get %s: %s\n", path, error != NULL ? error->message : "failed");
        g_clear_error (&error);
        return 1;
    }

    if (opt_verbose)
        fprintf (stderr, "%" G_GINT64_FORMAT " bytes\n", remaining);

    while (TRUE)
    {
        gssize n;

        n = shfs_get_read (conn, buf, sizeof (buf), &error);
        if (n < 0)
        {
            fprintf (stderr, "get %s: %s\n", path, error != NULL ? error->message : "failed");
            g_clear_error (&error);
            return 1;
        }
        if (n == 0)
            break;

        if (fwrite (buf, 1, n, stdout) != (size_t) n)
        {
            fprintf (stderr, "get %s: local write failed\n", path);
            return 1;
        }
    }

    if (!shfs_get_finish (conn, NULL, &error))
    {
        fprintf (stderr, "get %s: %s\n", path, error != NULL ? error->message : "failed");
        g_clear_error (&error);
        return 1;
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
main (int argc, char **argv)
{
    shfs_conn_params_t params;
    shfs_connect_cb_t cb;
    shfs_conn_t *conn;
    GError *error = NULL;
    int rc = 0;

    if (!parse_args (argc, argv))
    {
        usage (argv[0]);
        return 2;
    }

    if (!setup_log ())
        return 2;

    /* None of these talks to a host. */
    if (opt_digest != NULL)
        return do_digest (opt_digest);
    if (opt_helpers)
        return do_helpers (opt_host);
    if (opt_helper != NULL && opt_dump)
        return do_helper_dump (opt_host, opt_helper);

    if (opt_host == NULL)
    {
        usage (argv[0]);
        return 2;
    }

    memset (&params, 0, sizeof (params));
    params.host = opt_host;
    params.user = opt_user;
    params.password = opt_password;
    params.port = opt_port;

    memset (&cb, 0, sizeof (cb));
    cb.hostkey = cb_hostkey;
    cb.password = cb_password;
    cb.passphrase = cb_passphrase;
    cb.status = cb_status;

    conn = shfs_conn_open (&params, &cb, &error);
    if (conn == NULL)
    {
        fprintf (stderr, "connect: %s\n", error != NULL ? error->message : "failed");
        g_clear_error (&error);
        return 1;
    }

    if (opt_verbose)
        fprintf (stderr, "connected; digests: %#x\n", shfs_conn_digest_algos (conn));

    if (opt_mkdir != NULL)
    {
        if (!shfs_mkdir_path (conn, opt_mkdir, 0755, &error))
        {
            fprintf (stderr, "mkdir %s: %s\n", opt_mkdir,
                     error != NULL ? error->message : "failed");
            g_clear_error (&error);
            rc = 1;
        }
        else
            printf ("created %s\n", opt_mkdir);
    }

    if (rc == 0 && opt_rmdir != NULL)
    {
        if (!shfs_rmdir_path (conn, opt_rmdir, &error))
        {
            fprintf (stderr, "rmdir %s: %s\n", opt_rmdir,
                     error != NULL ? error->message : "failed");
            g_clear_error (&error);
            rc = 1;
        }
        else
            printf ("removed %s\n", opt_rmdir);
    }

    if (rc == 0 && opt_list != NULL)
        rc = do_list (conn, opt_list);

    if (rc == 0 && opt_get != NULL)
        rc = do_get (conn, opt_get);

    if (rc == 0 && opt_resume != NULL)
    {
        GError *perr = NULL;
        struct stat st;
        gint64 remote_size;
        gint64 k;

        if (opt_local == NULL)
        {
            fprintf (stderr, "--resume-probe needs --local\n");
            rc = 2;
        }
        else if (stat (opt_local, &st) != 0)
        {
            fprintf (stderr, "%s: cannot stat\n", opt_local);
            rc = 1;
        }
        else
        {
            remote_size = shfs_file_size (conn, opt_resume, &perr);
            if (remote_size < 0)
            {
                fprintf (stderr, "%s: %s\n", opt_resume,
                         perr != NULL ? perr->message : "not found");
                g_clear_error (&perr);
                rc = 1;
            }
            else
            {
                k = shfs_resume_probe (conn, opt_resume, opt_local, remote_size,
                                       (gint64) st.st_size, &perr);
                if (k < 0)
                    printf ("cannot continue: local %" G_GINT64_FORMAT
                            " bytes, remote %" G_GINT64_FORMAT "\n",
                            (gint64) st.st_size, remote_size);
                else
                    printf ("%" G_GINT64_FORMAT " of %" G_GINT64_FORMAT
                            " local bytes verified against %" G_GINT64_FORMAT " remote\n",
                            k, (gint64) st.st_size, remote_size);
                g_clear_error (&perr);
            }
        }
    }

    shfs_conn_close (conn);

    return rc;
}

/* --------------------------------------------------------------------------------------------- */
