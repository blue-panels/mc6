/*
   Midnight Commander - mcterm PTY terminal widget.

   Embeds a shell inside the filemanager as a PTY-backed terminal widget
   with vterm emulation and OSC 7 panel synchronization.

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

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/timerfd.h>
#endif
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_PTY_H
#include <pty.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif

#include "lib/global.h"
#include "lib/strutil.h"
#include "lib/widget.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/skin.h"

#include "src/viewer/vterm.h"
#include "src/viewer/terminal_buffer.h"
#include "src/keymap.h"

#include "mcterm.h"
#include "mcterm_filter.h"
#include "mcterm_key.h"
#include "mcterm_proto.h"
#include "mcterm_select.h"
#include "mcterm_setup.h"

/*** file scope variables ************************************************************************/

#define MCTERM_INTERNAL_SYNC_TIMEOUT_USEC G_USEC_PER_SEC

#define MCTERM_INITIAL_OSC7_MARKER        "7;file://__mc_sync__/"
#define MCTERM_LINE_SETTLE_USEC           (150L * 1000L)
#define MCTERM_WHEEL_ROWS                 3
/* How often the host is woken while a command runs, in nanoseconds. */
#define MCTERM_BUSY_TICK_NSEC (200L * 1000L * 1000L)

/* How far Ctrl-Left and Ctrl-Right take the cursor: a tab stop, which is the
   step the columns of terminal output tend to fall on. */
#define MCTERM_JUMP_COLS 8

struct WMcTerm
{
    Widget base;
    mcview_vterm_t *vterm;
    int pty_master;
    pid_t child_pid;
    gboolean child_dead;
    int child_exit_status;
    /* Valid only when osc7_capable is TRUE. */
    gboolean shell_at_prompt;
    guint last_osc7_gen;
    gboolean osc7_capable;
    /* Marks our own OSC 7 and our own prompt marks: anything without it belongs to someone else. */
    char *osc7_token;
    /* Whether the shell sends semantic prompt marks. With them the prompt says where it ends,
       and OSC 7 goes back to being about the directory alone. */
    gboolean osc133_capable;
    guint last_osc133_gen;
    /* Where the shell left its cursor when it finished drawing the prompt: the point at which
       typing begins. The line is empty while nothing is drawn from there on. */
    gint64 input_start_row;
    int input_start_col;
    gboolean input_start_valid;
    /* Keys sent to the line are not on the screen until the shell echoes them: the line was
       told to go, or was typed on, and the screen has yet to say so. */
    gboolean line_cleared;
    gboolean line_typed;
    int last_exit_code;
    /* Command submitted by the host, used before its process group becomes visible. */
    char *command_hint;
    /* A command of ours is under way and its "done" mark has yet to arrive. */
    gboolean awaiting_command_done;
    /* While a command runs the host shows that something is going on, and needs waking to
       move it along: a timer does that, and so does any output the command makes. */
    int busy_tick_fd;
    guint busy_phase;
    void (*on_busy_tick) (void *data);
    void *on_busy_tick_data;
    void (*on_prompt_ready) (void *data);
    void *on_prompt_ready_data;
    void (*on_after_redraw) (void *data);
    void *on_after_redraw_data;
    gboolean pending_internal_sync;
    gboolean waiting_for_initial_osc7;
    mcview_terminal_buffer_t *sync_snapshot_buf; /* owned, freed in mcterm_free */
    int sync_snapshot_cursor_row;
    gint64 internal_sync_deadline;
    int scrollback;  // rows above the live screen; 0 follows the output
    gboolean scroll_allowed;
    // Whether the host types elsewhere; without that the arrows are the shell's.
    gboolean typing_elsewhere;
    mcterm_sel_t sel;
    /* Only the rows that matched are drawn, the view moving among them alone. */
    mcterm_filter_t filter;
    char *last_filter;  // what was filtered by last, for the filter to go back on
    /* Where the terminal is being read, as against where the shell is typing.
       It exists while the widget has the focus, and the arrows move it. */
    gboolean cursor_valid;
    gint64 cursor_row;
    int cursor_col;
    /* Sixel pictures are written to the terminal after the cells, and the
       cells under them are written every time, so that a picture that moved
       or went is erased. */
    gboolean paint_pending;   // the cells were drawn: paint after the refresh
    gboolean pictures_shown;  // pictures were painted the last time
};

/* Where the rows of the screen fall in the output as a whole. */
typedef struct
{
    gboolean compose;   // history and live rows are drawn as one view
    gboolean snapshot;  // a held-back frame is on the screen
    gboolean filtered;  // the rows that matched are drawn, and nothing else
    mcview_terminal_buffer_t *buf;
    int top_row;       // first live row on the screen
    int content_rows;  // rows of output drawn
    int blank_above;   // empty rows above them
    gint64 first_abs;  // number of the row drawn at @blank_above
    // The last row it draws: at a prompt the shell's own row is the host's.
    gint64 newest_abs;
} mcterm_geom_t;

/*** forward declarations ************************************************************************/

static cb_ret_t mcterm_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data);
static void mcterm_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event);
static gboolean mcterm_handle_osc7_generation (WMcTerm *t);
static int mcterm_pty_ready_cb (int fd, void *info);
static gboolean mcterm_osc7_is_ours (const WMcTerm *t, const char *raw);
static gboolean mcterm_handle_osc133_generation (WMcTerm *t);
static gboolean mcterm_write_all (int master, const unsigned char *data, size_t len);
static void mcterm_busy_tick (WMcTerm *t);
static void mcterm_busy_tick_set (WMcTerm *t, gboolean on);
static gboolean mcterm_handle_stalled_internal_sync (WMcTerm *t);
static int mcterm_resolve_top_row_for_buf (const WMcTerm *t, const mcview_terminal_buffer_t *buf,
                                           int rows);

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */

