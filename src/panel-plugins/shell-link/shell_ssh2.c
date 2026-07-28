/*
   Virtual File System: SHELL implementation for transferring files over
   shell connections - libssh2 transport layer.

   Copyright (C) 2025
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2025

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
 * \brief Source: libssh2 transport for the shell connection library
 */

#include <config.h>

#ifdef ENABLE_SHELL_SSH2

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <libssh2.h>

#include "lib/global.h"

#include "lib/util.h"      // unix_error_string()
#include "lib/mcconfig.h"  // mc_config_get_home_dir()

#include "shfs-priv.h"  // SHELL_FLAG_RSH
#include "shell_ssh2.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define SHELL_SSH2_DEFAULT_PORT 22
#define SHA1_DIGEST_LENGTH      20

/* LIBSSH2_INVALID_SOCKET is defined in libssh2 >= 1.4.1 */
#ifndef LIBSSH2_INVALID_SOCKET
#define LIBSSH2_INVALID_SOCKET -1
#endif

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

#ifdef LIBSSH2_KNOWNHOST_KEY_ED25519
static const char *const hostkey_method_ssh_ed25519 = "ssh-ed25519";
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_521
static const char *const hostkey_method_ssh_ecdsa_521 = "ecdsa-sha2-nistp521";
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_384
static const char *const hostkey_method_ssh_ecdsa_384 = "ecdsa-sha2-nistp384";
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
static const char *const hostkey_method_ssh_ecdsa_256 = "ecdsa-sha2-nistp256";
#endif
static const char *const hostkey_method_ssh_rsa = "ssh-rsa";
static const char *const hostkey_method_ssh_dss = "ssh-dss";

static const char *default_hostkey_methods =
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
    "ecdsa-sha2-nistp256,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_384
    "ecdsa-sha2-nistp384,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_521
    "ecdsa-sha2-nistp521,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
    "ecdsa-sha2-nistp256-cert-v01@openssh.com,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_384
    "ecdsa-sha2-nistp384-cert-v01@openssh.com,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_521
    "ecdsa-sha2-nistp521-cert-v01@openssh.com,"
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ED25519
    "ssh-ed25519,"
    "ssh-ed25519-cert-v01@openssh.com,"
#endif
    "rsa-sha2-256,"
    "rsa-sha2-512,"
    "ssh-rsa,"
    "ssh-rsa-cert-v01@openssh.com,"
    "ssh-dss";

/**
 * Per-connection state. A pointer to this lives in the libssh2 session
 * abstract, so callbacks reach it without file scope state.
 */
typedef struct shell_ssh2_ctx
{
    const shfs_conn_params_t *params;
    const shfs_connect_cb_t *cb;
    const char *user;        // params->user, or the local user name
    char *local_user;        // owned when params->user was NULL
    const char *kbi_passwd;  // secret offered to keyboard-interactive
} shell_ssh2_ctx_t;

/* --------------------------------------------------------------------------------------------- */

static void shell_ssh2_status (const shell_ssh2_ctx_t *ctx, const char *fmt, ...)
    G_GNUC_PRINTF (2, 3);

