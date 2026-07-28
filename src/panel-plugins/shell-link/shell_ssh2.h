/*
   libshfs - libssh2 transport for the shell connection library.

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
 * \brief Header: libssh2 transport for the shell connection library
 */

#ifndef MC__SHELL_SSH2_H
#define MC__SHELL_SSH2_H

#ifdef ENABLE_SHELL_SSH2

#include <libssh2.h>

#include "shfs.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct shell_ssh2_ctx;

typedef struct
{
    int socket_fd;
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    LIBSSH2_KNOWNHOSTS *known_hosts;
    char *known_hosts_file;
    LIBSSH2_AGENT *agent;
    /** Owned. Lives as long as the session, because the session abstract
        points at it. */
    struct shell_ssh2_ctx *ctx;
} shell_ssh2_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/**
 * Open a transport to the host described by @params.
 *
 * The host key decision, passwords, passphrases, keyboard-interactive answers,
 * progress and cancellation all go through @cb.
 */
shell_ssh2_t *shell_ssh2_open (const shfs_conn_params_t *params, const shfs_connect_cb_t *cb,
                               GError **mcerror);
void shell_ssh2_close (shell_ssh2_t *ssh2);
ssize_t shell_ssh2_read (shell_ssh2_t *ssh2, void *buf, size_t len);
ssize_t shell_ssh2_write (shell_ssh2_t *ssh2, const void *buf, size_t len);

/*** inline functions ****************************************************************************/

#endif /* ENABLE_SHELL_SSH2 */

#endif /* MC__SHELL_SSH2_H */