static const char *
mcterm_command_basename (const char *path)
{
    const char *slash = (path != NULL) ? strrchr (path, PATH_SEP) : NULL;

    return (slash != NULL) ? slash + 1 : path;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_command_is_interpreter (const char *name)
{
    static const char *const interpreters[] = { "python", "perl", "ruby", "node", "php",
                                                "bash",   "dash", "zsh",  "fish", NULL };
    const char *const *p;

    for (p = interpreters; *p != NULL; p++)
        if (g_str_has_prefix (name, *p))
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
mcterm_command_ellipsize (const char *text, int max_width)
{
    char *prefix;
    char *result;

    if (max_width <= 0)
        return g_strdup ("");
    if (str_term_width1 (text) <= max_width)
        return g_strdup (text);
    if (max_width <= 3)
        return g_strndup ("...", (gsize) max_width);

    prefix = g_strdup (str_term_substring (text, 0, max_width - 3));
    g_strchomp (prefix);
    result = g_strconcat (prefix, "...", (char *) NULL);
    g_free (prefix);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static char *
mcterm_command_from_argv (const char *const *argv, gsize argc, int max_width)
{
    const char *name;
    char *description;
    char *result;

    if (argv == NULL || argc == 0 || argv[0] == NULL || *argv[0] == '\0')
        return NULL;

    name = mcterm_command_basename (argv[0]);
    if (mcterm_command_is_interpreter (name) && argc > 1 && argv[1] != NULL && argv[1][0] != '-'
        && argv[1][0] != '\0')
        description = g_strdup_printf ("%s %s", name, mcterm_command_basename (argv[1]));
    else if (argc > 4)
        description = g_strdup_printf ("%s ...", name);
    else
        description = g_strdup (name);

    result = mcterm_command_ellipsize (description, max_width);
    g_free (description);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static char *
mcterm_command_from_line (const char *command, int max_width)
{
    int argc = 0;
    char **argv = NULL;
    char *result = NULL;

    if (command != NULL && g_shell_parse_argv (command, &argc, &argv, NULL))
    {
        result = mcterm_command_from_argv ((const char *const *) argv, (gsize) argc, max_width);
        g_strfreev (argv);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef __linux__

static char *
mcterm_command_from_process (pid_t pid, int max_width)
{
    char path[64];
    char *contents = NULL;
    gsize len = 0;
    GPtrArray *args;
    gsize pos;
    char *result = NULL;

    g_snprintf (path, sizeof (path), "/proc/%ld/cmdline", (long) pid);
    if (!g_file_get_contents (path, &contents, &len, NULL) || len == 0)
        goto ret;

    args = g_ptr_array_new ();
    for (pos = 0; pos < len;)
    {
        gsize arg_len = strnlen (contents + pos, len - pos);

        if (arg_len == 0)
            break;
        g_ptr_array_add (args, contents + pos);
        pos += arg_len + 1;
    }

    result = mcterm_command_from_argv ((const char *const *) args->pdata, args->len, max_width);
    g_ptr_array_free (args, TRUE);

ret:
    g_free (contents);
    return result;
}

#endif /* __linux__ */

static int
mcterm_pty_ready_cb (int fd, void *info)
{
    WMcTerm *t = (WMcTerm *) info;
    unsigned char buf[65536];
    ssize_t n;
    gboolean suppress_draw = FALSE;

    (void) fd;

    /* Single non-retrying read: EINTR just loses one wakeup; the next
       select() re-triggers, so no data is lost and no retry is needed. */
    n = read (t->pty_master, buf, sizeof (buf));

    if (n > 0)
    {
        ssize_t i;
        int history_before = mcview_vterm_history_len (t->vterm);

        for (i = 0; i < n; i++)
        {
            vterm_event_t ev;

            ev = mcview_vterm_feed (t->vterm, buf[i]);
            mcview_vterm_apply_event (t->vterm, &ev);

            // the shell asked what the terminal is; the emulator wrote the answer
            if (ev.type == VTERM_REPLY && ev.reply != NULL && t->pty_master >= 0)
            {
                ssize_t ignored;

                ignored = write (t->pty_master, ev.reply, strlen (ev.reply));
                (void) ignored;
            }
            mcterm_handle_osc7_generation (t);
            mcterm_handle_osc133_generation (t);
        }

        t->line_cleared = FALSE;
        t->line_typed = FALSE;

        // New output must not drag the view away from what is being read.
        if (t->scrollback > 0)
        {
            int grown = mcview_vterm_history_len (t->vterm) - history_before;

            if (grown > 0)
                t->scrollback = MIN (t->scrollback + grown, mcview_vterm_history_len (t->vterm));
        }

        suppress_draw = !mcterm_handle_stalled_internal_sync (t);

        if (widget_get_state (WIDGET (t), WST_VISIBLE) && !suppress_draw)
        {
            widget_draw (WIDGET (t));
            if (t->shell_at_prompt && t->osc7_capable && t->on_prompt_ready != NULL)
                t->on_prompt_ready (t->on_prompt_ready_data);
            else
                send_message (WIDGET (t), NULL, MSG_CURSOR, 0, NULL);
            if (t->on_after_redraw != NULL)
                t->on_after_redraw (t->on_after_redraw_data);
            tty_refresh ();
        }
        else if (!suppress_draw && !t->shell_at_prompt && t->osc7_capable && t->busy_tick_fd < 0)
        {
            /* Hidden and busy, with no timer to lean on: the command's own output is what
               moves the indicator the host shows. Where a timer runs, it does that instead,
               so that a silent command moves it too - and the two do not both drive it. */
            mcterm_busy_tick (t);
        }
        else if (!suppress_draw && t->shell_at_prompt && t->osc7_capable
                 && t->on_prompt_ready != NULL)
        {
            /* Hidden behind the panels, the terminal has nothing to draw - but its prompt is
               on screen all the same, in front of the command line, and that row is the
               host's to draw. */
            t->on_prompt_ready (t->on_prompt_ready_data);
        }
    }
    else if (n == 0 || (n < 0 && errno == EIO))
    {
        t->child_dead = TRUE;
        mcterm_busy_tick_set (t, FALSE);
        delete_select_channel (t->pty_master);
        close (t->pty_master);
        t->pty_master = -1;
        if (t->child_pid > 0)
        {
            if (waitpid (t->child_pid, &t->child_exit_status, WNOHANG) > 0)
                t->child_pid = -1;
        }
        if (widget_get_state (WIDGET (t), WST_VISIBLE))
        {
            widget_draw (WIDGET (t));
            if (t->on_after_redraw != NULL)
                t->on_after_redraw (t->on_after_redraw_data);
            tty_refresh ();
        }
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/**
 * Whether an OSC 7 is the one our own shell sends.
 *
 * The setup we write into the pty appends the session token to every OSC 7. Without it the
 * sequence came from somewhere else - the output of a command, or a shell on the far side of
 * ssh - and says nothing about the directory this terminal is in.
 */

static gboolean
mcterm_osc7_is_ours (const WMcTerm *t, const char *raw)
{
    const char *mark;

    if (t->osc7_token == NULL)
        return TRUE;

    if (raw == NULL)
        return FALSE;

    mark = strrchr (raw, '?');

    return (mark != NULL
            && strncmp (mark, MCTERM_OSC7_TOKEN_PREFIX, sizeof (MCTERM_OSC7_TOKEN_PREFIX) - 1) == 0
            && strcmp (mark + sizeof (MCTERM_OSC7_TOKEN_PREFIX) - 1, t->osc7_token) == 0);
}

/* --------------------------------------------------------------------------------------------- */

/* Keep snapshot restore ordered with OSC 7 inside a PTY read. */
static gboolean
mcterm_handle_osc7_generation (WMcTerm *t)
{
    const char *raw;
    guint gen;

    if (!t->osc7_capable)
        return FALSE;

    gen = mcview_vterm_osc7_generation (t->vterm);
    if (gen == t->last_osc7_gen)
        return FALSE;

    t->last_osc7_gen = gen;

    raw = mcview_vterm_osc7_raw (t->vterm);
    if (!mcterm_osc7_is_ours (t, raw))
        return FALSE;

    if (t->waiting_for_initial_osc7)
    {
        if (raw == NULL
            || strncmp (raw, MCTERM_INITIAL_OSC7_MARKER, sizeof (MCTERM_INITIAL_OSC7_MARKER) - 1)
                != 0)
            return FALSE;

        t->waiting_for_initial_osc7 = FALSE;
        t->internal_sync_deadline = 0;

        if (t->pending_internal_sync)
        {
            t->pending_internal_sync = FALSE;
            if (t->sync_snapshot_buf != NULL)
            {
                mcview_vterm_set_keep_history (t->vterm, FALSE);
                mcview_vterm_set_keep_history (t->vterm, TRUE);
                mcview_vterm_restore_sync_snapshot (t->vterm, t->sync_snapshot_buf,
                                                    t->sync_snapshot_cursor_row);
                t->sync_snapshot_buf = NULL;
            }
        }

        return FALSE;
    }

    /* With prompt marks the shell says itself when it is done printing the prompt; OSC 7
       arrives before that and would call it ready too early. */
    if (!t->osc133_capable)
        t->shell_at_prompt = TRUE;
    t->internal_sync_deadline = 0;

    if (t->pending_internal_sync)
    {
        t->pending_internal_sync = FALSE;
        if (t->sync_snapshot_buf != NULL)
        {
            mcview_vterm_restore_sync_snapshot (t->vterm, t->sync_snapshot_buf,
                                                t->sync_snapshot_cursor_row);
            t->sync_snapshot_buf = NULL;
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_busy_tick (WMcTerm *t)
{
    t->busy_phase++;
    if (t->on_busy_tick != NULL)
        t->on_busy_tick (t->on_busy_tick_data);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef __linux__
static int
mcterm_busy_tick_cb (int fd, void *data)
{
    WMcTerm *t = (WMcTerm *) data;
    guint64 ticks;
    ssize_t ignored;

    ignored = read (fd, &ticks, sizeof (ticks));
    (void) ignored;

    mcterm_busy_tick (t);

    return 0;
}
#endif

/* --------------------------------------------------------------------------------------------- */
/**
 * Run a timer for as long as a command does.
 *
 * mc has no clock of its own: what it waits on is a set of file descriptors. A timer that is
 * one of them wakes the loop on its own account, so whatever the host draws while a command
 * runs keeps moving even when the command says nothing. Where there is no such timer, the
 * output of the command is what moves it.
 */

static void
mcterm_busy_tick_set (WMcTerm *t, gboolean on)
{
#ifdef __linux__
    if (on && t->busy_tick_fd < 0)
    {
        struct itimerspec its;
        int fd;

        fd = timerfd_create (CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (fd < 0)
            return;

        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = MCTERM_BUSY_TICK_NSEC;
        its.it_value = its.it_interval;

        if (timerfd_settime (fd, 0, &its, NULL) != 0)
        {
            close (fd);
            return;
        }

        t->busy_tick_fd = fd;
        add_select_channel (fd, mcterm_busy_tick_cb, t);
    }
    else if (!on && t->busy_tick_fd >= 0)
    {
        delete_select_channel (t->busy_tick_fd);
        close (t->busy_tick_fd);
        t->busy_tick_fd = -1;
    }
#else
    (void) t;
    (void) on;
#endif
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Follow the shell through its prompt marks.
 *
 * @return TRUE when the shell has just come back to its prompt.
 */

static gboolean
mcterm_handle_osc133_generation (WMcTerm *t)
{
    mcterm_osc133_t mark;
    guint gen;

    if (!t->osc133_capable)
        return FALSE;

    gen = mcview_vterm_osc133_generation (t->vterm);
    if (gen == t->last_osc133_gen)
        return FALSE;

    t->last_osc133_gen = gen;

    if (!mcterm_osc133_parse (mcview_vterm_osc133_raw (t->vterm), t->osc7_token, &mark))
        return FALSE;

    switch (mark.mark)
    {
    case MCTERM_MARK_PROMPT_END:
        /* A prompt is printed again whenever the line editor redraws it - among other times,
           right after we hand it a command. Until the shell reports that command done, a
           prompt on screen is a redrawn one and the shell is still busy. */
        if (t->awaiting_command_done)
            return FALSE;
        /* Wherever the prompt ends is where the typed line begins - also for a prompt printed
           afresh while the shell was already at one: Enter on an empty line runs no command,
           so nothing else moves the line down to the new prompt. */
        t->input_start_row =
            mcview_vterm_scrolled_rows (t->vterm) + mcview_vterm_cursor_row (t->vterm);
        t->input_start_col = mcview_vterm_cursor_col (t->vterm);
        t->input_start_valid = TRUE;
        if (t->shell_at_prompt)
            return FALSE;
        t->shell_at_prompt = TRUE;
        g_clear_pointer (&t->command_hint, g_free);
        mcterm_busy_tick_set (t, FALSE);
        return TRUE;

    case MCTERM_MARK_COMMAND_START:
        t->shell_at_prompt = FALSE;
        t->input_start_valid = FALSE;
        mcterm_busy_tick_set (t, TRUE);
        return FALSE;

    case MCTERM_MARK_COMMAND_DONE:
        t->awaiting_command_done = FALSE;
        if (mark.exit_code >= 0)
            t->last_exit_code = mark.exit_code;
        return FALSE;

    default:
        return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_handle_stalled_internal_sync (WMcTerm *t)
{
    if (!t->pending_internal_sync)
        return TRUE;

    /* The startup setup is guaranteed to end with OSC 7.  PTY reads are
       unrelated to how far Bash has got through its startup, so keep its
       setup hidden until that marker arrives. */
    if (t->waiting_for_initial_osc7)
        return FALSE;

    if (g_get_monotonic_time () < t->internal_sync_deadline)
        return FALSE;

    t->pending_internal_sync = FALSE;
    t->internal_sync_deadline = 0;
    mcview_terminal_buffer_free (t->sync_snapshot_buf);
    t->sync_snapshot_buf = NULL;
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
mcterm_resolve_top_row_for_buf (const WMcTerm *t, const mcview_terminal_buffer_t *buf, int rows)
{
    int top;
    int max;

    top = mcview_vterm_dpy_top_row (t->vterm);
    if (top >= 0)
        return top;

    max = mcview_terminal_buffer_max_row (buf);
    if (max < 0)
        return 0;

    top = max - rows + 1;
    return (top > 0) ? top : 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_exec_shell (int pty_slave, const char *start_dir)
{
    const char *shell;
    char tty_name[MC_MAXPATHLEN];

    /* exec() preserves SIG_IGN; the child shell needs normal signal handling. */
    signal (SIGINT, SIG_DFL);
    signal (SIGQUIT, SIG_DFL);
    signal (SIGPIPE, SIG_DFL);

    /* Read while the slave fd is still open; it is closed further down. */
    if (ttyname_r (pty_slave, tty_name, sizeof (tty_name)) != 0)
        tty_name[0] = '\0';

    if (setsid () < 0)
        _exit (1);
    if (ioctl (pty_slave, TIOCSCTTY, 0) < 0)
        _exit (1);

    dup2 (pty_slave, STDIN_FILENO);
    dup2 (pty_slave, STDOUT_FILENO);
    dup2 (pty_slave, STDERR_FILENO);

    if (pty_slave > STDERR_FILENO)
        close (pty_slave);

    /* Close all fds inherited from MC so they do not leak into the shell. */
    {
        int maxfd = (int) sysconf (_SC_OPEN_MAX);
        int i;

        if (maxfd <= 0)
            maxfd = 1024;
        for (i = STDERR_FILENO + 1; i < maxfd; i++)
            close (i);
    }

    shell = (mc_global.shell != NULL) ? mc_global.shell->path : NULL;
    if (shell == NULL || *shell == '\0')
        shell = "/bin/sh";

    g_setenv ("TERM", "xterm-256color", TRUE);

    /* Tell an mc started from here how to reach us: it should ask for the panels
       instead of running a second copy inside our own terminal. */
    {
        char pid_str[32];

        g_snprintf (pid_str, sizeof (pid_str), "%ld", (long) getppid ());
        g_setenv ("MC_PID", pid_str, TRUE);
        if (tty_name[0] != '\0')
            g_setenv ("MC_TTY", tty_name, TRUE);
    }

    if (start_dir != NULL && chdir (start_dir) != 0)
    { /* fallback: shell starts in mc's cwd */
    }

    execl (shell, shell, NULL);
    _exit (127);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Copy the shell setup with every placeholder replaced by the session token.
 *
 * The setup is written as a plain shell literal, printf escapes and all, so the token cannot be
 * spliced in with g_strdup_printf() without doubling every percent sign in it.
 */

static char *
mcterm_setup_with_token (const char *setup, const char *token)
{
    static const size_t ph_len = sizeof (MCTERM_TOKEN_PLACEHOLDER) - 1;
    GString *out;
    const char *p = setup;

    out = g_string_sized_new (strlen (setup) + 64);

    while (TRUE)
    {
        const char *ph = strstr (p, MCTERM_TOKEN_PLACEHOLDER);

        if (ph == NULL)
        {
            g_string_append (out, p);
            break;
        }

        g_string_append_len (out, p, ph - p);
        g_string_append (out, token);
        p = ph + ph_len;
    }

    return g_string_free (out, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_write_silent (int master, const char *data, size_t len)
{
    struct termios tt;
    gboolean echo_was_on = FALSE;
    gboolean ok;

    if (tcgetattr (master, &tt) == 0 && (tt.c_lflag & ECHO) != 0)
    {
        struct termios tt_noecho = tt;

        tt_noecho.c_lflag &= ~(tcflag_t) ECHO;
        if (tcsetattr (master, TCSANOW, &tt_noecho) == 0)
            echo_was_on = TRUE;
    }

    ok = mcterm_write_all (master, (const unsigned char *) data, len);

    if (echo_was_on)
        (void) tcsetattr (master, TCSANOW, &tt);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_enable_osc7 (WMcTerm *t, int master, const char *setup_template)
{
    char *setup;
    gboolean written;

    t->osc7_capable = TRUE;
    t->osc133_capable = TRUE;
    t->shell_at_prompt = FALSE;
    t->sync_snapshot_buf = mcview_terminal_buffer_copy (mcview_vterm_buf (t->vterm));
    t->sync_snapshot_cursor_row = mcview_vterm_cursor_row (t->vterm);
    t->pending_internal_sync = TRUE;
    t->waiting_for_initial_osc7 = TRUE;

    setup = mcterm_setup_with_token (setup_template, t->osc7_token);
    written = mcterm_write_silent (master, setup, strlen (setup));
    g_free (setup);

    if (!written)
    {
        t->osc7_capable = FALSE;
        t->osc133_capable = FALSE;
        t->shell_at_prompt = TRUE;
        t->pending_internal_sync = FALSE;
        t->waiting_for_initial_osc7 = FALSE;
        mcview_terminal_buffer_free (t->sync_snapshot_buf);
        t->sync_snapshot_buf = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

/* One screen: live rows, history above them, @back rows away from the end. */
static mcview_terminal_buffer_t *
mcterm_compose_view (WMcTerm *t, int lines, int live_top, int live_rows, int back)
{
    mcview_terminal_buffer_t *view;
    const mcview_terminal_buffer_t *live;
    int hist_len;
    int start;
    int row;

    live = mcview_vterm_buf (t->vterm);
    hist_len = mcview_vterm_history_len (t->vterm);
    view = mcview_terminal_buffer_new ();

    // Where the top of the screen falls in history followed by live rows.
    start = hist_len + live_rows - lines - back;

    for (row = 0; row < lines; row++)
    {
        int v = start + row;
        GArray *cells = NULL;

        if (v < 0)
            continue;

        if (v < hist_len)
        {
            const GArray *h = mcview_vterm_history_row (t->vterm, v);

            if (h != NULL)
                mcview_terminal_buffer_set_row (view, row, h);
            continue;
        }

        cells = mcview_terminal_buffer_row_copy (live, live_top + v - hist_len);
        if (cells != NULL)
        {
            mcview_terminal_buffer_set_row (view, row, cells);
            g_array_unref (cells);
        }
    }

    return view;
}

/* --------------------------------------------------------------------------------------------- */

/* Put the output row @row on line @screen_row of @view. */
static void
mcterm_view_set_row (WMcTerm *t, mcview_terminal_buffer_t *view, int screen_row, gint64 row)
{
    const gint64 scrolled = mcview_vterm_scrolled_rows (t->vterm);

    if (row >= scrolled)
    {
        GArray *cells;

        cells =
            mcview_terminal_buffer_row_copy (mcview_vterm_buf (t->vterm), (int) (row - scrolled));
        if (cells != NULL)
        {
            mcview_terminal_buffer_set_row (view, screen_row, cells);
            g_array_unref (cells);
        }
    }
    else
    {
        // The history keeps its newest rows only, so the oldest ones are gone.
        const int len = mcview_vterm_history_len (t->vterm);
        const gint64 index = row - (scrolled - len);

        if (index >= 0 && index < len)
        {
            const GArray *cells = mcview_vterm_history_row (t->vterm, (int) index);

            if (cells != NULL)
                mcview_terminal_buffer_set_row (view, screen_row, cells);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* One screen of the rows that matched, the view standing where it was left. */
static mcview_terminal_buffer_t *
mcterm_compose_filtered (WMcTerm *t, const mcterm_geom_t *g)
{
    mcview_terminal_buffer_t *view;
    int row;

    view = mcview_terminal_buffer_new ();

    for (row = 0; row < g->content_rows; row++)
    {
        const gint64 abs_row = mcterm_filter_row (&t->filter, t->filter.top + row);

        if (abs_row < 0)
            break;

        mcterm_view_set_row (t, view, g->blank_above + row, abs_row);
    }

    return view;
}

/* --------------------------------------------------------------------------------------------- */

/* Move the view @delta rows, up when negative. TRUE if it moved. */
static gboolean
mcterm_scroll_view (WMcTerm *t, int delta)
{
    int back;
    int max;

    if (t->vterm == NULL || mcview_vterm_in_alt_screen (t->vterm) || !t->scroll_allowed)
        return FALSE;

    if (mcterm_filter_active (&t->filter))
    {
        const int lines = WIDGET (t)->rect.lines;
        const int len = mcterm_filter_len (&t->filter);
        int top;

        top = CLAMP (t->filter.top + delta, 0, MAX (len - lines, 0));
        if (top == t->filter.top)
            return FALSE;

        t->filter.top = top;
        widget_draw (WIDGET (t));

        return TRUE;
    }

    max = mcview_vterm_history_len (t->vterm);
    back = t->scrollback - delta;
    back = CLAMP (back, 0, max);

    if (back == t->scrollback)
        return FALSE;

    t->scrollback = back;
    widget_draw (WIDGET (t));

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The colors of the terminal's own skin section, not of the viewer's. */
static void
mcterm_canvas_colors (mcview_canvas_colors_t *colors)
{
    colors->section = "mcterm";
    colors->normal = MCTERM_NORMAL_COLOR;
    colors->bold = -1;
    colors->underline = -1;
    colors->bold_underline = -1;
}

/* --------------------------------------------------------------------------------------------- */

/* Typing returns to the end, the way a terminal does. */
static void
mcterm_follow_end (WMcTerm *t)
{
    const gboolean filtered = mcterm_filter_active (&t->filter);

    // The output goes on below the rows that matched: the filter is in the way of it.
    if (filtered)
        mcterm_filter_clear (&t->filter);

    if (t->scrollback != 0 || filtered)
    {
        t->scrollback = 0;
        widget_draw (WIDGET (t));
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Fit the rows through @cursor_row into the widget and return the last content row. */
static int
mcterm_fit_content (const WMcTerm *t, const mcview_terminal_buffer_t *buf, int cursor_row,
                    int lines, int *top_row, int *content_rows, int *blank_above)
{
    const int max_row = mcview_terminal_buffer_max_row (buf);
    int effective_max;

    /* The row the shell types on is the host's when it types elsewhere, drawn on its command line
       by mcterm_draw_prompt_row(); shown full screen the terminal draws that row itself. */
    if (t->shell_at_prompt && t->osc7_capable && t->typing_elsewhere
        && !mcview_vterm_in_alt_screen (t->vterm))
        effective_max = cursor_row - 1;
    else
        effective_max = (cursor_row > max_row) ? cursor_row : max_row;

    if (effective_max >= *top_row + lines)
        *top_row = effective_max - lines + 1;

    *content_rows = (effective_max >= *top_row) ? (effective_max - *top_row + 1) : 0;
    if (*content_rows > lines)
        *content_rows = lines;
    *blank_above = (!mcview_vterm_in_alt_screen (t->vterm) && *content_rows < lines)
        ? (lines - *content_rows)
        : 0;

    return effective_max;
}

/* --------------------------------------------------------------------------------------------- */

/* FALSE when there is no terminal to draw. */
static gboolean
mcterm_geometry (const WMcTerm *t, mcterm_geom_t *g)
{
    const WRect *r = &WIDGET (t)->rect;
    int cursor_row, effective_max;
    gboolean fill_top;

    if (t->vterm == NULL)
        return FALSE;

    // An alt-screen application draws a screen of its own, which has no rows to pick from.
    if (mcterm_filter_active (&t->filter) && !mcview_vterm_in_alt_screen (t->vterm))
    {
        const int len = mcterm_filter_len (&t->filter);

        g->snapshot = FALSE;
        g->compose = TRUE;
        g->filtered = TRUE;
        g->buf = mcview_vterm_buf (t->vterm);
        g->top_row = 0;
        g->content_rows = CLAMP (len - t->filter.top, 0, r->lines);
        g->blank_above = r->lines - g->content_rows;
        g->first_abs = mcterm_filter_row (&t->filter, t->filter.top);
        g->newest_abs = mcterm_filter_row (&t->filter, len - 1);

        return TRUE;
    }

    g->filtered = FALSE;
    g->snapshot = t->pending_internal_sync && t->sync_snapshot_buf != NULL;
    g->buf = g->snapshot ? t->sync_snapshot_buf : mcview_vterm_buf (t->vterm);
    g->top_row = g->snapshot ? mcterm_resolve_top_row_for_buf (t, g->buf, r->lines)
                             : mcview_vterm_resolve_top_row (t->vterm, r->lines);
    cursor_row = g->snapshot ? t->sync_snapshot_cursor_row : mcview_vterm_cursor_row (t->vterm);
    effective_max = mcterm_fit_content (t, g->buf, cursor_row, r->lines, &g->top_row,
                                        &g->content_rows, &g->blank_above);

    /* The prompt row is drawn on the command line, so a full screen is one row
       short at the top; that row is the newest one in the history. */
    fill_top = g->blank_above > 0 && cursor_row >= r->lines - 1;
    g->compose =
        !g->snapshot && !mcview_vterm_in_alt_screen (t->vterm) && (t->scrollback > 0 || fill_top);

    if (g->compose)
    {
        // Where the top of the screen falls in history followed by live rows.
        const int hist_len = mcview_vterm_history_len (t->vterm);
        const int start = hist_len + g->content_rows - r->lines - t->scrollback;

        g->first_abs = mcview_vterm_scrolled_rows (t->vterm) - hist_len + start;
        g->blank_above = 0;
    }
    else
        g->first_abs = mcview_vterm_scrolled_rows (t->vterm) + g->top_row;

    g->newest_abs = mcview_vterm_scrolled_rows (t->vterm) + effective_max;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* The row of the output drawn on screen line @y; -1 where none is. */
static gint64
mcterm_geom_row (const WMcTerm *t, const mcterm_geom_t *g, int y)
{
    if (y < g->blank_above)
        return -1;

    if (g->filtered)
        return mcterm_filter_row (&t->filter, t->filter.top + (y - g->blank_above));

    return g->first_abs + (y - g->blank_above);
}

/* --------------------------------------------------------------------------------------------- */

/* The screen line @row is drawn on. One off the screen is counted just the same,
   so that the caller can tell how far the view has to move. */
static gint64
mcterm_geom_screen_row (const WMcTerm *t, const mcterm_geom_t *g, gint64 row)
{
    if (g->filtered)
        return g->blank_above + (mcterm_filter_index (&t->filter, row) - t->filter.top);

    return g->blank_above + (row - g->first_abs);
}

/* --------------------------------------------------------------------------------------------- */

/* The number of the row under @y, and the column under @x. */
static gboolean
mcterm_row_at (const WMcTerm *t, int y, int x, gint64 *row, int *col)
{
    const WRect *r = &WIDGET (t)->rect;
    mcterm_geom_t g;

    if (!mcterm_geometry (t, &g))
        return FALSE;

    y = CLAMP (y, g.blank_above, r->lines - 1);
    *row = mcterm_geom_row (t, &g, y);
    *col = CLAMP (x, 0, r->cols - 1);

    if (g.filtered)
    {
        // Below the last row that matched: that row is what was clicked on.
        if (*row < 0)
            *row = mcterm_filter_row (&t->filter, mcterm_filter_len (&t->filter) - 1);

        return (*row >= 0);
    }

    /* The top of the screen can be filler standing above the oldest row there
       is; a click there belongs to the first row that exists. */
    {
        const gint64 oldest =
            mcview_vterm_scrolled_rows (t->vterm) - mcview_vterm_history_len (t->vterm);

        *row = CLAMP (*row, MIN (oldest, g.newest_abs), g.newest_abs);
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Bring @row onto the screen, scrolling the view as far as it takes. */
static void
mcterm_show_row (WMcTerm *t, gint64 row)
{
    const WRect *r = &WIDGET (t)->rect;
    const gint64 hist = mcview_vterm_history_len (t->vterm);
    mcterm_geom_t g;
    gint64 screen_row, delta;

    if (!mcterm_geometry (t, &g))
        return;

    screen_row = mcterm_geom_screen_row (t, &g, row);
    if (screen_row < 0)
        delta = screen_row;
    else if (screen_row >= r->lines)
        delta = screen_row - r->lines + 1;
    else
        return;

    // The view moves within the history and no further, so the step fits there.
    mcterm_scroll_view (t, (int) CLAMP (delta, -hist, hist));
}

/* --------------------------------------------------------------------------------------------- */

/* The last column of @row that carries something, -1 for a row that carries
   nothing at all. */
static int
mcterm_row_last_col (WMcTerm *t, gint64 row, int cols)
{
    int col;

    for (col = cols - 1; col >= 0; col--)
    {
        const mcview_vterm_cell_t *cell = mcterm_sel_cell_at (t->vterm, row, col);

        if (cell != NULL && cell->ch != 0 && cell->ch != ' ')
            return col;
    }

    return -1;
}

/* --------------------------------------------------------------------------------------------- */

/* The rows were laid out again for a new width: what pointed at them is
   pointed again, or dropped. */
static void
mcterm_after_reflow (WMcTerm *t)
{
    if (t->input_start_valid)
    {
        gint64 row;
        int col;

        if (mcview_vterm_reflow_map (t->vterm, t->input_start_row, t->input_start_col, &row, &col))
        {
            t->input_start_row = row;
            t->input_start_col = col;
        }
        else
            t->input_start_valid = FALSE;
    }
    mcterm_sel_clear (&t->sel);
    mcterm_filter_clear (&t->filter);
    t->cursor_valid = FALSE;
    t->scrollback = MIN (t->scrollback, mcview_vterm_history_len (t->vterm));
}

/* --------------------------------------------------------------------------------------------- */

/* The cursor starts where the shell is typing, or on the last row the
   terminal draws when the shell is typing on the command line. */
static void
mcterm_cursor_reset (WMcTerm *t)
{
    const gint64 shell_row =
        mcview_vterm_scrolled_rows (t->vterm) + mcview_vterm_cursor_row (t->vterm);
    mcterm_geom_t g;

    t->cursor_valid = TRUE;

    if (!mcterm_geometry (t, &g) || shell_row <= g.newest_abs)
    {
        t->cursor_row = shell_row;
        t->cursor_col = mcview_vterm_cursor_col (t->vterm);
    }
    else
    {
        t->cursor_row = g.newest_abs;
        t->cursor_col = 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Move the cursor over the output. With @marking the region grows behind it,
   the way a block grows from the cursor of the editor; without, it is dropped. */
static void
mcterm_cursor_move (WMcTerm *t, long command, gboolean marking)
{
    const WRect *r = &WIDGET (t)->rect;
    const int page = (r->lines > 1) ? r->lines - 1 : 1;
    const gint64 scrolled = mcview_vterm_scrolled_rows (t->vterm);
    mcterm_geom_t g;
    gint64 oldest;
    gint64 newest;
    gint64 row;
    int col;

    if (!mcterm_geometry (t, &g))
        return;

    if (!t->cursor_valid)
        mcterm_cursor_reset (t);

    if (g.filtered)
    {
        /* The rows in between are not drawn, so a step over them is a step from
           one row that matched to the next: @row counts those, not the output. */
        oldest = 0;
        newest = MAX (mcterm_filter_len (&t->filter) - 1, 0);
        row = mcterm_filter_index (&t->filter, t->cursor_row);
    }
    else
    {
        oldest = scrolled - mcview_vterm_history_len (t->vterm);
        // The cursor stays on what the terminal draws, and off the command line.
        newest = MAX (g.newest_abs, oldest);
        row = t->cursor_row;
    }

    col = t->cursor_col;

    switch (command)
    {
    case CK_Left:
    case CK_MarkLeft:
        if (col > 0)
            col--;
        else if (row > oldest)
        {
            row--;
            col = r->cols - 1;
        }
        break;
    case CK_Right:
    case CK_MarkRight:
        if (col < r->cols - 1)
            col++;
        else if (row < newest)
        {
            row++;
            col = 0;
        }
        break;
    case CK_Up:
    case CK_MarkUp:
        row--;
        break;
    case CK_Down:
    case CK_MarkDown:
        row++;
        break;
    case CK_MarkPageUp:
        row -= page;
        break;
    case CK_MarkPageDown:
        row += page;
        break;
    case CK_WordLeft:
        col -= MCTERM_JUMP_COLS;
        break;
    case CK_WordRight:
        col += MCTERM_JUMP_COLS;
        break;
    case CK_Home:
        col = 0;
        break;
    case CK_End:
        // Past the text, where the end of a line puts the cursor everywhere else.
        col = mcterm_row_last_col (t, t->cursor_row, r->cols);
        col = (col < 0) ? 0 : MIN (col + 1, r->cols - 1);
        break;
    case CK_MarkToHome:
        col = 0;
        break;
    case CK_MarkToEnd:
        // On the text, so that what is lit up is the text and nothing besides.
        col = MAX (mcterm_row_last_col (t, t->cursor_row, r->cols), 0);
        break;
    default:
        break;
    }

    row = CLAMP (row, oldest, newest);
    col = CLAMP (col, 0, r->cols - 1);

    if (g.filtered)
    {
        row = mcterm_filter_row (&t->filter, (int) row);
        if (row < 0)
            return;
    }

    if (marking)
    {
        if (!t->sel.anchored)
            mcterm_sel_start (&t->sel, t->cursor_row, t->cursor_col);
        mcterm_sel_extend (&t->sel, row, col);
    }
    else
        mcterm_sel_clear (&t->sel);

    t->cursor_row = row;
    t->cursor_col = col;
    mcterm_show_row (t, row);
    widget_draw (WIDGET (t));
    send_message (WIDGET (t), NULL, MSG_CURSOR, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* A page of newlines: the screen goes into the history, the prompt line stays.
   With @whole_buffer the history goes too, and nothing is left to scroll back to. */
static void
mcterm_clear_screen (WMcTerm *t, gboolean whole_buffer)
{
    const gint64 scrolled = mcview_vterm_scrolled_rows (t->vterm);
    const int cursor_row = mcview_vterm_cursor_row (t->vterm);
    const gint64 first_row = (t->shell_at_prompt && t->input_start_valid)
        ? MAX (t->input_start_row - scrolled, 0)
        : cursor_row;

    mcterm_follow_end (t);
    if (t->sel.anchored)
        mcterm_sel_clear (&t->sel);

    mcview_vterm_page_up (t->vterm, (int) (cursor_row - first_row) + 1);
    if (whole_buffer)
        mcview_vterm_clear_history (t->vterm);

    // The line moved down with its rows.
    t->input_start_row += mcview_vterm_scrolled_rows (t->vterm) - scrolled
        + (mcview_vterm_cursor_row (t->vterm) - cursor_row);
    t->cursor_valid = FALSE;
    widget_draw (WIDGET (t));
    send_message (WIDGET (t), NULL, MSG_CURSOR, 0, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* What to filter by: the marked text when it is of one row, and the word under
   the cursor when there is no such mark. NULL when neither says anything. */
static char *
mcterm_filter_pattern (WMcTerm *t)
{
    const WRect *r = &WIDGET (t)->rect;
    char *text = NULL;

    if (t->sel.active)
    {
        text = mcterm_sel_text (&t->sel, t->vterm, r->cols);
        /* A mark over several rows says nothing about the row to look for, and
           one over blanks says it of every row: the word under the cursor, then. */
        if (text != NULL && (strchr (text, '\n') != NULL || text[strspn (text, " \t")] == '\0'))
            g_clear_pointer (&text, g_free);
    }

    if (text == NULL)
    {
        mcterm_sel_t word;

        if (!t->cursor_valid)
            mcterm_cursor_reset (t);

        mcterm_sel_clear (&word);
        mcterm_sel_word (&word, t->vterm, t->cursor_row, t->cursor_col, r->cols);
        text = mcterm_sel_text (&word, t->vterm, r->cols);
    }

    // A blank is a word of its own, and one that every row of output carries.
    if (text != NULL && text[strspn (text, " \t")] == '\0')
        g_clear_pointer (&text, g_free);

    return text;
}

/* --------------------------------------------------------------------------------------------- */

/* Show the rows that match @pattern and no others. FALSE when none of them does,
   the view left as it was. */
static gboolean
mcterm_filter_set (WMcTerm *t, const char *pattern)
{
    const WRect *r = &WIDGET (t)->rect;
    const gint64 newest = mcview_vterm_scrolled_rows (t->vterm)
        + MAX (mcview_terminal_buffer_max_row (mcview_vterm_buf (t->vterm)),
               mcview_vterm_cursor_row (t->vterm));
    gint64 cursor_row;
    int index, len;

    if (!t->cursor_valid)
        mcterm_cursor_reset (t);
    cursor_row = t->cursor_row;

    if (!mcterm_filter_apply (&t->filter, t->vterm, r->cols, newest, pattern))
        return FALSE;

    // The toggle filters by what is kept here; anything else is kept in its turn.
    if (t->last_filter != pattern)
    {
        g_free (t->last_filter);
        t->last_filter = g_strdup (pattern);
    }

    // What was marked is not what is drawn any more.
    mcterm_sel_clear (&t->sel);
    t->scrollback = 0;

    /* The cursor stays where it was reading: on the row it was on, or on the
       first row that matched below it. */
    len = mcterm_filter_len (&t->filter);
    index = mcterm_filter_index (&t->filter, cursor_row);
    t->filter.top = CLAMP (index - r->lines / 2, 0, MAX (len - r->lines, 0));
    t->cursor_row = mcterm_filter_row (&t->filter, index);
    // The column it was reading at is the column it goes on reading at.
    t->cursor_col = CLAMP (t->cursor_col, 0, r->cols - 1);
    t->cursor_valid = TRUE;

    widget_draw (WIDGET (t));
    send_message (WIDGET (t), NULL, MSG_CURSOR, 0, NULL);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Filter by the marked text, or by the word under the cursor. FALSE when there was nothing to
   filter by: the key is the shell's. */
static gboolean
mcterm_filter_by_word (WMcTerm *t)
{
    char *pattern;
    gboolean ok;

    pattern = mcterm_filter_pattern (t);
    if (pattern == NULL)
        return FALSE;

    ok = mcterm_filter_set (t, pattern);
    g_free (pattern);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/* Lift the filter if one is on; otherwise put the last one back on, over the
   output as it stands now. FALSE when there has been none: the key is the shell's. */
static gboolean
mcterm_filter_toggle (WMcTerm *t)
{
    if (mcterm_filter_active (&t->filter))
    {
        mcterm_filter_clear (&t->filter);
        widget_draw (WIDGET (t));
        send_message (WIDGET (t), NULL, MSG_CURSOR, 0, NULL);

        return TRUE;
    }

    return (t->last_filter != NULL && mcterm_filter_set (t, t->last_filter));
}

/* --------------------------------------------------------------------------------------------- */

/* The view has moved: bring the cursor along, or it would be left off the
   screen, where it cannot be drawn and the command line takes it over. */
static void
mcterm_cursor_into_view (WMcTerm *t)
{
    const WRect *r = &WIDGET (t)->rect;
    const gint64 oldest =
        mcview_vterm_scrolled_rows (t->vterm) - mcview_vterm_history_len (t->vterm);
    mcterm_geom_t g;
    gint64 first, last;

    if (!t->cursor_valid || !mcterm_geometry (t, &g))
        return;

    if (g.filtered)
    {
        const int last_shown = t->filter.top + MAX (g.content_rows - 1, 0);
        int index;

        index = CLAMP (mcterm_filter_index (&t->filter, t->cursor_row), t->filter.top, last_shown);
        index = MIN (index, mcterm_filter_len (&t->filter) - 1);
        if (index >= 0)
            t->cursor_row = mcterm_filter_row (&t->filter, index);

        return;
    }

    first = MAX (g.first_abs, oldest);
    last = g.first_abs + (g.compose ? r->lines : g.content_rows) - 1;
    last = MIN (last, g.newest_abs);

    if (last >= first)
        t->cursor_row = CLAMP (t->cursor_row, first, last);
}

/* --------------------------------------------------------------------------------------------- */

/* Over what was drawn: the marked cells again, in the colour of a selection. */
static void
mcterm_draw_selection (WMcTerm *t, const mcterm_geom_t *g)
{
    const WRect *r = &WIDGET (t)->rect;
    int row;

    if (!t->sel.active || g->snapshot)
        return;

    tty_setcolor (MCTERM_SELECTED_COLOR);

    for (row = g->blank_above; row < r->lines; row++)
    {
        const gint64 abs_row = mcterm_geom_row (t, g, row);
        int from, to, col;

        if (abs_row < 0 || !mcterm_sel_row_span (&t->sel, abs_row, r->cols, &from, &to))
            continue;

        for (col = from; col < to; col++)
        {
            const mcview_vterm_cell_t *cell = mcterm_sel_cell_at (t->vterm, abs_row, col);

            tty_gotoyx (r->y + row, r->x + col);
            tty_print_anychar ((cell == NULL || cell->ch == 0) ? ' ' : cell->ch);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

/* After the refresh: the pictures, straight to the terminal, over the cells
   that were just written. Cell coordinates come from the same geometry the
   cells were drawn with. */
static void
mcterm_paint_pictures (void *data)
{
    WMcTerm *t = data;
    const WRect *r = &WIDGET (t)->rect;
    mcterm_geom_t g;
    guint i, shown = 0;

    /* Hidden with pictures on the screen: whatever is drawn over the widget
       leaves the pixels where no cell changed. Every cell again, then. */
    if (t->pictures_shown && !widget_get_state (WIDGET (t), WST_VISIBLE))
    {
        t->pictures_shown = FALSE;
        t->paint_pending = FALSE;
        tty_touch_screen ();
        tty_refresh ();
        return;
    }

    if (!t->paint_pending)
        return;
    t->paint_pending = FALSE;
    t->pictures_shown = FALSE;

    if (!widget_get_state (WIDGET (t), WST_VISIBLE) || t->child_dead || !tty_has_sixel ()
        || !mcterm_geometry (t, &g) || g.snapshot || g.filtered)
        return;

    for (i = 0; i < mcview_vterm_images_len (t->vterm); i++)
    {
        const mcview_vterm_image_t *image = mcview_vterm_image (t->vterm, i);
        const gint64 abs_row = mcview_vterm_scrolled_rows (t->vterm) + image->row;
        const gint64 y = r->y + g.blank_above + (abs_row - g.first_abs);
        const int x = r->x + image->col;
        char move[32];

        /* A picture cannot be cut: one that does not fit is not painted. */
        if (image->data == NULL || y < r->y + g.blank_above || y + image->rows > r->y + r->lines
            || x + image->cols > r->x + r->cols)
            continue;

        g_snprintf (move, sizeof (move), ESC_STR "7" ESC_STR "[%d;%dH", (int) y + 1, x + 1);
        tty_raw_write (move, strlen (move));
        tty_raw_write (g_bytes_get_data (image->data, NULL), g_bytes_get_size (image->data));
        tty_raw_write (ESC_STR "8", 2);
        shown++;
    }

    t->pictures_shown = shown > 0;
}

/* --------------------------------------------------------------------------------------------- */

/* The size of the terminal for the program: rows and columns, and the pixels
   they make when the terminal draws pictures, for a program to size one. */
static void
mcterm_winsize (const WRect *r, struct winsize *ws)
{
    int cell_width = 0, cell_height = 0;

    memset (ws, 0, sizeof (*ws));
    ws->ws_row = (unsigned short) r->lines;
    ws->ws_col = (unsigned short) r->cols;
    if (tty_has_sixel ())
        tty_cell_size (&cell_width, &cell_height);
    ws->ws_xpixel = (unsigned short) (r->cols * cell_width);
    ws->ws_ypixel = (unsigned short) (r->lines * cell_height);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_do_draw (WMcTerm *t)
{
    const WRect *r = &WIDGET (t)->rect;

    (void) mcterm_handle_stalled_internal_sync (t);

    if (t->pictures_shown || mcview_vterm_images_len (t->vterm) > 0)
    {
        tty_touch_area (r->y, r->x, r->lines, r->cols);
        t->paint_pending = TRUE;
    }

    if (t->child_dead)
    {
        int row;

        tty_setcolor (MCTERM_NORMAL_COLOR);
        for (row = 0; row < r->lines; row++)
        {
            int col;

            tty_gotoyx (r->y + row, r->x);
            for (col = 0; col < r->cols; col++)
                tty_print_char (' ');
        }
        tty_gotoyx (r->y, r->x);
        tty_print_string ("[ Process exited ]");
        return;
    }

    {
        mcterm_geom_t g;
        mcview_canvas_colors_t colors;

        if (!mcterm_geometry (t, &g))
            return;

        mcterm_canvas_colors (&colors);

        if (g.compose)
        {
            mcview_terminal_buffer_t *view;

            view = g.filtered
                ? mcterm_compose_filtered (t, &g)
                : mcterm_compose_view (t, r->lines, g.top_row, g.content_rows, t->scrollback);
            mcview_render_terminal_canvas (view, 0, r->y, r->x, r->lines, r->cols, &colors);
            mcview_terminal_buffer_free (view);
        }
        else
        {
            if (g.blank_above > 0)
            {
                int row, col;

                tty_setcolor (MCTERM_NORMAL_COLOR);
                for (row = 0; row < g.blank_above; row++)
                {
                    tty_gotoyx (r->y + row, r->x);
                    for (col = 0; col < r->cols; col++)
                        tty_print_char (' ');
                }
            }

            if (g.content_rows > 0)
                mcview_render_terminal_canvas (g.buf, g.top_row, r->y + g.blank_above, r->x,
                                               g.content_rows, r->cols, &colors);
        }

        mcterm_draw_selection (t, &g);
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_write_all (int master, const unsigned char *data, size_t len)
{
    const unsigned char *p = data;
    size_t remaining = len;

    while (remaining > 0)
    {
        ssize_t nw = write (master, p, remaining);
        if (nw < 0)
        {
            if (errno == EINTR)
                continue;
            return FALSE;
        }
        if (nw == 0)
            return FALSE;
        p += nw;
        remaining -= (size_t) nw;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_waitpid_reap (pid_t pid)
{
    while (waitpid (pid, NULL, 0) < 0 && errno == EINTR)
        ;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcterm_send_encoded_key (WMcTerm *t, int key)
{
    unsigned char buf[64];
    gboolean app_cursor = mcview_vterm_app_cursor_keys (t->vterm);
    size_t n = mcterm_encode_key_xterm (key, buf, sizeof (buf), app_cursor);

    if (n > 0)
        return mcterm_write_all (t->pty_master, buf, n);
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

/* The whole output becomes the mark, and a second press takes it back. Under a filter the mark
   runs from the first row shown to the last, so what lies hidden in between is marked too: a
   mark is one unbroken run of rows and knows no holes. */
static gboolean
mcterm_mark_all (WMcTerm *t)
{
    const WRect *r = &WIDGET (t)->rect;
    mcterm_geom_t g;
    gint64 oldest, newest;

    if (t->sel.anchored)
    {
        mcterm_sel_clear (&t->sel);
        widget_draw (WIDGET (t));
        return TRUE;
    }

    if (r->cols <= 0 || !mcterm_geometry (t, &g))
        return FALSE;

    if (g.filtered)
    {
        const int len = mcterm_filter_len (&t->filter);

        if (len <= 0)
            return FALSE;

        oldest = mcterm_filter_row (&t->filter, 0);
        newest = mcterm_filter_row (&t->filter, len - 1);
    }
    else
    {
        oldest = mcview_vterm_scrolled_rows (t->vterm) - mcview_vterm_history_len (t->vterm);
        newest = MAX (g.newest_abs, oldest);
    }

    if (oldest < 0 || newest < oldest)
        return FALSE;

    mcterm_sel_start (&t->sel, oldest, 0);
    mcterm_sel_extend (&t->sel, newest, r->cols - 1);

    // Reading goes on from the end of what was marked.
    t->cursor_row = newest;
    t->cursor_col = r->cols - 1;
    t->cursor_valid = TRUE;
    mcterm_show_row (t, newest);
    widget_draw (WIDGET (t));
    send_message (WIDGET (t), NULL, MSG_CURSOR, 0, NULL);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Carry out a command of the terminal's own, whether it came from a key of its keymap or from
   the button bar, which has no key to name and passes 0. MSG_NOT_HANDLED for one it has no use
   for: the key that stands for it belongs to the shell and is typed into it. */
static cb_ret_t
mcterm_execute_cmd (WMcTerm *t, long command, int key)
{
    const int page = WIDGET (t)->rect.lines - 1;

    switch (command)
    {
    case CK_Store:
        // A panel over it, or nothing marked: the key is someone else's.
        if (!t->scroll_allowed || !t->sel.active)
        {
            /* Enter is the shell's line and follows the output down to where that line is
               typed; the other keys of this command leave the view where it is. */
            if (key == '\n' || key == KEY_ENTER)
                mcterm_follow_end (t);
            break;
        }
        // Copied and done with: what is on the clipfile needs no marker.
        mcterm_sel_copy (&t->sel, t->vterm, WIDGET (t)->rect.cols);
        mcterm_sel_clear (&t->sel);
        widget_draw (WIDGET (t));
        return MSG_HANDLED;

    case CK_Unmark:
        if (!t->sel.anchored)
        {
            mcterm_follow_end (t);
            break;
        }
        mcterm_sel_clear (&t->sel);
        widget_draw (WIDGET (t));
        return MSG_HANDLED;

    case CK_MarkAll:
        // A panel over it: the key is someone else's.
        if (!t->scroll_allowed)
        {
            mcterm_follow_end (t);
            break;
        }
        if (!mcterm_mark_all (t))
            break;
        return MSG_HANDLED;

    case CK_MarkLeft:
    case CK_MarkRight:
    case CK_MarkUp:
    case CK_MarkDown:
    case CK_MarkPageUp:
    case CK_MarkPageDown:
    case CK_MarkToHome:
    case CK_MarkToEnd:
        // A panel over the terminal owns these keys, see CK_Store above.
        if (!t->scroll_allowed)
        {
            mcterm_follow_end (t);
            break;
        }
        mcterm_cursor_move (t, command, TRUE);
        return MSG_HANDLED;

    case CK_Left:
    case CK_Right:
    case CK_Up:
    case CK_Down:
    case CK_WordLeft:
    case CK_WordRight:
    case CK_Home:
    case CK_End:
        if (!t->scroll_allowed)
        {
            mcterm_follow_end (t);
            break;
        }
        mcterm_cursor_move (t, command, FALSE);
        return MSG_HANDLED;

    case CK_Clear:
    case CK_ClearAll:
        // A panel over the terminal: the key is someone else's.
        if (!t->scroll_allowed)
            break;
        mcterm_clear_screen (t, command == CK_ClearAll);
        return MSG_HANDLED;

    case CK_FilterWord:
        if (!t->scroll_allowed)
        {
            mcterm_follow_end (t);
            break;
        }
        if (!mcterm_filter_by_word (t))
            break;
        return MSG_HANDLED;

    case CK_FilterToggle:
        // Without a filter of its own the terminal has no use for the key.
        if (!t->scroll_allowed || !mcterm_filter_toggle (t))
            break;
        return MSG_HANDLED;

    case CK_ScrollUp:
    case CK_ScrollDown:
    case CK_PageUp:
    case CK_PageDown:
    case CK_Top:
    case CK_Bottom:
    {
        const int hist = mcterm_filter_active (&t->filter) ? mcterm_filter_len (&t->filter)
                                                           : mcview_vterm_history_len (t->vterm);
        int delta;

        if (command == CK_ScrollUp)
            delta = -1;
        else if (command == CK_ScrollDown)
            delta = 1;
        else if (command == CK_PageUp)
            delta = -page;
        else if (command == CK_PageDown)
            delta = page;
        else if (command == CK_Top)
            delta = -hist;
        else
            delta = hist;

        if (mcterm_scroll_view (t, delta))
        {
            mcterm_cursor_into_view (t);
            send_message (WIDGET (t), NULL, MSG_CURSOR, 0, NULL);
            return MSG_HANDLED;
        }

        // Held back, the key is ours; at the end, only these two are.
        if (t->scrollback > 0 || mcterm_filter_active (&t->filter) || command == CK_ScrollUp
            || command == CK_ScrollDown)
            return MSG_HANDLED;
        break;
    }

    default:
        mcterm_follow_end (t);
        if (t->sel.anchored)
        {
            mcterm_sel_clear (&t->sel);
            widget_draw (WIDGET (t));
        }
        break;
    }

    return MSG_NOT_HANDLED;
}

static cb_ret_t
mcterm_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WMcTerm *t = (WMcTerm *) w;

    (void) sender;
    (void) parm;

    switch (msg)
    {
    case MSG_DRAW:
        mcterm_do_draw (t);
        return MSG_HANDLED;

    case MSG_CURSOR:
    {
        /* Shown full screen with the shell typing at the prompt, the cursor
           follows the shell, not the reading cursor left at the focus point. */
        const gboolean live_line = !t->typing_elsewhere && t->shell_at_prompt && t->scrollback == 0
            && !t->sel.anchored && !mcterm_filter_active (&t->filter);

        /* Focused, the terminal shows where it is being read; the shell has
           the cursor back as soon as the focus goes to the command line. */
        if (!live_line && widget_get_state (w, WST_FOCUSED) && t->cursor_valid && t->vterm != NULL
            && !mcview_vterm_in_alt_screen (t->vterm))
        {
            mcterm_geom_t g;

            if (mcterm_geometry (t, &g))
            {
                const WRect *r = &w->rect;
                const gint64 screen_row = mcterm_geom_screen_row (t, &g, t->cursor_row);

                if (screen_row >= 0 && screen_row < r->lines)
                {
                    tty_gotoyx (r->y + (int) screen_row,
                                r->x + CLAMP (t->cursor_col, 0, r->cols - 1));
                    return MSG_HANDLED;
                }
            }
        }
        if (t->shell_at_prompt && t->osc7_capable && t->typing_elsewhere)
            return MSG_NOT_HANDLED;
        if (t->vterm != NULL && !t->child_dead)
        {
            const WRect *r = &w->rect;
            mcview_terminal_buffer_t *buf = mcview_vterm_buf (t->vterm);
            int top_row = mcview_vterm_resolve_top_row (t->vterm, r->lines);
            int cursor_row = mcview_vterm_cursor_row (t->vterm);
            int content_rows;
            int blank_above;

            /* Preserve the live-buffer cursor during an internal sync; mcterm_geometry() may be
               fitting the snapshot then, but the fallback has always followed the live vterm. */
            (void) mcterm_fit_content (t, buf, cursor_row, r->lines, &top_row, &content_rows,
                                       &blank_above);

            int crow = CLAMP (cursor_row - top_row + blank_above, 0, r->lines - 1);
            int ccol = CLAMP (mcview_vterm_cursor_col (t->vterm), 0, r->cols - 1);

            tty_gotoyx (r->y + crow, r->x + ccol);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;
    }

    case MSG_RESIZE:
    {
        const int old_cols = w->rect.cols;

        widget_default_callback (w, NULL, MSG_RESIZE, 0, data);
        mcview_vterm_set_size (t->vterm, w->rect.lines, w->rect.cols);
        if (w->rect.cols != old_cols)
            mcterm_after_reflow (t);
        if (!t->child_dead && t->pty_master >= 0)
        {
            struct winsize ws;

            mcterm_winsize (&w->rect, &ws);
            ioctl (t->pty_master, TIOCSWINSZ, &ws);
        }
        return MSG_HANDLED;
    }

    case MSG_HOTKEY:
        if (!widget_get_state (w, WST_FOCUSED))
            return MSG_NOT_HANDLED;
        if (t->child_dead || t->pty_master < 0)
            return MSG_NOT_HANDLED;
        if ((parm == 0x0F || parm == XCTRL ('O'))
            && (t->vterm == NULL || !mcview_vterm_in_alt_screen (t->vterm)))
            return MSG_NOT_HANDLED;
        return mcterm_send_encoded_key (t, parm) ? MSG_HANDLED : MSG_NOT_HANDLED;

    case MSG_KEY:
        /* Alt-screen applications own Ctrl+O. */
        if ((parm == 0x0F || parm == XCTRL ('O'))
            && (t->vterm == NULL || !mcview_vterm_in_alt_screen (t->vterm)))
            return MSG_NOT_HANDLED;

        // An alt-screen application gets every key itself.
        if (t->vterm != NULL && !mcview_vterm_in_alt_screen (t->vterm)
            && mcterm_execute_cmd (t, mcterm_key_command (t, parm), parm) == MSG_HANDLED)
            return MSG_HANDLED;

        if (t->child_dead || t->pty_master < 0)
            return MSG_NOT_HANDLED;
        return mcterm_send_encoded_key (t, parm) ? MSG_HANDLED : MSG_NOT_HANDLED;

    case MSG_ACTION:
        // The button bar names the terminal's commands while no panel is on screen.
        if (t->vterm == NULL || mcview_vterm_in_alt_screen (t->vterm))
            return MSG_NOT_HANDLED;
        return mcterm_execute_cmd (t, parm, 0);

    case MSG_FOCUS:
        // Reading starts where the shell is typing.
        if (t->vterm != NULL)
            mcterm_cursor_reset (t);
        widget_draw (w);
        return MSG_HANDLED;

    case MSG_UNFOCUS:
        // The mark is worked on with the keys of the terminal, which are gone now.
        t->cursor_valid = FALSE;
        if (t->sel.anchored)
        {
            mcterm_sel_clear (&t->sel);
            widget_draw (w);
        }
        return MSG_HANDLED;

    case MSG_DESTROY:
        if (t->pty_master >= 0)
        {
            delete_select_channel (t->pty_master);
            close (t->pty_master);
            t->pty_master = -1;
        }
        if (t->child_pid > 0)
        {
            kill (t->child_pid, SIGTERM);
            {
                struct timespec ts = { 0, 10 * 1000 * 1000 }; /* 10ms */
                int i;

                for (i = 0; i < 5; i++)
                {
                    pid_t ret = waitpid (t->child_pid, NULL, WNOHANG);

                    if (ret > 0 || (ret < 0 && errno == ECHILD))
                    {
                        t->child_pid = -1;
                        break;
                    }
                    nanosleep (&ts, NULL);
                }
            }
            if (t->child_pid > 0)
            {
                kill (t->child_pid, SIGKILL);
                mcterm_waitpid_reap (t->child_pid);
                t->child_pid = -1;
            }
        }
        mcterm_busy_tick_set (t, FALSE);
        tty_painter_remove (mcterm_paint_pictures, t);
        if (t->pictures_shown)
            tty_touch_screen (); /* the pixels go with the next refresh */
        mcview_vterm_free (t->vterm);
        t->vterm = NULL;
        t->last_osc7_gen = 0;
        g_free (t->osc7_token);
        t->osc7_token = NULL;
        g_clear_pointer (&t->command_hint, g_free);
        mcview_terminal_buffer_free (t->sync_snapshot_buf);
        t->sync_snapshot_buf = NULL;
        mcterm_filter_clear (&t->filter);
        g_clear_pointer (&t->last_filter, g_free);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/*** public functions ****************************************************************************/

WMcTerm *
mcterm_new (const WRect *r, const char *start_dir)
{
    WMcTerm *t;
    Widget *w;
    int master = -1, slave = -1;
    pid_t pid;

    mcterm_key_table_init (mc_global.profile_name, mc_global.main_config);

    {
        struct winsize ws;

        mcterm_winsize (r, &ws);
        if (openpty (&master, &slave, NULL, NULL, &ws) < 0)
            return NULL;
    }

    pid = fork ();
    if (pid < 0)
    {
        close (master);
        close (slave);
        return NULL;
    }

    if (pid == 0)
    {
        close (master);
        mcterm_exec_shell (slave, start_dir);
        /* not reached */
    }

    close (slave);
    slave = -1;

    t = g_new0 (WMcTerm, 1);
    w = WIDGET (t);

    widget_init (w, r, mcterm_callback, mcterm_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_WANT_CURSOR | WOP_WANT_HOTKEY;

    t->pty_master = master;
    t->child_pid = pid;
    t->child_dead = FALSE;
    t->shell_at_prompt = TRUE;
    t->last_osc7_gen = 0;
    t->osc7_capable = FALSE;
    t->osc133_capable = FALSE;
    t->last_osc133_gen = 0;
    t->last_exit_code = -1;
    t->awaiting_command_done = FALSE;
    t->busy_tick_fd = -1;
    t->busy_phase = 0;
    t->vterm = mcview_vterm_new ();
    t->scroll_allowed = TRUE;
    t->typing_elsewhere = TRUE;
    mcview_vterm_set_keep_history (t->vterm, TRUE);
    mcview_vterm_set_autowrap (t->vterm, TRUE);
    mcview_vterm_set_size (t->vterm, r->lines, r->cols);
    {
        int cell_width, cell_height;

        tty_cell_size (&cell_width, &cell_height);
        mcview_vterm_set_cell_size (t->vterm, cell_width, cell_height);
        mcview_vterm_set_sixel (t->vterm, tty_has_sixel ());
    }
    tty_painter_add (mcterm_paint_pictures, t);

    {
        struct winsize ws;

        mcterm_winsize (r, &ws);
        ioctl (master, TIOCSWINSZ, &ws);
    }

    add_select_channel (master, mcterm_pty_ready_cb, t);

    {
        const shell_type_t shell_type =
            (mc_global.shell != NULL) ? mc_global.shell->type : SHELL_NONE;

        t->osc7_token = g_strdup_printf ("%08x%08x", g_random_int (), g_random_int ());

        {
            const char *setup = mcterm_shell_setup (shell_type);

            if (setup != NULL)
                mcterm_enable_osc7 (t, master, setup);
        }
    }

    return t;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_wait_for_prompt (WMcTerm *t, int timeout_msec)
{
    const gint64 deadline = g_get_monotonic_time () + (gint64) timeout_msec * 1000;

    while (t != NULL && !t->child_dead && t->pty_master >= 0 && !t->shell_at_prompt)
    {
        fd_set read_set;
        struct timeval timeout;
        const gint64 remaining = deadline - g_get_monotonic_time ();
        int rc;

        if (remaining <= 0)
            break;

        FD_ZERO (&read_set);
        FD_SET (t->pty_master, &read_set);
        timeout.tv_sec = (time_t) (remaining / G_USEC_PER_SEC);
        timeout.tv_usec = (suseconds_t) (remaining % G_USEC_PER_SEC);
        rc = select (t->pty_master + 1, &read_set, NULL, NULL, &timeout);
        if (rc < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        if (rc == 0)
            break;

        if (FD_ISSET (t->pty_master, &read_set))
            mcterm_pty_ready_cb (t->pty_master, t);
    }

    return (t != NULL && !t->child_dead && t->shell_at_prompt && t->osc7_capable);
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_free (WMcTerm *t)
{
    if (t == NULL)
        return;
    send_message (WIDGET (t), NULL, MSG_DESTROY, 0, NULL);
    g_free (t);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_is_alive (const WMcTerm *t)
{
    return (t != NULL && !t->child_dead);
}

/* --------------------------------------------------------------------------------------------- */

static void
mcterm_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    WMcTerm *t = (WMcTerm *) w;
    gint64 row;
    int col;

    switch (msg)
    {
    case MSG_MOUSE_SCROLL_UP:
        mcterm_scroll_view (t, -MCTERM_WHEEL_ROWS);
        break;
    case MSG_MOUSE_SCROLL_DOWN:
        mcterm_scroll_view (t, MCTERM_WHEEL_ROWS);
        break;

    case MSG_MOUSE_DOWN:
        /* An alt-screen application draws its own thing, and a panel on the
           screen hides what would be marked; neither is marked in. */
        if ((event->buttons & GPM_B_LEFT) == 0 || mcterm_in_alt_screen (t) || !t->scroll_allowed)
            break;
        if (!mcterm_row_at (t, event->y, event->x, &row, &col))
            break;
        widget_select (w);
        mcterm_sel_clear (&t->sel);
        if ((event->count & GPM_TRIPLE) != 0)
            mcterm_sel_line (&t->sel, t->vterm, row, WIDGET (t)->rect.cols);
        else if ((event->count & GPM_DOUBLE) != 0)
            mcterm_sel_word (&t->sel, t->vterm, row, col, WIDGET (t)->rect.cols);
        else
            mcterm_sel_start (&t->sel, row, col);
        t->cursor_row = row;
        t->cursor_col = t->sel.active ? t->sel.point_col : col;
        t->cursor_valid = TRUE;
        widget_draw (w);
        send_message (w, NULL, MSG_CURSOR, 0, NULL);
        break;

    case MSG_MOUSE_DRAG:
        if (!t->sel.anchored)
            break;
        // A drag runs on outside the widget, and the view follows it.
        if (event->y < 0)
            mcterm_scroll_view (t, -1);
        else if (event->y >= WIDGET (t)->rect.lines)
            mcterm_scroll_view (t, 1);
        if (!mcterm_row_at (t, event->y, event->x, &row, &col))
            break;
        mcterm_sel_extend (&t->sel, row, col);
        t->cursor_row = row;
        t->cursor_col = col;
        widget_draw (w);
        send_message (w, NULL, MSG_CURSOR, 0, NULL);
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_set_typing_elsewhere (WMcTerm *t, gboolean elsewhere)
{
    if (t != NULL)
        t->typing_elsewhere = elsewhere;
}

/* --------------------------------------------------------------------------------------------- */

long
mcterm_key_command (const WMcTerm *t, int key)
{
    long command;

    if (t == NULL || t->vterm == NULL || mcterm_map == NULL)
        return CK_IgnoreKey;

    // An alt-screen application is typed at, not scrolled or marked.
    if (mcview_vterm_in_alt_screen (t->vterm))
        return CK_IgnoreKey;

    command = keybind_lookup_keymap_command (mcterm_map, key);

    switch (command)
    {
    case CK_ScrollUp:
    case CK_ScrollDown:
    case CK_PageUp:
    case CK_PageDown:
    case CK_Top:
    case CK_Bottom:
    case CK_Clear:
    case CK_ClearAll:
        // Looking back at the output does not need the focus, typing goes on.
        return command;

    case CK_Left:
    case CK_Right:
    case CK_Up:
    case CK_Down:
    case CK_Home:
    case CK_End:
        /* When the command line owns text input, keep its basic editing and
         * history keys there. Otherwise the shell reads them itself. */
        return t->typing_elsewhere && !widget_get_state (CONST_WIDGET (t), WST_FOCUSED)
            ? CK_IgnoreKey
            : command;

    case CK_WordLeft:
    case CK_WordRight:
        /* Ctrl-Left and Ctrl-Right navigate the terminal output even when
         * the command line owns text input. */
        return command;

    case CK_Store:
    case CK_MarkAll:
    case CK_FilterWord:
    case CK_FilterToggle:
        /* Marking the output, cutting it down and taking it out are the terminal's whoever is
         * typing, as long as it has the screen to itself. Enter is the exception: it is the
         * shell's own key and stays with whoever holds the focus. */
        if (t->scroll_allowed && key != '\n' && key != KEY_ENTER)
            return command;
        return widget_get_state (CONST_WIDGET (t), WST_FOCUSED) ? command : CK_IgnoreKey;

    default:
        // The cursor and the mark are its own only while it holds the focus.
        return widget_get_state (CONST_WIDGET (t), WST_FOCUSED) ? command : CK_IgnoreKey;
    }
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_set_scroll_allowed (WMcTerm *t, gboolean allowed)
{
    if (t == NULL)
        return;

    t->scroll_allowed = allowed;

    /* A panel has come over the terminal: what is marked below it can neither
       be seen nor added to, so the mark goes. */
    if (!allowed && t->sel.anchored)
    {
        mcterm_sel_clear (&t->sel);
        widget_draw (WIDGET (t));
    }
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

void
mcterm_scroll_to_end (WMcTerm *t)
{
    if (t != NULL)
        mcterm_follow_end (t);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_in_alt_screen (const WMcTerm *t)
{
    return (t != NULL && t->vterm != NULL && mcview_vterm_in_alt_screen (t->vterm));
}

/* --------------------------------------------------------------------------------------------- */

Widget *
mcterm_widget (WMcTerm *t)
{
    return WIDGET (t);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_send_line (WMcTerm *t, const char *line)
{

    if (t == NULL || t->child_dead || t->pty_master < 0)
        return FALSE;

    if (line != NULL && *line != '\0')
    {
        if (!mcterm_write_silent (t->pty_master, line, strlen (line)))
            return FALSE;
    }
    if (!mcterm_write_silent (t->pty_master, "\r", 1))
        return FALSE;
    if (t->osc7_capable)
    {
        t->shell_at_prompt = FALSE;
        t->last_osc7_gen = mcview_vterm_osc7_generation (t->vterm);
        // The shell tells us when it is over; a prompt drawn before then is a redrawn one.
        t->awaiting_command_done = t->osc133_capable;
        mcterm_busy_tick_set (t, TRUE);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_clear_shell_line (WMcTerm *t)
{
    if (t == NULL || t->child_dead || t->pty_master < 0)
        return;

    /* Ctrl-U is what the tty itself kills the line with. A line editor kills to the start with
       it, so the cursor goes to the end first. */
    if (mc_global.shell != NULL
        && (mc_global.shell->type == SHELL_BASH || mc_global.shell->type == SHELL_ZSH
            || mc_global.shell->type == SHELL_FISH || mc_global.shell->type == SHELL_TCSH))
        mcterm_send_key (t, XCTRL ('E'));
    mcterm_send_key (t, XCTRL ('U'));
    t->line_cleared = TRUE;
    t->line_typed = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_send_internal_line (WMcTerm *t, const char *line)
{
    char *hidden;
    gboolean ok;

    if (t == NULL || line == NULL)
        return FALSE;

    mcview_terminal_buffer_free (t->sync_snapshot_buf);

    t->sync_snapshot_buf = mcview_terminal_buffer_copy (mcview_vterm_buf (t->vterm));
    t->sync_snapshot_cursor_row = mcview_vterm_cursor_row (t->vterm);
    t->pending_internal_sync = TRUE;
    t->waiting_for_initial_osc7 = FALSE;
    t->internal_sync_deadline = g_get_monotonic_time () + MCTERM_INTERNAL_SYNC_TIMEOUT_USEC;

    /* This command is ours, not the user's: keep it out of the shell's history. bash can drop the
       entry outright; the others rely on the leading space the setup taught them to ignore. */
    if (mc_global.shell != NULL && mc_global.shell->type == SHELL_BASH)
        hidden = g_strdup_printf ("%s; history -d $HISTCMD 2>/dev/null", line);
    else
        hidden = g_strdup_printf (" %s", line);

    ok = mcterm_send_line (t, hidden);
    g_free (hidden);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_set_command_hint (WMcTerm *t, const char *command)
{
    if (t == NULL)
        return;

    g_free (t->command_hint);
    t->command_hint = g_strdup (command);
}

/* --------------------------------------------------------------------------------------------- */

char *
mcterm_running_command (const WMcTerm *t, int max_width)
{
#ifdef __linux__
    if (t != NULL && t->pty_master >= 0)
    {
        pid_t foreground = tcgetpgrp (t->pty_master);

        if (foreground > 0 && foreground != t->child_pid)
        {
            char *result = mcterm_command_from_process (foreground, max_width);

            if (result != NULL)
                return result;
        }
    }
#endif

    return (t != NULL) ? mcterm_command_from_line (t->command_hint, max_width) : NULL;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_shell_at_prompt (const WMcTerm *t)
{
    return (t != NULL && !t->child_dead && t->shell_at_prompt);
}

/* --------------------------------------------------------------------------------------------- */

/* Keys just sent are not on the screen until the shell echoes them. Before the line is read off
   the screen, give the shell a moment to answer. */
static void
mcterm_settle_line (WMcTerm *t)
{
    const gint64 deadline = g_get_monotonic_time () + MCTERM_LINE_SETTLE_USEC;

    while ((t->line_typed || t->line_cleared) && !t->child_dead && t->pty_master >= 0)
    {
        fd_set read_set;
        struct timeval timeout;
        const gint64 remaining = deadline - g_get_monotonic_time ();
        int rc;

        if (remaining <= 0)
            break;

        FD_ZERO (&read_set);
        FD_SET (t->pty_master, &read_set);
        timeout.tv_sec = (time_t) (remaining / G_USEC_PER_SEC);
        timeout.tv_usec = (suseconds_t) (remaining % G_USEC_PER_SEC);
        rc = select (t->pty_master + 1, &read_set, NULL, NULL, &timeout);
        if (rc < 0 && errno == EINTR)
            continue;
        if (rc <= 0)
            break;

        mcterm_pty_ready_cb (t->pty_master, t);
    }
}

/* --------------------------------------------------------------------------------------------- */

char *
mcterm_shell_line_text (WMcTerm *t)
{
    const mcview_terminal_buffer_t *buf;
    const int cols = WIDGET (t)->rect.cols;
    const int lines = WIDGET (t)->rect.lines;
    GString *text;
    gint64 row;
    int cursor_row;
    int col;

    if (!mcterm_shell_at_prompt (t) || !t->input_start_valid || t->vterm == NULL)
        return NULL;

    mcterm_settle_line (t);
    if (t->line_cleared)
        return NULL;

    /* Typed text starts where the prompt ended and runs on from there. It has scrolled that
       row away only if it is long; the part that is left is read all the same. */
    row = t->input_start_row - mcview_vterm_scrolled_rows (t->vterm);
    col = t->input_start_col;
    if (row < 0)
    {
        row = 0;
        col = 0;
    }

    buf = mcview_vterm_buf (t->vterm);
    cursor_row = mcview_vterm_cursor_row (t->vterm);
    text = g_string_new (NULL);

    /* The rows up to the cursor are the line's; past it, a row is the line's for as long as
       it has something on it. */
    for (; row < lines; row++, col = 0)
    {
        gsize row_start = text->len;
        gboolean blank = TRUE;

        for (; col < cols; col++)
        {
            const mcview_vterm_cell_t *cell = mcview_terminal_buffer_get (buf, (int) row, col);
            gunichar ch = (cell != NULL && cell->ch != 0) ? cell->ch : ' ';

            if (ch != ' ')
                blank = FALSE;
            g_string_append_unichar (text, ch);
        }

        if (blank && row > cursor_row)
        {
            g_string_truncate (text, row_start);
            break;
        }
    }

    while (text->len > 0 && text->str[text->len - 1] == ' ')
        g_string_truncate (text, text->len - 1);

    if (text->len == 0)
    {
        g_string_free (text, TRUE);
        return NULL;
    }

    return g_string_free (text, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_shell_line_is_empty (WMcTerm *t)
{
    char *text = mcterm_shell_line_text (t);

    g_free (text);
    return (text == NULL && !t->line_typed);
}

/* --------------------------------------------------------------------------------------------- */

int
mcterm_shell_line_point (WMcTerm *t)
{
    const int cols = WIDGET (t)->rect.cols;
    gint64 rows;

    if (!mcterm_shell_at_prompt (t) || !t->input_start_valid || t->vterm == NULL)
        return 0;

    mcterm_settle_line (t);
    rows = mcview_vterm_scrolled_rows (t->vterm) + mcview_vterm_cursor_row (t->vterm)
        - t->input_start_row;

    return MAX ((int) rows * cols + mcview_vterm_cursor_col (t->vterm) - t->input_start_col, 0);
}

/* --------------------------------------------------------------------------------------------- */

const char *
mcterm_osc7_raw (const WMcTerm *t)
{
    if (t == NULL || t->vterm == NULL)
        return NULL;
    return mcview_vterm_osc7_raw (t->vterm);
}

/* --------------------------------------------------------------------------------------------- */

const char *
mcterm_osc7_token (const WMcTerm *t)
{
    return (t != NULL) ? t->osc7_token : NULL;
}

/* --------------------------------------------------------------------------------------------- */

int
mcterm_last_exit_code (const WMcTerm *t)
{
    return (t != NULL) ? t->last_exit_code : -1;
}

/* --------------------------------------------------------------------------------------------- */

guint
mcterm_busy_phase (const WMcTerm *t)
{
    return (t != NULL) ? t->busy_phase : 0;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_set_busy_tick_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    if (t == NULL)
        return;

    t->on_busy_tick = cb;
    t->on_busy_tick_data = data;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_set_prompt_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    if (t == NULL)
        return;
    t->on_prompt_ready = cb;
    t->on_prompt_ready_data = data;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_set_after_redraw_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    if (t == NULL)
        return;
    t->on_after_redraw = cb;
    t->on_after_redraw_data = data;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_osc7_capable (const WMcTerm *t)
{
    return (t != NULL && t->osc7_capable);
}

/* --------------------------------------------------------------------------------------------- */

int
mcterm_cursor_col (const WMcTerm *t)
{
    if (t == NULL || t->vterm == NULL)
        return 0;
    return mcview_vterm_cursor_col (t->vterm);
}

/* --------------------------------------------------------------------------------------------- */

/* Hand one key to the shell, as if it had been typed at the terminal: the shell owns the line
   it is editing, so its own line editor does the typing, the history and the completion. */
gboolean
mcterm_send_key (WMcTerm *t, int key)
{
    if (t == NULL || t->child_dead || t->pty_master < 0)
        return FALSE;

    if (key >= 0x20 && key <= 0xFF && key != 0x7F)
    {
        t->line_typed = TRUE;
        t->line_cleared = FALSE;
    }

    return mcterm_send_encoded_key (t, key);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_send_tab_complete (WMcTerm *t, const char *text)
{
    if (t == NULL || t->child_dead || t->pty_master < 0)
        return FALSE;

    if (text != NULL && *text != '\0')
    {
        if (!mcterm_write_all (t->pty_master, (const unsigned char *) text, strlen (text)))
            return FALSE;
    }
    if (!mcterm_write_all (t->pty_master, (const unsigned char *) "\t", 1))
        return FALSE;

    if (t->osc7_capable)
    {
        t->shell_at_prompt = FALSE;
        t->last_osc7_gen = mcview_vterm_osc7_generation (t->vterm);
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

void
mcterm_draw_prompt_row (const WMcTerm *t, int screen_y, const char *skin_section, int color)
{
    const WRect *r;
    mcview_terminal_buffer_t *buf;
    mcview_canvas_colors_t colors;
    int cursor_row;

    if (t == NULL || t->vterm == NULL || t->child_dead)
        return;

    r = &CONST_WIDGET (t)->rect;
    buf = mcview_vterm_buf (t->vterm);
    cursor_row = mcview_vterm_cursor_row (t->vterm);

    /* The row belongs to the host, and so do its colors: it stands next to
       whatever the host puts on the rest of that row. */
    colors.section = skin_section;
    colors.normal = color;
    colors.bold = -1;
    colors.underline = -1;
    colors.bold_underline = -1;

    mcview_render_terminal_canvas (buf, cursor_row, screen_y, r->x, 1, r->cols, &colors);
}