static void
shell_ssh2_status (const shell_ssh2_ctx_t *ctx, const char *fmt, ...)
{
    char buf[BUF_MEDIUM];
    va_list ap;

    if (ctx->cb == NULL || ctx->cb->status == NULL)
        return;

    va_start (ap, fmt);
    g_vsnprintf (buf, sizeof (buf), fmt, ap);
    va_end (ap);

    ctx->cb->status (buf, ctx->cb->user_data);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_cancelled (const shell_ssh2_ctx_t *ctx)
{
    return (ctx->cb != NULL && ctx->cb->cancelled != NULL
            && ctx->cb->cancelled (ctx->cb->user_data));
}

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static int
shell_ssh2_open_socket (shell_ssh2_ctx_t *ctx, GError **mcerror)
{
    struct addrinfo hints, *res = NULL, *curr_res;
    int my_socket = 0;
    char port[BUF_TINY];
    int e;
    int ssh_port;

    mc_return_val_if_error (mcerror, LIBSSH2_INVALID_SOCKET);

    if (ctx->params->host == NULL || *ctx->params->host == '\0')
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: Invalid host name."));
        return LIBSSH2_INVALID_SOCKET;
    }

    ssh_port = ctx->params->port;
    if (ssh_port <= SHELL_FLAG_RSH)
        ssh_port = SHELL_SSH2_DEFAULT_PORT;

    sprintf (port, "%d", ssh_port);

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

#ifdef AI_ADDRCONFIG
    hints.ai_flags = AI_ADDRCONFIG;
#endif

    e = getaddrinfo (ctx->params->host, port, &hints, &res);

#ifdef AI_ADDRCONFIG
    if (e == EAI_BADFLAGS)
    {
        hints.ai_flags = 0;
        e = getaddrinfo (ctx->params->host, port, &hints, &res);
    }
#endif

    if (e != 0)
    {
        mc_propagate_error (mcerror, e, _ ("shell: %s"), gai_strerror (e));
        my_socket = LIBSSH2_INVALID_SOCKET;
        goto ret;
    }

    for (curr_res = res; curr_res != NULL; curr_res = curr_res->ai_next)
    {
        int save_errno;

        my_socket = socket (curr_res->ai_family, curr_res->ai_socktype, curr_res->ai_protocol);

        if (my_socket < 0)
        {
            if (curr_res->ai_next != NULL)
                continue;

            shell_ssh2_status (ctx, _ ("shell: %s"), unix_error_string (errno));
            my_socket = LIBSSH2_INVALID_SOCKET;
            goto ret;
        }

        shell_ssh2_status (ctx, _ ("shell: making connection to %s"), ctx->params->host);

        if (connect (my_socket, curr_res->ai_addr, curr_res->ai_addrlen) >= 0)
            break;

        save_errno = errno;

        close (my_socket);

        if (save_errno == EINTR && shell_ssh2_cancelled (ctx))
            mc_propagate_error (mcerror, 0, "%s", _ ("shell: connection interrupted by user"));
        else if (curr_res->ai_next == NULL)
            mc_propagate_error (mcerror, save_errno, _ ("shell: connection to server failed: %s"),
                                unix_error_string (save_errno));
        else
            continue;

        my_socket = LIBSSH2_INVALID_SOCKET;
        break;
    }

ret:
    if (res != NULL)
        freeaddrinfo (res);
    return my_socket;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_read_known_hosts (shell_ssh2_t *ssh2, shell_ssh2_ctx_t *ctx, GError **mcerror)
{
    struct libssh2_knownhost *store = NULL;
    int rc;
    gboolean found = FALSE;

    ssh2->known_hosts = libssh2_knownhost_init (ssh2->session);
    if (ssh2->known_hosts == NULL)
        goto err;

    ssh2->known_hosts_file =
        mc_build_filename (mc_config_get_home_dir (), ".ssh", "known_hosts", (char *) NULL);

    if (!exist_file (ssh2->known_hosts_file))
    {
        mc_propagate_error (mcerror, 0, _ ("shell: cannot open %s:\n%s"), ssh2->known_hosts_file,
                            unix_error_string (errno));
        return FALSE;
    }

    rc = libssh2_knownhost_readfile (ssh2->known_hosts, ssh2->known_hosts_file,
                                     LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    if (rc < 0)
    {
        /* A file we cannot read is not a file with no hosts in it. Carrying on
           would treat every host as new, and answering "store" then rewrites
           the file from an empty set, losing every key it holds. */
        mc_propagate_error (mcerror, rc, _ ("shell: cannot read %s"), ssh2->known_hosts_file);
        return FALSE;
    }

    if (rc > 0)
    {
        const char *kh_name_end = NULL;

        while (!found && libssh2_knownhost_get (ssh2->known_hosts, &store, store) == 0)
        {
            if (store == NULL)
                continue;

            if (store->name == NULL)
                continue;

            if (store->name[0] != '[')
                found = strcmp (store->name, ctx->params->host) == 0;
            else
            {
                int kh_port;

                kh_name_end = strstr (store->name, "]:");
                if (kh_name_end == NULL)
                    continue;

                kh_port = (int) g_ascii_strtoll (kh_name_end + 2, NULL, 10);
                if (kh_port == ctx->params->port)
                {
                    size_t kh_name_size;

                    kh_name_size = strlen (store->name) - 1 - strlen (kh_name_end);
                    found = strncmp (store->name + 1, ctx->params->host, kh_name_size) == 0;
                }
            }
        }
    }

    if (found)
    {
        int mask;
        const char *hostkey_method = NULL;
        char *hostkey_methods;

        mask = store->typemask & LIBSSH2_KNOWNHOST_KEY_MASK;

        switch (mask)
        {
#ifdef LIBSSH2_KNOWNHOST_KEY_ED25519
        case LIBSSH2_KNOWNHOST_KEY_ED25519:
            hostkey_method = hostkey_method_ssh_ed25519;
            break;
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_521
        case LIBSSH2_KNOWNHOST_KEY_ECDSA_521:
            hostkey_method = hostkey_method_ssh_ecdsa_521;
            break;
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_384
        case LIBSSH2_KNOWNHOST_KEY_ECDSA_384:
            hostkey_method = hostkey_method_ssh_ecdsa_384;
            break;
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
        case LIBSSH2_KNOWNHOST_KEY_ECDSA_256:
            hostkey_method = hostkey_method_ssh_ecdsa_256;
            break;
#endif
        case LIBSSH2_KNOWNHOST_KEY_SSHRSA:
            hostkey_method = hostkey_method_ssh_rsa;
            break;
        case LIBSSH2_KNOWNHOST_KEY_SSHDSS:
            hostkey_method = hostkey_method_ssh_dss;
            break;
        case LIBSSH2_KNOWNHOST_KEY_RSA1:
            mc_propagate_error (mcerror, 0, "%s",
                                _ ("shell: found host key of unsupported type: RSA1"));
            return FALSE;
        default:
            mc_propagate_error (mcerror, 0, "%s 0x%x", _ ("shell: unknown host key type:"),
                                (unsigned int) mask);
            return FALSE;
        }

        hostkey_methods = g_strdup_printf ("%s,%s", hostkey_method, default_hostkey_methods);
        rc = libssh2_session_method_pref (ssh2->session, LIBSSH2_METHOD_HOSTKEY, hostkey_methods);
        g_free (hostkey_methods);
        if (rc < 0)
            goto err;
    }

    return TRUE;

err:
{
    char *err = NULL;
    int err_len;

    libssh2_session_last_error (ssh2->session, &err, &err_len, 1);
    mc_propagate_error (mcerror, 0, "%s", err);
    g_free (err);
}
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
shell_ssh2_compute_fingerprint_hash (LIBSSH2_SESSION *session)
{
    static char result[SHA1_DIGEST_LENGTH * 3 + 1];
    const char *fingerprint;
    size_t i;

    fingerprint = libssh2_hostkey_hash (session, LIBSSH2_HOSTKEY_HASH_SHA1);
    if (fingerprint == NULL)
        return NULL;

    for (i = 0; i < SHA1_DIGEST_LENGTH && i * 3 < sizeof (result) - 1; i++)
        g_snprintf ((gchar *) (result + i * 3), 4, "%02x:", (guint8) fingerprint[i]);

    result[i * 3 - 1] = '\0';

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_ssh2_update_known_hosts (shell_ssh2_t *ssh2, shell_ssh2_ctx_t *ctx, const char *remote_key,
                               size_t remote_key_len, int type_mask)
{
    int rc;

    rc = libssh2_knownhost_addc (ssh2->known_hosts, ctx->params->host, NULL, remote_key,
                                 remote_key_len, NULL, 0, type_mask, NULL);
    if (rc < 0)
        return rc;

    /* A full rewrite of the user's known_hosts from what libssh2 parsed on the
       way in: lines its parser could not read are dropped here. */
    rc = libssh2_knownhost_writefile (ssh2->known_hosts, ssh2->known_hosts_file,
                                      LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    if (rc < 0)
        return rc;

    shell_ssh2_status (ctx, _ ("Permanently added %s to the list of known hosts."),
                       ctx->params->host);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_process_known_host (shell_ssh2_t *ssh2, shell_ssh2_ctx_t *ctx, GError **mcerror)
{
    const char *remote_key;
    const char *key_type;
    const char *fingerprint_hash;
    size_t remote_key_len = 0;
    int remote_key_type = LIBSSH2_HOSTKEY_TYPE_UNKNOWN;
    int keybit = 0;
    struct libssh2_knownhost *host = NULL;
    int rc;
    char *msg = NULL;
    gboolean handle_query = FALSE;
    shfs_hostkey_status_t status = SHFS_HOSTKEY_UNKNOWN;

    remote_key = libssh2_session_hostkey (ssh2->session, &remote_key_len, &remote_key_type);
    if (remote_key == NULL || remote_key_len == 0
        || remote_key_type == LIBSSH2_HOSTKEY_TYPE_UNKNOWN)
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: cannot get the remote host key"));
        return FALSE;
    }

    switch (remote_key_type)
    {
    case LIBSSH2_HOSTKEY_TYPE_RSA:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHRSA;
        key_type = "RSA";
        break;
    case LIBSSH2_HOSTKEY_TYPE_DSS:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHDSS;
        key_type = "DSS";
        break;
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
        keybit = LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
        key_type = "ECDSA";
        break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_384
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
        keybit = LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
        key_type = "ECDSA";
        break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_521
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
        keybit = LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
        key_type = "ECDSA";
        break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
    case LIBSSH2_HOSTKEY_TYPE_ED25519:
        keybit = LIBSSH2_KNOWNHOST_KEY_ED25519;
        key_type = "ED25519";
        break;
#endif
    default:
        mc_propagate_error (mcerror, 0, "%s",
                            _ ("shell: unsupported key type, can't check remote host key"));
        return FALSE;
    }

    fingerprint_hash = shell_ssh2_compute_fingerprint_hash (ssh2->session);
    if (fingerprint_hash == NULL)
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: can't compute host key fingerprint hash"));
        return FALSE;
    }

    rc = libssh2_knownhost_checkp (
        ssh2->known_hosts, ctx->params->host, ctx->params->port, remote_key, remote_key_len,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | keybit, &host);

    switch (rc)
    {
    default:
    case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
        goto err;

    case LIBSSH2_KNOWNHOST_CHECK_MATCH:
        break;

    case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
        status = SHFS_HOSTKEY_UNKNOWN;
        handle_query = TRUE;
        break;

    case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
        status = SHFS_HOSTKEY_MISMATCH;
        handle_query = TRUE;
        break;
    }

    if (handle_query)
    {
        shfs_hostkey_action_t action = SHFS_HOSTKEY_REJECT;

        /* No callback means nobody can vouch for the key, so refuse. */
        if (ctx->cb != NULL && ctx->cb->hostkey != NULL)
        {
            msg = g_strdup_printf ("%s SHA1:%s", key_type, fingerprint_hash);
            action = ctx->cb->hostkey (status, ctx->params->host, msg, ctx->cb->user_data);
            g_free (msg);
            msg = NULL;
        }

        switch (action)
        {
        case SHFS_HOSTKEY_TRUST_STORE:
            if (shell_ssh2_update_known_hosts (ssh2, ctx, remote_key, remote_key_len,
                                               LIBSSH2_KNOWNHOST_TYPE_PLAIN
                                                   | LIBSSH2_KNOWNHOST_KEYENC_RAW | keybit)
                < 0)
                goto err;
            break;
        case SHFS_HOSTKEY_TRUST_ONCE:
            break;
        case SHFS_HOSTKEY_REJECT:
        default:
            mc_propagate_error (mcerror, 0, "%s", _ ("shell: host key verification failed"));
            goto err;
        }
    }

    return TRUE;

err:
{
    char *err = NULL;
    int err_len;

    libssh2_session_last_error (ssh2->session, &err, &err_len, 1);
    if (err != NULL)
    {
        mc_propagate_error (mcerror, 0, "%s", err);
        g_free (err);
    }
}

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_auth_agent (shell_ssh2_t *ssh2, shell_ssh2_ctx_t *ctx)
{
    struct libssh2_agent_publickey *identity, *prev_identity = NULL;
    int rc;

    ssh2->agent = libssh2_agent_init (ssh2->session);
    if (ssh2->agent == NULL)
        return FALSE;

    if (libssh2_agent_connect (ssh2->agent) != 0)
        return FALSE;

    if (libssh2_agent_list_identities (ssh2->agent) != 0)
        return FALSE;

    while (TRUE)
    {
        rc = libssh2_agent_get_identity (ssh2->agent, &identity, prev_identity);
        if (rc == 1)
            break;

        if (rc < 0)
            return FALSE;

        if (libssh2_agent_userauth (ssh2->agent, ctx->user, identity) == 0)
            break;

        prev_identity = identity;
    }

    return (rc == 0);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_auth_pubkey (shell_ssh2_t *ssh2, shell_ssh2_ctx_t *ctx)
{
    static const char *const key_names[] = { "id_ed25519", "id_ecdsa", "id_rsa", "id_dsa", NULL };
    int i;

    for (i = 0; key_names[i] != NULL; i++)
    {
        char *privkey;
        char *pubkey;

        privkey =
            mc_build_filename (mc_config_get_home_dir (), ".ssh", key_names[i], (char *) NULL);
        pubkey = g_strdup_printf ("%s.pub", privkey);

        if (exist_file (privkey))
        {
            int rc;

            rc = libssh2_userauth_publickey_fromfile (ssh2->session, ctx->user,
                                                      exist_file (pubkey) ? pubkey : NULL, privkey,
                                                      ctx->params->password);
            if (rc == 0)
            {
                g_free (pubkey);
                g_free (privkey);
                return TRUE;
            }

            /* If key needs passphrase and we don't have one, try prompting */
            if (rc == LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED || rc == LIBSSH2_ERROR_FILE)
            {
                char *passwd = NULL;

                if (ctx->cb != NULL && ctx->cb->passphrase != NULL)
                    passwd = ctx->cb->passphrase (privkey, ctx->cb->user_data);

                if (passwd != NULL)
                {
                    rc = libssh2_userauth_publickey_fromfile (ssh2->session, ctx->user,
                                                              exist_file (pubkey) ? pubkey : NULL,
                                                              privkey, passwd);
                    g_free (passwd);

                    if (rc == 0)
                    {
                        g_free (pubkey);
                        g_free (privkey);
                        return TRUE;
                    }
                }
            }
        }

        g_free (pubkey);
        g_free (privkey);
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static LIBSSH2_USERAUTH_KBDINT_RESPONSE_FUNC (shell_ssh2_keyboard_interactive_helper)
{
    const shell_ssh2_ctx_t *ctx;
    int i;
    size_t len;

    (void) instruction;
    (void) instruction_len;

    ctx = (abstract != NULL) ? *(const shell_ssh2_ctx_t **) abstract : NULL;

    if (ctx == NULL || ctx->kbi_passwd == NULL)
        return;

    if (strncmp (name, ctx->user, name_len) != 0)
        return;

    len = strlen (ctx->kbi_passwd);

    for (i = 0; i < num_prompts; ++i)
        if (memcmp (prompts[i].text, "Password: ", prompts[i].length) == 0)
        {
            responses[i].text = strdup (ctx->kbi_passwd);
            responses[i].length = len;
        }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_ssh2_auth_password (shell_ssh2_t *ssh2, shell_ssh2_ctx_t *ctx)
{
    int rc;

    if (ctx->params->password != NULL)
    {
        while ((rc = libssh2_userauth_password (ssh2->session, ctx->user, ctx->params->password))
               == LIBSSH2_ERROR_EAGAIN)
            ;
        if (rc == 0)
            return TRUE;

        ctx->kbi_passwd = ctx->params->password;

        while ((rc = libssh2_userauth_keyboard_interactive (ssh2->session, ctx->user,
                                                            shell_ssh2_keyboard_interactive_helper))
               == LIBSSH2_ERROR_EAGAIN)
            ;

        ctx->kbi_passwd = NULL;

        if (rc == 0)
            return TRUE;
    }

    {
        char *passwd = NULL;

        /* retry is TRUE when a stored secret was already offered and refused */
        if (ctx->cb != NULL && ctx->cb->password != NULL)
            passwd = ctx->cb->password (ctx->params->host, ctx->user, ctx->params->password != NULL,
                                        ctx->cb->user_data);

        if (passwd == NULL)
            return FALSE;

        while ((rc = libssh2_userauth_password (ssh2->session, ctx->user, passwd))
               == LIBSSH2_ERROR_EAGAIN)
            ;

        if (rc != 0)
        {
            ctx->kbi_passwd = passwd;

            while ((rc = libssh2_userauth_keyboard_interactive (
                        ssh2->session, ctx->user, shell_ssh2_keyboard_interactive_helper))
                   == LIBSSH2_ERROR_EAGAIN)
                ;

            ctx->kbi_passwd = NULL;
        }

        if (rc == 0)
        {
            if (ctx->cb != NULL && ctx->cb->password_accepted != NULL)
                ctx->cb->password_accepted (passwd, ctx->cb->user_data);
            g_free (passwd);
            return TRUE;
        }

        g_free (passwd);
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

shell_ssh2_t *
shell_ssh2_open (const shfs_conn_params_t *params, const shfs_connect_cb_t *cb, GError **mcerror)
{
    shell_ssh2_ctx_t *ctx;
    shell_ssh2_t *ssh2;
    int rc;

    mc_return_val_if_error (mcerror, NULL);

    /* rsh mode - libssh2 is not applicable */
    if (params->port == SHELL_FLAG_RSH)
        return NULL;

    ssh2 = g_new0 (shell_ssh2_t, 1);
    ssh2->socket_fd = LIBSSH2_INVALID_SOCKET;

    ctx = g_new0 (shell_ssh2_ctx_t, 1);
    ssh2->ctx = ctx;
    ctx->params = params;
    ctx->cb = cb;
    if (params->user != NULL)
        ctx->user = params->user;
    else
    {
        ctx->local_user = g_strdup (g_get_user_name ());
        ctx->user = ctx->local_user;
    }

    ssh2->socket_fd = shell_ssh2_open_socket (ctx, mcerror);
    if (ssh2->socket_fd == LIBSSH2_INVALID_SOCKET)
        goto err;

    /* The context goes into the session abstract so that callbacks reach it. */
    ssh2->session = libssh2_session_init_ex (NULL, NULL, NULL, ctx);
    if (ssh2->session == NULL)
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: failed to init SSH session"));
        goto err;
    }

    if (ctx->params->port == SHELL_FLAG_COMPRESSED)
        libssh2_session_flag (ssh2->session, LIBSSH2_FLAG_COMPRESS, 1);

    if (!shell_ssh2_read_known_hosts (ssh2, ctx, mcerror))
        goto err;

    while ((rc = libssh2_session_handshake (ssh2->session, (libssh2_socket_t) ssh2->socket_fd))
           == LIBSSH2_ERROR_EAGAIN)
        ;
    if (rc != 0)
    {
        mc_propagate_error (mcerror, rc, "%s", _ ("shell: failure establishing SSH session"));
        goto err;
    }

    if (!shell_ssh2_process_known_host (ssh2, ctx, mcerror))
        goto err;

    if (!shell_ssh2_auth_agent (ssh2, ctx) && !shell_ssh2_auth_pubkey (ssh2, ctx)
        && !shell_ssh2_auth_password (ssh2, ctx))
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: authentication failed"));
        goto err;
    }

    libssh2_session_set_blocking (ssh2->session, 1);

    ssh2->channel = libssh2_channel_open_session (ssh2->session);
    if (ssh2->channel == NULL)
    {
        mc_propagate_error (mcerror, 0, "%s", _ ("shell: failed to open SSH channel"));
        goto err;
    }

    rc = libssh2_channel_exec (ssh2->channel, "echo SHELL:; /bin/sh");
    if (rc != 0)
    {
        mc_propagate_error (mcerror, rc, "%s", _ ("shell: failed to exec remote shell"));
        goto err;
    }

    return ssh2;

err:
    /* Clear the error - caller will handle fallback */
    if (mcerror != NULL && *mcerror != NULL)
    {
        g_error_free (*mcerror);
        *mcerror = NULL;
    }

    shell_ssh2_close (ssh2);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
shell_ssh2_close (shell_ssh2_t *ssh2)
{
    if (ssh2 == NULL)
        return;

    if (ssh2->channel != NULL)
    {
        libssh2_channel_close (ssh2->channel);
        libssh2_channel_free (ssh2->channel);
        ssh2->channel = NULL;
    }

    if (ssh2->agent != NULL)
    {
        libssh2_agent_disconnect (ssh2->agent);
        libssh2_agent_free (ssh2->agent);
        ssh2->agent = NULL;
    }

    if (ssh2->known_hosts != NULL)
    {
        libssh2_knownhost_free (ssh2->known_hosts);
        ssh2->known_hosts = NULL;
    }

    MC_PTR_FREE (ssh2->known_hosts_file);

    if (ssh2->session != NULL)
    {
        libssh2_session_disconnect (ssh2->session, "shell: closing connection");
        libssh2_session_free (ssh2->session);
        ssh2->session = NULL;
    }

    if (ssh2->socket_fd != LIBSSH2_INVALID_SOCKET)
    {
        close (ssh2->socket_fd);
        ssh2->socket_fd = LIBSSH2_INVALID_SOCKET;
    }

    /* Freed after the session, which held a pointer to it in its abstract. */
    if (ssh2->ctx != NULL)
    {
        g_free (ssh2->ctx->local_user);
        MC_PTR_FREE (ssh2->ctx);
    }

    g_free (ssh2);
}

/* --------------------------------------------------------------------------------------------- */

ssize_t
shell_ssh2_read (shell_ssh2_t *ssh2, void *buf, size_t len)
{
    ssize_t n;

    while (TRUE)
    {
        n = libssh2_channel_read (ssh2->channel, buf, len);
        if (n != LIBSSH2_ERROR_EAGAIN)
            break;
    }

    if (n < 0)
    {
        errno = EIO;
        return -1;
    }

    return n;
}

/* --------------------------------------------------------------------------------------------- */

ssize_t
shell_ssh2_write (shell_ssh2_t *ssh2, const void *buf, size_t len)
{
    const char *p = (const char *) buf;
    size_t remaining = len;

    while (remaining > 0)
    {
        ssize_t n;

        n = libssh2_channel_write (ssh2->channel, p, remaining);
        if (n == LIBSSH2_ERROR_EAGAIN)
            continue;
        if (n < 0)
        {
            errno = EIO;
            return -1;
        }
        p += n;
        remaining -= (size_t) n;
    }

    return (ssize_t) len;
}

/* --------------------------------------------------------------------------------------------- */

#endif /* ENABLE_SHELL_SSH2 */
