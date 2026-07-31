/*
   libshfs - file access over a shell connection.

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
 * \brief Header: file access over a shell connection
 *
 * The library drives a remote /bin/sh over ssh and parses the output of the
 * helper scripts. It knows nothing about mc: no widgets, no dialogs, no
 * terminal. Everything the caller must decide is asked through callbacks,
 * every failure is reported through GError.
 *
 * Rules that shape this interface:
 *
 *  - one connection performs one operation at a time;
 *  - cancellation and protocol errors close the connection, they are not
 *    recovered from; see shfs_conn_is_alive();
 *  - a file is never shortened except through SHFS_WRITE_TRUNCATE, which the
 *    caller has to name explicitly and which exists so that a human can ask
 *    for it;
 *  - a partially written file is a verifiable prefix, not garbage, and is
 *    never removed by this library.
 */

#ifndef MC__SHFS_H
#define MC__SHFS_H

#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/** Error domain for everything this library reports. */
#define SHFS_ERROR (shfs_error_quark ())

/*** enums ***************************************************************************************/

typedef enum
{
    SHFS_ERR_FAILED = 0,
    SHFS_ERR_CANCELLED,  // a callback asked to stop
    SHFS_ERR_CLOSED,     // the connection is no longer usable
    SHFS_ERR_PROTO,      // the remote side broke the protocol
    SHFS_ERR_NOT_FOUND,
    SHFS_ERR_PERMISSION_DENIED,
    SHFS_ERR_EXISTS
} shfs_error_t;

/**
 * Where the helper script actually in use came from.
 *
 * The lookup order is the user's per-host override, then the set installed
 * with the plugin, then the copy compiled into the binary.
 */
typedef enum
{
    SHFS_HELPER_BUILTIN = 0,
    SHFS_HELPER_SYSTEM,
    SHFS_HELPER_USER
} shfs_helper_source_t;

/**
 * How much of the conversation with the remote shell to write down.
 *
 * Off by default: nothing is opened and nothing is written until a caller
 * asks. Each level includes the ones before it.
 *
 * File contents are never written to the log at any level. A transfer is
 * recorded by direction and byte count only.
 */
typedef enum
{
    SHFS_LOG_OFF = 0,
    SHFS_LOG_ERRORS,    // failures, and the reason given for them
    SHFS_LOG_COMMANDS,  // + which script ran against which path, and its reply
    SHFS_LOG_TRAFFIC    // + the script text sent and every line read back
} shfs_log_level_t;

/**
 * Digest algorithms, ordered by strength. Values are bit flags so that the
 * set a host supports can be intersected with the set another host supports.
 */
typedef enum
{
    SHFS_DIGEST_NONE = 0,
    SHFS_DIGEST_CKSUM = 1 << 0,  // POSIX, present everywhere, weakest
    SHFS_DIGEST_MD5 = 1 << 1,
    SHFS_DIGEST_SHA256 = 1 << 2
} shfs_digest_algo_t;

typedef enum
{
    SHFS_TRANSPORT_AUTO = 0,  // libssh2 when built in and not rsh
    SHFS_TRANSPORT_LIBSSH2,
    SHFS_TRANSPORT_EXTERNAL_SSH  // spawns ssh, which owns the terminal
} shfs_transport_t;

typedef enum
{
    SHFS_HOSTKEY_UNKNOWN = 0,  // host is not in known_hosts
    SHFS_HOSTKEY_MISMATCH      // host is known but presents a different key
} shfs_hostkey_status_t;

typedef enum
{
    SHFS_HOSTKEY_REJECT = 0,  // abort the connection
    SHFS_HOSTKEY_TRUST_ONCE,  // accept, do not write known_hosts
    SHFS_HOSTKEY_TRUST_STORE  // accept and record in known_hosts
} shfs_hostkey_action_t;

/**
 * How a write positions itself in the destination file.
 *
 * SHFS_WRITE_TRUNCATE is the only operation in this library that can make a
 * file shorter. Never use it to recover from a failure.
 */
typedef enum
{
    SHFS_WRITE_TRUNCATE = 0,  // start at 0, discard whatever was there
    SHFS_WRITE_AT,            // write at the given offset, never truncate
    /** Append where the file currently ends. Unlike SHFS_WRITE_AT with the
        size as the offset, this asks the remote side instead of trusting a
        size learned earlier. */
    SHFS_WRITE_APPEND
} shfs_write_mode_t;

