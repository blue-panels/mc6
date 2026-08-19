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
#include "lib/widget.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/skin.h"

#include "src/viewer/vterm.h"
#include "src/viewer/terminal_buffer.h"
#include "src/keymap.h"

#include "mcterm.h"
#include "mcterm_key.h"
#include "mcterm_proto.h"
#include "mcterm_select.h"

/*** file scope variables ************************************************************************/

#define MCTERM_INTERNAL_SYNC_TIMEOUT_USEC G_USEC_PER_SEC

#define MCTERM_INITIAL_OSC7_MARKER        "7;file://__mc_sync__/"
/* Stands in the shell setup where the session token goes. */
#define MCTERM_TOKEN_PLACEHOLDER "@MCTOKEN@"
#define MCTERM_WHEEL_ROWS        3
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
    int last_exit_code;
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
    /* Where the terminal is being read, as against where the shell is typing.
       It exists while the widget has the focus, and the arrows move it. */
    gboolean cursor_valid;
    gint64 cursor_row;
    int cursor_col;
};

/* Where the rows of the screen fall in the output as a whole. */
typedef struct
{
    gboolean compose;   // history and live rows are drawn as one view
    gboolean snapshot;  // a held-back frame is on the screen
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
static gboolean mcterm_osc7_is_ours (const WMcTerm *t, const char *raw);
static gboolean mcterm_handle_osc133_generation (WMcTerm *t);
static void mcterm_busy_tick (WMcTerm *t);
static void mcterm_busy_tick_set (WMcTerm *t, gboolean on);
static gboolean mcterm_handle_stalled_internal_sync (WMcTerm *t);
static int mcterm_resolve_top_row_for_buf (const WMcTerm *t, const mcview_terminal_buffer_t *buf,
                                           int rows);

/*** file scope functions ************************************************************************/

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
        if (t->awaiting_command_done || t->shell_at_prompt)
            return FALSE;
        t->shell_at_prompt = TRUE;
        mcterm_busy_tick_set (t, FALSE);
        return TRUE;

    case MCTERM_MARK_COMMAND_START:
        t->shell_at_prompt = FALSE;
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
    gboolean ok = TRUE;

    if (tcgetattr (master, &tt) == 0 && (tt.c_lflag & ECHO) != 0)
    {
        struct termios tt_noecho = tt;

        tt_noecho.c_lflag &= ~(tcflag_t) ECHO;
        if (tcsetattr (master, TCSANOW, &tt_noecho) == 0)
            echo_was_on = TRUE;
    }

    {
        const char *p = data;
        size_t remaining = len;

        while (remaining > 0)
        {
            ssize_t nw = write (master, p, remaining);
            if (nw < 0)
            {
                if (errno == EINTR)
                    continue;
                ok = FALSE;
                break;
            }
            if (nw == 0)
            {
                ok = FALSE;
                break;
            }
            p += nw;
            remaining -= (size_t) nw;
        }
    }

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

/* Move the view @delta rows, up when negative. TRUE if it moved. */
static gboolean
mcterm_scroll_view (WMcTerm *t, int delta)
{
    int back;
    int max;

    if (t->vterm == NULL || mcview_vterm_in_alt_screen (t->vterm) || !t->scroll_allowed)
        return FALSE;

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
    if (t->scrollback != 0)
    {
        t->scrollback = 0;
        widget_draw (WIDGET (t));
    }
}

/* --------------------------------------------------------------------------------------------- */

/* FALSE when there is no terminal to draw. */
static gboolean
mcterm_geometry (const WMcTerm *t, mcterm_geom_t *g)
{
    const WRect *r = &WIDGET (t)->rect;
    int max_row, cursor_row, effective_max;
    gboolean fill_top;

    if (t->vterm == NULL)
        return FALSE;

    g->snapshot = t->pending_internal_sync && t->sync_snapshot_buf != NULL;
    g->buf = g->snapshot ? t->sync_snapshot_buf : mcview_vterm_buf (t->vterm);
    g->top_row = g->snapshot ? mcterm_resolve_top_row_for_buf (t, g->buf, r->lines)
                             : mcview_vterm_resolve_top_row (t->vterm, r->lines);
    max_row = mcview_terminal_buffer_max_row (g->buf);
    cursor_row = g->snapshot ? t->sync_snapshot_cursor_row : mcview_vterm_cursor_row (t->vterm);

    // The row the shell types on is the host's, see mcterm_draw_prompt_row().
    if (t->shell_at_prompt && t->osc7_capable && !mcview_vterm_in_alt_screen (t->vterm))
        effective_max = cursor_row - 1;
    else
        effective_max = (cursor_row > max_row) ? cursor_row : max_row;
    if (effective_max >= g->top_row + r->lines)
        g->top_row = effective_max - r->lines + 1;

    g->content_rows = (effective_max >= g->top_row) ? (effective_max - g->top_row + 1) : 0;
    if (g->content_rows > r->lines)
        g->content_rows = r->lines;
    g->blank_above = (!mcview_vterm_in_alt_screen (t->vterm) && g->content_rows < r->lines)
        ? (r->lines - g->content_rows)
        : 0;

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

/* The number of the row under @y, and the column under @x. */
static gboolean
mcterm_row_at (const WMcTerm *t, int y, int x, gint64 *row, int *col)
{
    const WRect *r = &WIDGET (t)->rect;
    mcterm_geom_t g;

    if (!mcterm_geometry (t, &g))
        return FALSE;

    y = CLAMP (y, g.blank_above, r->lines - 1);
    *row = g.first_abs + (y - g.blank_above);
    *col = CLAMP (x, 0, r->cols - 1);

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

    screen_row = g.blank_above + (row - g.first_abs);
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
    const gint64 oldest = scrolled - mcview_vterm_history_len (t->vterm);
    mcterm_geom_t g;
    gint64 newest;
    gint64 row;
    int col;

    if (!mcterm_geometry (t, &g))
        return;

    // The cursor stays on what the terminal draws, and off the command line.
    newest = MAX (g.newest_abs, oldest);

    if (!t->cursor_valid)
        mcterm_cursor_reset (t);

    row = t->cursor_row;
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
        col = mcterm_row_last_col (t, row, r->cols);
        col = (col < 0) ? 0 : MIN (col + 1, r->cols - 1);
        break;
    case CK_MarkToHome:
        col = 0;
        break;
    case CK_MarkToEnd:
        // On the text, so that what is lit up is the text and nothing besides.
        col = MAX (mcterm_row_last_col (t, row, r->cols), 0);
        break;
    default:
        break;
    }

    row = CLAMP (row, oldest, newest);
    col = CLAMP (col, 0, r->cols - 1);

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
        const gint64 abs_row = g->first_abs + (row - g->blank_above);
        int from, to, col;

        if (!mcterm_sel_row_span (&t->sel, abs_row, r->cols, &from, &to))
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

static void
mcterm_do_draw (WMcTerm *t)
{
    const WRect *r = &WIDGET (t)->rect;

    (void) mcterm_handle_stalled_internal_sync (t);

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

            view = mcterm_compose_view (t, r->lines, g.top_row, g.content_rows, t->scrollback);
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
        /* Focused, the terminal shows where it is being read; the shell has
           the cursor back as soon as the focus goes to the command line. */
        if (widget_get_state (w, WST_FOCUSED) && t->cursor_valid && t->vterm != NULL
            && !mcview_vterm_in_alt_screen (t->vterm))
        {
            mcterm_geom_t g;

            if (mcterm_geometry (t, &g))
            {
                const WRect *r = &w->rect;
                const gint64 screen_row = g.blank_above + (t->cursor_row - g.first_abs);

                if (screen_row >= 0 && screen_row < r->lines)
                {
                    tty_gotoyx (r->y + (int) screen_row,
                                r->x + CLAMP (t->cursor_col, 0, r->cols - 1));
                    return MSG_HANDLED;
                }
            }
        }
        if (t->shell_at_prompt && t->osc7_capable)
            return MSG_NOT_HANDLED;
        if (t->vterm != NULL && !t->child_dead)
        {
            const WRect *r = &w->rect;
            mcview_terminal_buffer_t *buf = mcview_vterm_buf (t->vterm);
            int top_row = mcview_vterm_resolve_top_row (t->vterm, r->lines);
            int max_row = mcview_terminal_buffer_max_row (buf);
            int cursor_row = mcview_vterm_cursor_row (t->vterm);
            int effective_max = (cursor_row > max_row) ? cursor_row : max_row;
            if (effective_max >= top_row + r->lines)
                top_row = effective_max - r->lines + 1;
            int content_rows = (effective_max >= top_row) ? (effective_max - top_row + 1) : 0;
            int blank_above = (!mcview_vterm_in_alt_screen (t->vterm) && content_rows < r->lines)
                ? (r->lines - content_rows)
                : 0;
            int crow = cursor_row - top_row + blank_above;
            int ccol = mcview_vterm_cursor_col (t->vterm);

            if (crow < 0)
                crow = 0;
            if (crow >= r->lines)
                crow = r->lines - 1;
            if (ccol < 0)
                ccol = 0;
            if (ccol >= r->cols)
                ccol = r->cols - 1;
            tty_gotoyx (r->y + crow, r->x + ccol);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    case MSG_RESIZE:
        widget_default_callback (w, NULL, MSG_RESIZE, 0, data);
        mcview_vterm_set_size (t->vterm, w->rect.lines, w->rect.cols);
        if (!t->child_dead && t->pty_master >= 0)
        {
            struct winsize ws;

            ws.ws_row = (unsigned short) w->rect.lines;
            ws.ws_col = (unsigned short) w->rect.cols;
            ws.ws_xpixel = 0;
            ws.ws_ypixel = 0;
            ioctl (t->pty_master, TIOCSWINSZ, &ws);
        }
        return MSG_HANDLED;

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
        if (t->vterm != NULL && !mcview_vterm_in_alt_screen (t->vterm))
        {
            const int page = WIDGET (t)->rect.lines - 1;
            const long command = mcterm_key_command (t, parm);

            switch (command)
            {
            case CK_Store:
                // A panel over it, or nothing marked: the key is someone else's.
                if (!t->scroll_allowed || !t->sel.active)
                {
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

            case CK_ScrollUp:
            case CK_ScrollDown:
            case CK_PageUp:
            case CK_PageDown:
            case CK_Top:
            case CK_Bottom:
            {
                const int hist = mcview_vterm_history_len (t->vterm);
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
                if (t->scrollback > 0 || command == CK_ScrollUp || command == CK_ScrollDown)
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
        }

        if (t->child_dead || t->pty_master < 0)
            return MSG_NOT_HANDLED;
        return mcterm_send_encoded_key (t, parm) ? MSG_HANDLED : MSG_NOT_HANDLED;

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
        mcview_vterm_free (t->vterm);
        t->vterm = NULL;
        t->last_osc7_gen = 0;
        g_free (t->osc7_token);
        t->osc7_token = NULL;
        mcview_terminal_buffer_free (t->sync_snapshot_buf);
        t->sync_snapshot_buf = NULL;
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

        ws.ws_row = (unsigned short) r->lines;
        ws.ws_col = (unsigned short) r->cols;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
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
    mcview_vterm_set_size (t->vterm, r->lines, r->cols);

    {
        struct winsize ws;

        ws.ws_row = (unsigned short) r->lines;
        ws.ws_col = (unsigned short) r->cols;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl (master, TIOCSWINSZ, &ws);
    }

    add_select_channel (master, mcterm_pty_ready_cb, t);

    {
        const shell_type_t shell_type =
            (mc_global.shell != NULL) ? mc_global.shell->type : SHELL_NONE;

        t->osc7_token = g_strdup_printf ("%08x%08x", g_random_int (), g_random_int ());

        switch (shell_type)
        {
        case SHELL_BASH:
        {
            /* Percent-encoder for $PWD, a hook that reports the directory and the exit code
             * after every command, and a prompt wrapped in marks that say where it ends. */
            static const char setup[] =
                "__mc_pe(){"
                " local s=$1 o= i c;"
                " for((i=0;i<${#s};i++)); do"
                " c=${s:i:1};"
                " case $c in"
                " [a-zA-Z0-9/_~.-]) o+=$c;;"
                " *) printf -v o '%s%%%02X' \"$o\" \"'$c\";;"
                " esac; done;"
                " printf '%s' \"$o\";"
                " }; \\\n"
                "__mc_pc(){"
                " local e=$?;"
                " printf '\\033]133;D;%s;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\007' \"$e\";"
                " printf '\\033]7;file://%s" MCTERM_OSC7_TOKEN_PREFIX MCTERM_TOKEN_PLACEHOLDER
                "\\007' \"$(__mc_pe \"$PWD\")\";"
                /* Setting PS1 is an everyday thing to do at a prompt, and it would throw the
                   marks away. Put them back whenever they are gone. */
                " case \"$PS1\" in *'133;B;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER "'*) ;;"
                " *) PS1=\"\\[\\e]133;A;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\a\\]${PS1}\\[\\e]133;B;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\a\\]\";;"
                " esac;"
                " return $e;"
                " }; \\\n"
                " if test $BASH_VERSINFO -ge 5"
                " && [[ ${PROMPT_COMMAND@a} == *a* ]] 2>/dev/null; then \\\n"
                "  PROMPT_COMMAND+=(__mc_pc); \\\n"
                " else \\\n"
                "  PROMPT_COMMAND=\"${PROMPT_COMMAND:+$PROMPT_COMMAND; }__mc_pc\"; \\\n"
                " fi; \\\n"
                " PS0=\"\\[\\e]133;C;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\a\\]${PS0}\"; \\\n"
                " printf "
                "'\\033]7;file://__mc_sync__/" MCTERM_OSC7_TOKEN_PREFIX MCTERM_TOKEN_PLACEHOLDER
                "\\007'\r";

            mcterm_enable_osc7 (t, master, setup);
            break;
        }

        case SHELL_ZSH:
        {
            static const char setup[] =
                " __mc_pe(){local s=$1 o='' c i;for (( i=1; i<=${#s}; i++ )); do c=${s[i]};"
                "case $c in [a-zA-Z0-9/_~.-])o+=$c;;*)printf -v o '%s%%%02X' \"$o\" \"'$c\";"
                ";esac;done;printf %s \"$o\";}; \\\n"
                " __mc_first=1;__mc_precmd(){local e=$?;if (( __mc_first ));then printf "
                "'\\033[2J\\033[H';"
                "__mc_first=0;fi;"
                "printf '\\033]133;D;%s;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\007' \"$e\";"
                "printf '\\033]7;file://%s" MCTERM_OSC7_TOKEN_PREFIX MCTERM_TOKEN_PLACEHOLDER
                "\\007' \"$(__mc_pe \"$PWD\")\";"
                /* An assignment to PROMPT throws the marks away: put them back. */
                "[[ $PROMPT == *'133;B;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER "'* ]]"
                " || PROMPT=$'%{\\e]133;A;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\a%}'$PROMPT$'%{\\e]133;B;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\a%}';};"
                "precmd_functions+=(__mc_precmd); \\\n"
                " __mc_preexec(){printf "
                "'\\033]133;C;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER "\\007';};"
                "preexec_functions+=(__mc_preexec); \\\n"
                " printf "
                "'\\033]7;file://__mc_sync__/" MCTERM_OSC7_TOKEN_PREFIX MCTERM_TOKEN_PLACEHOLDER
                "\\007'\r";

            mcterm_enable_osc7 (t, master, setup);
            break;
        }

        case SHELL_FISH:
        {
            /* fish has an event for everything, so nothing here replaces what the user set:
             * the prompt function is copied and called, the rest hangs off events. */
            static const char setup[] =
                "functions -q __mc_orig_prompt; or functions -c fish_prompt __mc_orig_prompt; "
                "function fish_prompt; printf "
                "'\\033]133;A;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\a'; __mc_orig_prompt; "
                "printf '\\033]133;B;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER "\\a'; end; "
                "function __mc_preexec --on-event fish_preexec; "
                "printf '\\033]133;C;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER "\\a'; end; "
                "function __mc_postexec --on-event fish_postexec; "
                "printf '\\033]133;D;%s;" MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
                "\\a' $status; end; "
                "function __mc_cwd --on-event fish_prompt; "
                "printf '\\033]7;file://%s" MCTERM_OSC7_TOKEN_PREFIX MCTERM_TOKEN_PLACEHOLDER
                "\\a' (string escape --style=url -- $PWD); end; "
                "printf "
                "'\\033]7;file://__mc_sync__/" MCTERM_OSC7_TOKEN_PREFIX MCTERM_TOKEN_PLACEHOLDER
                "\\a'\r";

            mcterm_enable_osc7 (t, master, setup);
            break;
        }

        default:
            /* sh, dash, ash, ksh, mksh, tcsh, and whatever else the user runs: nothing to
             * chain a hook onto, so the terminal goes without the protocol. It still runs
             * commands; what it does not do is follow the shell's directory or know where
             * the prompt ends. See mcterm_osc7_capable(). */
            break;
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
        mcterm_sel_start (&t->sel, row, col);
        t->cursor_row = row;
        t->cursor_col = col;
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
    if (t != NULL && t->scrollback != 0)
    {
        t->scrollback = 0;
        widget_draw (WIDGET (t));
    }
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

gboolean
mcterm_send_internal_line (WMcTerm *t, const char *line)
{
    if (t == NULL || line == NULL)
        return FALSE;

    mcview_terminal_buffer_free (t->sync_snapshot_buf);

    t->sync_snapshot_buf = mcview_terminal_buffer_copy (mcview_vterm_buf (t->vterm));
    t->sync_snapshot_cursor_row = mcview_vterm_cursor_row (t->vterm);
    t->pending_internal_sync = TRUE;
    t->waiting_for_initial_osc7 = FALSE;
    t->internal_sync_deadline = g_get_monotonic_time () + MCTERM_INTERNAL_SYNC_TIMEOUT_USEC;

    return mcterm_send_line (t, line);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcterm_shell_at_prompt (const WMcTerm *t)
{
    return (t != NULL && !t->child_dead && t->shell_at_prompt);
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
