/*
   File manager for the M-Commander
   The panels follow what the filesystem does

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

/** \file dir_watch.c
 *  \brief Source: the panels follow what the filesystem does
 */

#include <config.h>

#include "dir_watch.h"

#if defined(HAVE_SYS_INOTIFY_H) && defined(HAVE_SYS_TIMERFD_H)
#define DIR_WATCH_INOTIFY 1
#endif

#ifdef DIR_WATCH_INOTIFY

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/timerfd.h>

#include "lib/global.h"
#include "lib/tty/key.h"  // add_select_channel(), delete_select_channel()
#include "lib/vfs/vfs.h"
#include "lib/widget.h"
#include "lib/widget/dialog-switch.h"  // filemanager

#include "src/setup.h"  // panels_options

#include "layout.h"  // get_panel_type(), get_panel_widget()

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/* IN_MODIFY is left out on purpose: a build in the directory would report every write, while the
   panel only shows what a closed file ends up as. */
#define DIR_WATCH_MASK                                                                             \
    (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_CLOSE_WRITE              \
     | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_EXCL_UNLINK)

// A burst of events is one reread: wait for the writing to stop first.
#define DIR_WATCH_QUIET_MS 250
// A long list costs more to reread, so it waits longer.
#define DIR_WATCH_QUIET_LONG_MS 1000
#define DIR_WATCH_LONG_LIST     20000
// However long the writing goes on, the panel is not left stale for longer than this.
#define DIR_WATCH_BURST_MAX_MS 2000

/*** file scope type declarations ****************************************************************/

typedef struct
{
    WPanel *panel;
    int wd;          // inotify watch, -1 when the panel has none
    gboolean stale;  // the directory changed; the panel waits to be reread
} dir_watch_t;

/*** file scope variables ************************************************************************/