typedef enum
{
    SHFS_RESUME_APPEND = 0,  // continue; maps to SHFS_WRITE_AT
    SHFS_RESUME_RESTART,     // start over; maps to SHFS_WRITE_TRUNCATE
    SHFS_RESUME_CANCEL
} shfs_resume_action_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/**
 * A digest together with the algorithm that produced it. Values made by
 * different algorithms must never be compared.
 */
typedef struct
{
    shfs_digest_algo_t algo;  // SHFS_DIGEST_NONE when no digest was obtained
    char hex[129];
} shfs_digest_t;

/**
 * Everything the library may need from the caller while a connection is being
 * established. The library never draws anything itself and never touches the
 * terminal.
 *
 * The authentication members are used by the libssh2 transport only. An
 * external ssh talks to the terminal on its own, and asks for it through
 * terminal_acquire().
 *
 * All members may be NULL. A NULL callback means "cannot answer": the library
 * skips that authentication method instead of failing the connection, except
 * for hostkey, where a NULL callback is treated as SHFS_HOSTKEY_REJECT.
 */
typedef struct
{
    /** Called after the handshake, before authentication, and only when the
        key is unknown or has changed. */
    shfs_hostkey_action_t (*hostkey) (shfs_hostkey_status_t status, const char *host,
                                      const char *fingerprint, void *user_data);

    /** Password for @user. Returning NULL cancels the connection. The library
        frees the returned string. @retry is TRUE when a stored password was
        already tried and rejected. */
    char *(*password) (const char *host, const char *user, gboolean retry, void *user_data);

    /** Passphrase for a key file. Returning NULL skips that key. */
    char *(*passphrase) (const char *keyfile, void *user_data);

    /** keyboard-interactive: @n prompts in, @n answers out, or NULL to cancel.
        The library frees the array and the strings in it. */
    char **(*kbd_interactive) (const char *name, const char *instruction,
                               const char *const *prompts, const gboolean *echo, int n,
                               void *user_data);

    /** Called once a secret has been accepted by the server, so that the
        caller can remember it and stop asking for the rest of the session.
        The library keeps no copy of it. */
    void (*password_accepted) (const char *password, void *user_data);

    /** Human-readable progress during connect: resolving, connecting,
        "permanently added to known hosts" and the like. Purely
        informational. */
    void (*status) (const char *text, void *user_data);

    /** Polled while the library waits on the network. Returning TRUE aborts
        the connection with G_IO_ERROR_CANCELLED. */
    gboolean (*cancelled) (void *user_data);

    /** The library is about to run an external ssh, which reads passwords and
        host key answers from /dev/tty. Give up the screen in terminal_acquire()
        and take it back in terminal_release(). Which transport is used cannot
        be settled before connecting - libssh2 may fail and fall back - so the
        library asks rather than the caller predicting. */
    void (*terminal_acquire) (void *user_data);
    void (*terminal_release) (void *user_data);

    void *user_data;
} shfs_connect_cb_t;

typedef struct
{
    const char *host;
    const char *user;  // NULL -> the local user name
    /** Stored secret. Offered first as the passphrase of each candidate key,
        and only then as an account password. */
    const char *password;
    int port;             // 0 -> default
    gboolean compressed;  // ssh -C
    gboolean use_rsh;
    shfs_transport_t transport;  // SHFS_TRANSPORT_AUTO unless forced
} shfs_conn_params_t;

typedef struct
{
    char *name;
    struct stat st;
    char *linkname;  // NULL unless a symbolic link
} shfs_entry_t;

/** Returning FALSE cancels the transfer, which closes the connection. */
typedef gboolean (*shfs_progress_cb) (gint64 done, gint64 total, void *user_data);

/** Asked when the destination already exists and is shorter than the source. */
typedef shfs_resume_action_t (*shfs_resume_cb) (const char *path, gint64 have, gint64 total,
                                                void *user_data);

typedef struct
{
    shfs_progress_cb progress;
    shfs_resume_cb resume;  // NULL -> the transfer fails instead of guessing
    void *user_data;
} shfs_transfer_cb_t;

typedef struct shfs_conn_t shfs_conn_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* --- transport, decidable without connecting --- */

/** Which transport these parameters will actually use. Depends only on the
    build configuration and @params. */
shfs_transport_t shfs_transport_resolve (const shfs_conn_params_t *params);

/* --- session --- */

shfs_conn_t *shfs_conn_open (const shfs_conn_params_t *params, const shfs_connect_cb_t *cb,
                             GError **error);
void shfs_conn_close (shfs_conn_t *conn);

