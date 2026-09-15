/*
   lib - mc_pread() while signals arrive

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
 */

#define TEST_SUITE_NAME "/lib/util"

#include "tests/mctest.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lib/util.h"

/* --------------------------------------------------------------------------------------------- */

#define OUTPUT_BYTES 4000000
#define BLOCK_BYTES  65536
#define SIGNAL_COUNT 200

static volatile sig_atomic_t signals_seen = 0;

/* --------------------------------------------------------------------------------------------- */

static void
winch_handler (int sig)
{
    (void) sig;
    signals_seen++;
}

/* --------------------------------------------------------------------------------------------- */

/* A child of our own keeps sending the signal a window resize sends. */
static pid_t
start_signal_source (pid_t target)
{
    pid_t pid;

    pid = fork ();
    if (pid == 0)
    {
        int i;

        for (i = 0; i < SIGNAL_COUNT; i++)
        {
            kill (target, SIGWINCH);
            usleep (2000);
        }
        _exit (0);
    }

    return pid;
}

/* --------------------------------------------------------------------------------------------- */

/* The bytes the child sends back, written where every system can cat them. */
static char *
make_data_file (void)
{
    char *path = NULL;
    char *block;
    int fd;
    gsize written = 0;

    fd = g_file_open_tmp ("mc-pread-XXXXXX", &path, NULL);
    ck_assert_msg (fd >= 0, "cannot create a temporary file");

    block = g_malloc (BLOCK_BYTES);
    memset (block, 'x', BLOCK_BYTES);

    while (written < OUTPUT_BYTES)
    {
        const gsize want = MIN ((gsize) BLOCK_BYTES, (gsize) OUTPUT_BYTES - written);
        ssize_t done;

        do
        {
            done = write (fd, block, want);
        }
        while (done < 0 && errno == EINTR);

        ck_assert_msg (done > 0, "cannot write the temporary file");
        written += (gsize) done;
    }

    g_free (block);
    close (fd);

    return path;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (mc_pread_survives_signals_test)
{
    struct sigaction act, oact;
    mc_pipe_t *pip;
    GError *error = NULL;
    pid_t signals;
    gsize total = 0;
    gboolean out_done = FALSE;
    gboolean err_done = FALSE;
    char *path;
    char *quoted;
    char *command;

    memset (&act, 0, sizeof (act));
    act.sa_handler = winch_handler;
    sigemptyset (&act.sa_mask);
#ifdef SA_RESTART
    act.sa_flags = SA_RESTART;
#endif
    sigaction (SIGWINCH, &act, &oact);

    /* Much more than a pipe holds, and not a byte on stderr: the child cannot
       finish until its stdout is read, so a read of the wrong stream never
       returns. */
    path = make_data_file ();
    quoted = g_shell_quote (path);
    command = g_strconcat ("cat -- ", quoted, NULL);
    g_free (quoted);
    pip = mc_popen (command, TRUE, TRUE, &error);
    g_free (command);
    ck_assert_msg (pip != NULL, "mc_popen() failed");

    signals = start_signal_source (getpid ());

    while (!out_done || !err_done)
    {
        pip->out.len = MC_PIPE_BUFSIZE;
        pip->err.len = MC_PIPE_BUFSIZE;
        mc_pread (pip, &error);
        ck_assert_msg (error == NULL, "mc_pread() failed");

        if (!out_done && pip->out.len > 0)
            total += (gsize) pip->out.len;
        else if (!out_done
                 && (pip->out.len == MC_PIPE_STREAM_EOF || pip->out.len == MC_PIPE_ERROR_READ))
        {
            out_done = TRUE;
            close (pip->out.fd);
            pip->out.fd = -1;
        }

        if (!err_done && pip->err.len <= 0
            && (pip->err.len == MC_PIPE_STREAM_EOF || pip->err.len == MC_PIPE_ERROR_READ))
        {
            err_done = TRUE;
            close (pip->err.fd);
            pip->err.fd = -1;
        }
    }

    ck_assert_msg (total == OUTPUT_BYTES, "read %lu bytes of %d", (unsigned long) total,
                   OUTPUT_BYTES);

    mc_pclose (pip, &error);
    g_clear_error (&error);
    unlink (path);
    g_free (path);

    if (signals > 0)
    {
        kill (signals, SIGKILL);
        waitpid (signals, NULL, 0);
    }
    sigaction (SIGWINCH, &oact, NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_set_timeout (tc_core, 30);

    // Add new tests here: ***************
    tcase_add_test (tc_core, mc_pread_survives_signals_test);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