static GArray *dir_watches = NULL;
static int dir_watch_fd = -1;
static int dir_watch_timer_fd = -1;
// When the current burst of events started, 0 when there is none.
static gint64 dir_watch_burst_start = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static dir_watch_t *
dir_watch_find (const WPanel *panel)
{
    guint i;

    if (dir_watches == NULL)
        return NULL;

    for (i = 0; i < dir_watches->len; i++)
    {
        dir_watch_t *w = &g_array_index (dir_watches, dir_watch_t, i);

        if (w->panel == panel)
            return w;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

/* Two panels can show one directory, and inotify gives them the same watch. */
static gboolean
dir_watch_wd_shared (int wd, const dir_watch_t *except)
{
    guint i;

    for (i = 0; i < dir_watches->len; i++)
    {
        const dir_watch_t *w = &g_array_index (dir_watches, dir_watch_t, i);

        if (w != except && w->wd == wd)
            return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
dir_watch_drop (dir_watch_t *w)
{
    if (w->wd >= 0 && !dir_watch_wd_shared (w->wd, w))
        inotify_rm_watch (dir_watch_fd, w->wd);

    w->wd = -1;
    w->stale = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
dir_watch_mark (int wd, gboolean gone)
{
    guint i;

    for (i = 0; i < dir_watches->len; i++)
    {
        dir_watch_t *w = &g_array_index (dir_watches, dir_watch_t, i);

        if (w->wd != wd)
            continue;

        w->stale = TRUE;
        // The watch went with the directory; the reread finds a parent to show.
        if (gone)
            w->wd = -1;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
dir_watch_mark_all (void)
{
    guint i;

    for (i = 0; i < dir_watches->len; i++)
        g_array_index (dir_watches, dir_watch_t, i).stale = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
dir_watch_any_stale (void)
{
    guint i;

    for (i = 0; i < dir_watches->len; i++)
        if (g_array_index (dir_watches, dir_watch_t, i).stale)
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
dir_watch_timer_set (long ms)
{
    struct itimerspec its;

    if (dir_watch_timer_fd < 0)
        return;

    memset (&its, 0, sizeof (its));
    its.it_value.tv_sec = ms / 1000;
    its.it_value.tv_nsec = (ms % 1000) * 1000000L;
    timerfd_settime (dir_watch_timer_fd, 0, &its, NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* How long the writing has to stop for before the panel is reread. A long list costs more to
   reread, so it waits longer. */
static long
dir_watch_quiet_ms (void)
{
    guint i;

    for (i = 0; i < dir_watches->len; i++)
        if (g_array_index (dir_watches, dir_watch_t, i).panel->dir.len > DIR_WATCH_LONG_LIST)
            return DIR_WATCH_QUIET_LONG_MS;

    return DIR_WATCH_QUIET_MS;
}

/* --------------------------------------------------------------------------------------------- */

static void
dir_watch_timer_arm (void)
{
    gint64 now;

    if (dir_watch_timer_fd < 0)
        return;

    now = g_get_monotonic_time ();

    /* The timer starts again with every event, so writing that goes on and on would keep the
       panel stale. Past this point the reread waits for the timer that is already running. */
    if (dir_watch_burst_start != 0
        && now - dir_watch_burst_start > DIR_WATCH_BURST_MAX_MS * G_TIME_SPAN_MILLISECOND)
        return;

    if (dir_watch_burst_start == 0)
        dir_watch_burst_start = now;

    dir_watch_timer_set (dir_watch_quiet_ms ());
}

/* --------------------------------------------------------------------------------------------- */

static int
dir_watch_read_cb (int fd, void *info)
{
    /* The name of an event is in the buffer after it, so the buffer holds whole events only. */
    union
    {
        struct inotify_event ev;
        char buf[4096];
    } chunk;
    ssize_t len;
    gboolean any = FALSE;

    (void) info;

    while ((len = read (fd, &chunk, sizeof (chunk))) > 0)
    {
        const char *p;

        for (p = chunk.buf; p + (ssize_t) sizeof (struct inotify_event) <= chunk.buf + len;)
        {
            const struct inotify_event *ev = (const struct inotify_event *) p;

            if ((ev->mask & IN_Q_OVERFLOW) != 0)
                dir_watch_mark_all ();
            else
                dir_watch_mark (ev->wd, (ev->mask & (IN_IGNORED | IN_UNMOUNT)) != 0);

            any = TRUE;
            p += sizeof (struct inotify_event) + ev->len;
        }
    }

    if (any)
        dir_watch_timer_arm ();

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* The panels are reread while the file manager itself waits for a key. Under a dialog of its own,
   or under a file operation that walks the list, they are left alone. */
static gboolean
dir_watch_can_reload (void)
{
    return filemanager != NULL && top_dlg != NULL && DIALOG (top_dlg->data) == filemanager;
}

/* --------------------------------------------------------------------------------------------- */

static int
dir_watch_timer_cb (int fd, void *info)
{
    guint64 ticks;

    (void) info;

    while (read (fd, &ticks, sizeof (ticks)) > 0)
        ;

    dir_watch_burst_start = 0;

    if (!dir_watch_can_reload ())
    {
        // Ask again later: what is on top now has the screen and the list.
        if (dir_watch_any_stale ())
            dir_watch_timer_set (DIR_WATCH_QUIET_LONG_MS);
        return 0;
    }

    dir_watch_reload_pending ();

    // Leave the wait for a key: the screen has changed under it.
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
dir_watch_panel_ready (const WPanel *panel)
{
    const Widget *w = CONST_WIDGET (panel);

    if (!widget_get_state (w, WST_VISIBLE))
        return FALSE;
    // The list of a plugin or of a panelized panel is not this directory.
    if (panel->is_plugin_panel || panel->is_panelized)
        return FALSE;
    // A reread stops the quick search, and the user is typing in it.
    if (panel->quick_search.active)
        return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
dir_watch_reload_one (dir_watch_t *w)
{
    WPanel *panel = w->panel;
    const file_entry_t *fe;
    char *current = NULL;

    w->stale = FALSE;

    fe = panel_current_entry (panel);
    if (fe != NULL)
        current = g_strndup (fe->fname->str, fe->fname->len);

    // We know the directory changed, so "Fast dir reload" must not talk us out of the reread.
    memset (&panel->dir_stat, 0, sizeof (panel->dir_stat));
    panel_reload (panel);
    panel_set_current_by_name (panel, current);
    g_free (current);

    panel->dirty = TRUE;
    widget_draw (WIDGET (panel));

    // The directory may be gone and the panel on a parent of it now.
    dir_watch_track (panel);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
dir_watch_init (void)
{
    struct itimerspec its;
    int i;

    if (dir_watch_fd >= 0)
        return;

    dir_watch_fd = inotify_init1 (IN_NONBLOCK | IN_CLOEXEC);
    if (dir_watch_fd < 0)
        return;

    dir_watch_timer_fd = timerfd_create (CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (dir_watch_timer_fd < 0)
    {
        close (dir_watch_fd);
        dir_watch_fd = -1;
        return;
    }

    // Disarmed until there is something to wait for.
    memset (&its, 0, sizeof (its));
    timerfd_settime (dir_watch_timer_fd, 0, &its, NULL);

    dir_watches = g_array_new (FALSE, TRUE, sizeof (dir_watch_t));
    add_select_channel (dir_watch_fd, dir_watch_read_cb, NULL);
    add_select_channel (dir_watch_timer_fd, dir_watch_timer_cb, NULL);

    // The panels are built before this, so they are taken as they are found.
    for (i = 0; i < 2; i++)
        if (get_panel_type (i) == view_listing)
            dir_watch_track (PANEL (get_panel_widget (i)));
}

/* --------------------------------------------------------------------------------------------- */

void
dir_watch_shutdown (void)
{
    if (dir_watch_fd < 0)
        return;

    delete_select_channel (dir_watch_fd);
    delete_select_channel (dir_watch_timer_fd);
    close (dir_watch_fd);
    close (dir_watch_timer_fd);
    dir_watch_fd = -1;
    dir_watch_timer_fd = -1;
    dir_watch_burst_start = 0;

    g_array_free (dir_watches, TRUE);
    dir_watches = NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
dir_watch_track (WPanel *panel)
{
    dir_watch_t *w;
    int wd;

    if (dir_watch_fd < 0 || panel == NULL)
        return;

    w = dir_watch_find (panel);

    if (!panels_options.watch_dirs || panel->is_plugin_panel || panel->cwd_vpath == NULL
        || !vfs_file_is_local (panel->cwd_vpath))
    {
        if (w != NULL)
            dir_watch_drop (w);
        return;
    }

    /* A watch on a directory reports its own entries, whatever their number: the cost is one
       watch per panel, not one per file. */
    wd = inotify_add_watch (dir_watch_fd, vfs_path_as_str (panel->cwd_vpath), DIR_WATCH_MASK);

    if (w == NULL)
    {
        dir_watch_t entry = { .panel = panel, .wd = -1, .stale = FALSE };

        g_array_append_val (dir_watches, entry);
        w = &g_array_index (dir_watches, dir_watch_t, dir_watches->len - 1);
    }

    if (w->wd != wd)
    {
        int old = w->wd;

        w->wd = wd;
        // A watch of our own that nothing points at any more.
        if (old >= 0 && !dir_watch_wd_shared (old, NULL))
            inotify_rm_watch (dir_watch_fd, old);
    }

    /* The listing was read when the panel got here, so whatever happened before that is in it.
       On a filesystem inotify cannot watch (NFS and the like) wd is -1 and mc works as it did. */
    w->stale = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

void
dir_watch_forget (WPanel *panel)
{
    guint i;

    if (dir_watch_fd < 0)
        return;

    for (i = 0; i < dir_watches->len; i++)
    {
        dir_watch_t *w = &g_array_index (dir_watches, dir_watch_t, i);

        if (w->panel == panel)
        {
            dir_watch_drop (w);
            g_array_remove_index (dir_watches, i);
            return;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

void
dir_watch_reload_pending (void)
{
    guint i;

    if (dir_watch_fd < 0)
        return;

    for (i = 0; i < dir_watches->len; i++)
    {
        dir_watch_t *w = &g_array_index (dir_watches, dir_watch_t, i);

        if (w->stale && dir_watch_panel_ready (w->panel))
            dir_watch_reload_one (w);
    }
}

/* --------------------------------------------------------------------------------------------- */

#else /* DIR_WATCH_INOTIFY */

void
dir_watch_init (void)
{
}

void
dir_watch_shutdown (void)
{
}

void
dir_watch_track (WPanel *panel)
{
    (void) panel;
}

void
dir_watch_forget (WPanel *panel)
{
    (void) panel;
}

void
dir_watch_reload_pending (void)
{
}

#endif /* DIR_WATCH_INOTIFY */