/** FALSE after a cancellation or a protocol error. Every further call on such
    a connection fails with G_IO_ERROR_CLOSED. */
gboolean shfs_conn_is_alive (const shfs_conn_t *conn);

/** Bit set of the digest algorithms this host can run, probed at connect. */
shfs_digest_algo_t shfs_conn_digest_algos (const shfs_conn_t *conn);

/** Strongest algorithm present in both sets, or SHFS_DIGEST_NONE. Use it to
    pick one algorithm for a transfer that spans two connections. */
shfs_digest_algo_t shfs_digest_common (shfs_digest_algo_t a, shfs_digest_algo_t b);

/* --- directories --- */

/** Returns a GPtrArray of shfs_entry_t *, or NULL on failure. Free it with
    shfs_entries_free(), which frees the elements too. */
GPtrArray *shfs_list_dir (shfs_conn_t *conn, const char *path, GError **error);
void shfs_entries_free (GPtrArray *entries);

/* --- whole file transfer --- */

/* No whole-file call: the streaming calls below leave progress reporting,
   resuming and the destination to the caller. */

/* --- streaming transfer --- */

/**
 * Read @path from @offset. @remaining receives how many bytes will be
 * delivered.  @max_bytes above zero caps that at the far end - a preview then
 * costs one screenful instead of the whole file.  A helper older than get 3
 * ignores the cap and sends everything; @remaining still says what follows.
 *
 * shfs_get_read() returns the number of bytes read, 0 at end of stream, and
 * -1 on error. shfs_get_finish() may only be called once the stream has ended,
 * that is after exactly @remaining bytes have been read or 0 was returned;
 * calling it earlier closes the connection, because the channel would be left
 * out of step with the protocol.
 */
gboolean shfs_get_begin (shfs_conn_t *conn, const char *path, gint64 offset, gint64 max_bytes,
                         gint64 *remaining, GError **error);
gssize shfs_get_read (shfs_conn_t *conn, void *buf, gsize size, GError **error);
gboolean shfs_get_finish (shfs_conn_t *conn, shfs_digest_t *digest, GError **error);

/**
 * Write to @path.
 *
 * @mode says how the write positions itself; @offset is used only with
 * SHFS_WRITE_AT and is ignored otherwise.
 *
 * @segment_size is how many bytes this call will write. @final_size is how
 * long the file is expected to be when the write completes. They differ
 * whenever a write starts anywhere but at zero, and the pair is what lets the
 * library tell a resumed transfer from an ordinary append: for a resume of a
 * file that already held @base bytes of wanted data, the source is read from
 * n while the destination is written at base + n, so @segment_size is
 * source_size - n and @final_size is base + source_size.
 *
 * shfs_put_finish() closes the stream and reports the digest of the file as it
 * now stands on disk, computed with the algorithm passed to shfs_put_begin().
 */
gboolean shfs_put_begin (shfs_conn_t *conn, const char *path, shfs_write_mode_t mode, gint64 offset,
                         gint64 segment_size, gint64 final_size, shfs_digest_algo_t algo,
                         GError **error);
gboolean shfs_put_write (shfs_conn_t *conn, const void *buf, gsize size, GError **error);
gboolean shfs_put_finish (shfs_conn_t *conn, shfs_digest_t *digest, GError **error);

/* --- integrity --- */

gboolean shfs_checksum_range (shfs_conn_t *conn, const char *path, gint64 offset, gint64 length,
                              shfs_digest_algo_t algo, shfs_digest_t *digest, GError **error);

/**
 * Digests of consecutive blocks of @block bytes, used to locate where a
 * partially written file diverges from its source.
 *
 * @digests receives a GPtrArray of shfs_digest_t *, in order, with a free
 * function set: g_ptr_array_unref() releases the elements as well.
 */
gboolean shfs_block_digests (shfs_conn_t *conn, const char *path, gint64 offset, gint64 length,
                             gint64 block, shfs_digest_algo_t algo, GPtrArray **digests,
                             GError **error);

/**
 * A digest of a range of a local file, in the form the remote helpers print,
 * so that the two can be compared as strings. Supports every algorithm the
 * protocol negotiates, including the POSIX cksum that GLib does not know.
 */
gboolean shfs_local_digest_range (const char *path, gint64 offset, gint64 length,
                                  shfs_digest_algo_t algo, shfs_digest_t *digest, GError **error);

/**
 * How many bytes of a half-written file may be kept.
 *
 * @remote and @local name the two sides; which of them is the source and
 * which the destination does not matter, the question is only how far they
 * agree. @dest_size is what is already at the destination, @source_size what
 * the whole file should be.
 *
 * Returns the length of a prefix that is certainly correct - possibly 0 -
 * or -1 when the transfer cannot be continued at all, which happens when the
 * destination is already at least as long as the source. Nothing here
 * truncates or deletes anything.
 */
gint64 shfs_resume_probe (shfs_conn_t *conn, const char *remote, const char *local,
                          gint64 source_size, gint64 dest_size, GError **error);

/* --- operations --- */

/** TRUE when @path exists on the remote host. A missing file is not an error,
    so @error stays unset for that case. */
gboolean shfs_exists_path (shfs_conn_t *conn, const char *path, GError **error);
/** Metadata of a remote file. FALSE when it is not there. Comes from a
    directory listing, since the protocol has no stat of its own. */
gboolean shfs_file_stat (shfs_conn_t *conn, const char *path, struct stat *st, GError **error);
/** Its size alone, or -1 when it is not there. */
gint64 shfs_file_size (shfs_conn_t *conn, const char *path, GError **error);
gboolean shfs_unlink_path (shfs_conn_t *conn, const char *path, GError **error);
gboolean shfs_mkdir_path (shfs_conn_t *conn, const char *path, mode_t mode, GError **error);
gboolean shfs_rmdir_path (shfs_conn_t *conn, const char *path, GError **error);
gboolean shfs_rename_path (shfs_conn_t *conn, const char *from, const char *to, GError **error);
gboolean shfs_chmod_path (shfs_conn_t *conn, const char *path, mode_t mode, GError **error);
gboolean shfs_chown_path (shfs_conn_t *conn, const char *path, uid_t owner, gid_t group,
                          GError **error);
gboolean shfs_utime_path (shfs_conn_t *conn, const char *path, time_t atime, time_t mtime,
                          GError **error);
gboolean shfs_symlink_path (shfs_conn_t *conn, const char *target, const char *link,
                            GError **error);
gboolean shfs_hardlink_path (shfs_conn_t *conn, const char *target, const char *link,
                             GError **error);

GQuark shfs_error_quark (void);

/* --- helper scripts --- */

typedef struct
{
    char *name;                   // file name, "ls" and friends
    shfs_helper_source_t source;  // which of the three is in effect
    char *path;                   // file it was read from, NULL when built in
    gsize size;
    int version;           // what the file declares; 0 means it says nothing
    int expected_version;  // what this build was written against
} shfs_helper_t;

/** Every helper, in protocol order, each reporting where its effective
    version comes from for @hostname. Free with shfs_helpers_free(). */
GPtrArray *shfs_helpers_list (const char *hostname);
void shfs_helpers_free (GPtrArray *list);

/** The text that would actually run, and optionally where it came from.
    NULL only when @name is not a helper at all. */
char *shfs_helper_content (const char *hostname, const char *name, shfs_helper_source_t *source);
/** The copy compiled into the binary, or NULL if @name is not a helper. Not
    to be freed. */
const char *shfs_helper_builtin (const char *name);
/** Where an override for @hostname lives, whether or not it exists yet. */
char *shfs_helper_user_path (const char *hostname, const char *name);
/** Where the set installed with the plugin lives. */
char *shfs_helper_system_path (const char *name);

/**
 * Overrides in use on this connection that were written against an older
 * revision than this build expects, as shfs_helper_t *. Empty when everything
 * is current; owned by the connection.
 *
 * An old helper does not fail: it answers with less than it should, so the
 * caller is expected to warn once per connection.
 */
const GPtrArray *shfs_conn_stale_helpers (const shfs_conn_t *conn);

/* --- logging --- */

/** Open @path for appending and start writing at @level, or stop logging when
    @level is SHFS_LOG_OFF. Process wide, not per connection. */
gboolean shfs_log_set (shfs_log_level_t level, const char *path, GError **error);
shfs_log_level_t shfs_log_get_level (void);
/** The file being written to, or NULL when logging is off. */
const char *shfs_log_get_path (void);

/** Write one line. Public so that the caller can put its own events into the
    same file, in order. */
void shfs_log_printf (shfs_log_level_t level, const char *fmt, ...) G_GNUC_PRINTF (2, 3);
/** Write a block of text with unprintable bytes escaped, truncated at 4 KiB. */
void shfs_log_blob (shfs_log_level_t level, const char *tag, const char *data, gsize len);

/*** inline functions ****************************************************************************/

#endif
