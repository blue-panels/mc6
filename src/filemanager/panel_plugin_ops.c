/*
   Panel plugin file operations -- copy, move, delete, create, put.

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

/** \file panel_plugin_ops.c
 *  \brief Source: file operations on a panel driven by a panel plugin
 */

#include <config.h>

#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/vfs/vfs.h"
#include "lib/strutil.h"  // str_trunc()
#include "lib/file-entry.h"
#include "lib/timefmt.h"  // file_date()
#include "lib/util.h"     // mc_build_filename(), size_trunc_len()
#include "lib/widget.h"
#include "lib/panel-plugin.h"

#include "src/setup.h"    // confirm_delete, panels_options, use_internal_edit
#include "src/history.h"  // MC_HISTORY_FM_PLUGIN_COPY

#include "filemanager.h"  // other_panel
#include "ioblksize.h"    // IO_BUFSIZE
#include "panel.h"
#include "cmd.h"  // edit_file_at_line()

#include "panel_plugin_ops.h"  // Our definitions

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/** A panel item to work on: the name as the plugin knows it, and enough of the
    stat to tell a directory from a file. */
typedef struct
{
    char *name;
    mode_t mode;
} plugin_panel_item_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Copy a local file using POSIX read/write, preserving permissions.
 *
 * @param src source path (temp file from plugin)
 * @param dest destination path
 * @return TRUE on success
 */

static gboolean
copy_local_file (const char *src, const char *dest)
{
    struct stat st;
    int fd_src, fd_dest;
    char buf[IO_BUFSIZE];
    ssize_t nread;
    gboolean ok = TRUE;

    fd_src = open (src, O_RDONLY);
    if (fd_src == -1)
        return FALSE;

    if (fstat (fd_src, &st) != 0)
        st.st_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    fd_dest = open (dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (fd_dest == -1)
    {
        close (fd_src);
        return FALSE;
    }

    while ((nread = read (fd_src, buf, sizeof (buf))) > 0)
    {
        const char *p = buf;
        ssize_t remaining = nread;

        while (remaining > 0)
        {
            ssize_t nw = write (fd_dest, p, remaining);
            if (nw == -1)
            {
                ok = FALSE;
                goto done;
            }
            p += nw;
            remaining -= nw;
        }
    }

    if (nread == -1)
        ok = FALSE;

done:
    close (fd_src);
    close (fd_dest);
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

typedef enum
{
    PP_OVERWRITE_ASK = 0,
    PP_OVERWRITE_ALL,
    PP_OVERWRITE_NONE,
    PP_OVERWRITE_RESUME,  // finish the short ones, leave the rest alone
    PP_OVERWRITE_ABORT
} plugin_panel_overwrite_t;

/** What was decided once and applies to the rest of the run. */
typedef struct
{
    plugin_panel_overwrite_t mode;
} plugin_panel_overwrite_state_t;

typedef enum
{
    PP_ACT_WRITE = 0,  // replace what is there
    PP_ACT_RESUME,     // carry on from where the last attempt stopped
    PP_ACT_SKIP,
    PP_ACT_ABORT
} plugin_panel_action_t;

/** What is known about a name that is already taken. */
typedef struct
{
    char *src_path;  // full path, not just a name: it is shown in the dialog
    char *dest_path;
    struct stat src_st;
    struct stat dest_st;
    gboolean have_src;
    gboolean have_dest;
} plugin_panel_existing_t;

static void
plugin_panel_existing_clear (plugin_panel_existing_t *info)
{
    MC_PTR_FREE (info->src_path);
    MC_PTR_FREE (info->dest_path);
}

/** Full path of a file inside a plugin panel. Caller frees the result. */
static char *
plugin_panel_entry_path (const WPanel *panel, const char *name)
{
    char *loc;
    char *result;

    if (!panel->is_plugin_panel || panel->plugin == NULL || panel->plugin_data == NULL
        || panel->plugin->get_location == NULL)
        return mc_build_filename (vfs_path_as_str (panel->cwd_vpath), name, (char *) NULL);

    loc = panel->plugin->get_location (panel->plugin_data);
    if (loc == NULL)
        return g_strdup (name);

    result = g_str_has_suffix (loc, PATH_SEP_STR)
        ? g_strconcat (loc, name, (char *) NULL)
        : g_strconcat (loc, PATH_SEP_STR, name, (char *) NULL);
    g_free (loc);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/** Path as it should be shown in a dialog, truncated to @width. Result points
    into a static buffer. */
static const char *
plugin_panel_shown_path (const char *path, int width)
{
    static char buf[BUF_MEDIUM];
    char *shown = NULL;

    if (path == NULL)
        return _ ("unknown");

    /* A plugin location is not a filesystem path: vfs_path_from_str would
       resolve it against the current directory, turning "sh://host/dir/file"
       into "~/dev/mc/sh://host/dir/file". */
    if (panel_plugin_find_by_path (path) != NULL)
        shown = g_strdup (path);
    else
    {
        vfs_path_t *vpath;

        vpath = vfs_path_from_str (path);
        shown = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_HOME | VPF_STRIP_PASSWORD);
        vfs_path_free (vpath, TRUE);
    }

    g_strlcpy (buf, str_trunc (shown, width), sizeof (buf));
    g_free (shown);

    return buf;
}

/* --------------------------------------------------------------------------------------------- */

/** Append "63030   Jul 26 23:13", or "unknown" when @known is FALSE. */
static void
plugin_panel_describe_size (GString *out, const struct stat *st, gboolean known)
{
    char size[BUF_TINY];

    if (!known)
    {
        g_string_append (out, _ ("unknown"));
        return;
    }

    size_trunc_len (size, sizeof (size), (uintmax_t) st->st_size, 0, panels_options.kilobyte_si);
    g_string_append_printf (out, "%12s   %s", size, file_date (st->st_mtime));
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

/**
 * Ask about a name that is already taken.
 *
 * The checkbox turns the answer into a standing one, kept in @state for the
 * rest of the run.
 *
 * Not filegui.c's dialog: that one reads its inputs out of file_progress_ui_t
 * and stores itself there, and none of these paths have such a context.
 */
static plugin_panel_action_t
plugin_panel_ask_existing (const plugin_panel_existing_t *info,
                           plugin_panel_overwrite_state_t *state)
{
    enum
    {
        B_OVERWRITE = B_USER,
        B_SKIP,
        B_RESUME,
    };

    WDialog *dlg;
    WGroup *g;
    WCheck *remember;
    WButton *buttons[4];
    GString *first;
    GString *second;
    gboolean can_continue;
    int result;
    int y;
    int i;
    int buttons_width = 0;
    int x;
    const int width = 70;
    const int path_width = width - 6;
    const int gap = 2;
    /* Rows 1 to 8 are the description, the separators and the checkbox. */
    const int button_row = 9;

    /* A standing answer from an earlier file. */
    switch (state->mode)
    {
    case PP_OVERWRITE_ALL:
        return PP_ACT_WRITE;
    case PP_OVERWRITE_NONE:
        return PP_ACT_SKIP;
    case PP_OVERWRITE_ABORT:
        return PP_ACT_ABORT;
    case PP_OVERWRITE_RESUME:
        /* A standing Resume never overwrites: only a short destination is
           finished, anything else is skipped. */
        if (!info->have_src || !info->have_dest)
            return PP_ACT_SKIP;
        return info->dest_st.st_size > 0 && info->dest_st.st_size < info->src_st.st_size
            ? PP_ACT_RESUME
            : PP_ACT_SKIP;
    default:
        break;
    }

    /* Sizes are already in hand; whether the bytes really match costs several
       round trips, so it is checked only once Resume is pressed. */
    can_continue = info->have_src && info->have_dest && info->dest_st.st_size > 0
        && info->dest_st.st_size < info->src_st.st_size;

    first = g_string_new (NULL);
    plugin_panel_describe_size (first, &info->src_st, info->have_src);
    second = g_string_new (NULL);
    plugin_panel_describe_size (second, &info->dest_st, info->have_dest);

    dlg = dlg_create (TRUE, 0, 0, button_row + 2, width, WPOS_CENTER, TRUE, alarm_colors, NULL,
                      NULL, "[Plugin file exists]", _ ("File exists"));
    g = GROUP (dlg);

    y = 1;
    /* The whole path: between two plugin panels the name is the same on both
       sides and only the path says which is which. */
    group_add_widget (g, label_new (y++, 3, plugin_panel_shown_path (info->dest_path, path_width)));
    group_add_widget (g, hline_new (y++, -1, -1));

    group_add_widget (g, label_new (y, 3, _ ("New")));
    group_add_widget (g, label_new (y++, 20, first->str));
    group_add_widget (g, label_new (y, 3, _ ("Existing")));
    group_add_widget (g, label_new (y++, 20, second->str));

    group_add_widget (g, hline_new (y++, -1, -1));

    /* The button stays live even when there is nothing to finish here: the
       answer applies to every file still to come, and a dead button could not
       be remembered. */
    group_add_widget (g,
                      label_new (y++, 3,
                                 can_continue
                                     ? _ ("Resume: check what is here and send the rest.")
                                     : _ ("Resume: nothing to finish here, this one is kept.")));

    remember = check_new (y++, 3, FALSE, _ ("&Remember this answer for the rest"));
    group_add_widget (g, remember);

    group_add_widget (g, hline_new (y++, -1, -1));

    g_assert (y == button_row);

    buttons[0] = button_new (y, 0, B_OVERWRITE, NORMAL_BUTTON, _ ("&Overwrite"), NULL);
    buttons[1] = button_new (y, 0, B_SKIP, NORMAL_BUTTON, _ ("&Skip"), NULL);
    buttons[2] = button_new (y, 0, B_RESUME, NORMAL_BUTTON, _ ("&Resume"), NULL);
    buttons[3] = button_new (y, 0, B_CANCEL, NORMAL_BUTTON, _ ("Ca&ncel"), NULL);

    for (i = 0; i < 4; i++)
        buttons_width += button_get_width (buttons[i]) + (i > 0 ? gap : 0);

    x = (width - buttons_width) / 2;
    for (i = 0; i < 4; i++)
    {
        WIDGET (buttons[i])->rect.x = x;
        x += button_get_width (buttons[i]) + gap;
        group_add_widget (g, buttons[i]);
    }

    result = dlg_run (dlg);

    /* Remembered Resume means "finish whatever is short, leave the rest", not
       "resume everything". */
    if (CHECK (remember)->state)
        switch (result)
        {
        case B_OVERWRITE:
            state->mode = PP_OVERWRITE_ALL;
            break;
        case B_SKIP:
            state->mode = PP_OVERWRITE_NONE;
            break;
        case B_RESUME:
            state->mode = PP_OVERWRITE_RESUME;
            break;
        default:
            break;
        }

    widget_destroy (WIDGET (dlg));

    g_string_free (first, TRUE);
    g_string_free (second, TRUE);

    switch (result)
    {
    case B_OVERWRITE:
        return PP_ACT_WRITE;
    case B_SKIP:
        return PP_ACT_SKIP;
    case B_RESUME:
        /* Nothing to finish here is not an error. */
        return can_continue ? PP_ACT_RESUME : PP_ACT_SKIP;
    default:
        // Cancel, and Escape
        state->mode = PP_OVERWRITE_ABORT;
        return PP_ACT_ABORT;
    }
}

/* --------------------------------------------------------------------------------------------- */

/** Ask about a local destination, if there is anything there to ask about. */
static plugin_panel_action_t
plugin_panel_local_action (const WPanel *panel, const char *src, const char *path,
                           plugin_panel_overwrite_state_t *state)
{
    plugin_panel_existing_t info;
    plugin_panel_action_t act;

    memset (&info, 0, sizeof (info));

    if (stat (path, &info.dest_st) != 0)
        return PP_ACT_WRITE;

    info.have_dest = TRUE;
    info.dest_path = g_strdup (path);
    info.src_path = plugin_panel_entry_path (panel, src);

    if (panel->plugin->stat_entry != NULL)
        info.have_src = panel->plugin->stat_entry (panel->plugin_data, src, &info.src_st);

    act = plugin_panel_ask_existing (&info, state);
    plugin_panel_existing_clear (&info);

    if (act == PP_ACT_RESUME)
    {
        gint64 from = -1;

        if (panel->plugin->resume_offset != NULL && panel->plugin->resume_copy != NULL)
            from = panel->plugin->resume_offset (panel->plugin_data, src, path, TRUE);

        if (from <= 0)
        {
            message (D_ERROR, MSG_ERROR,
                     _ ("%s\nis not the beginning of the source, so there is nothing to\n"
                        "continue. It was left alone."),
                     path);
            return PP_ACT_SKIP;
        }

        if (panel->plugin->resume_copy (panel->plugin_data, src, path, TRUE, from) != MC_PPR_OK)
            message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s"), src);

        return PP_ACT_SKIP;  // already done, the caller must not copy again
    }

    return act;
}

/* --------------------------------------------------------------------------------------------- */

/** Ask about a destination inside a plugin. A plugin that cannot say whether
    something exists is written to without asking. */
static plugin_panel_action_t
plugin_panel_plugin_action (const WPanel *dest, const char *src, const char *name,
                            plugin_panel_overwrite_state_t *state)
{
    plugin_panel_existing_t info;
    plugin_panel_action_t act;

    if (dest->plugin->exists == NULL)
        return PP_ACT_WRITE;

    if (!dest->plugin->exists (dest->plugin_data, name))
        return PP_ACT_WRITE;

    memset (&info, 0, sizeof (info));
    info.dest_path = plugin_panel_entry_path (dest, name);

    if (dest->plugin->stat_entry != NULL)
        info.have_dest = dest->plugin->stat_entry (dest->plugin_data, name, &info.dest_st);

    if (src != NULL)
    {
        info.src_path = g_strdup (src);
        info.have_src = stat (src, &info.src_st) == 0;
    }

    act = plugin_panel_ask_existing (&info, state);
    plugin_panel_existing_clear (&info);

    return act;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean plugin_panel_stream_one (const WPanel *src, const WPanel *dest, const char *fname,
                                         gint64 offset);
static gint64 plugin_panel_stream_resume_offset (const WPanel *src, const WPanel *dest,
                                                 const char *fname);

/* --------------------------------------------------------------------------------------------- */

/** Do the first @len bytes agree on both sides? -1 when nobody can say. */
static int
plugin_panel_prefix_agrees (const WPanel *src, const WPanel *dest, const char *fname, gint64 len,
                            const char *algo)
{
    char *a;
    char *b;
    int same;

    if (len == 0)
        return 1;

    a = src->plugin->digest_range (src->plugin_data, fname, 0, len, algo);
    if (a == NULL)
        return -1;

    b = dest->plugin->digest_range (dest->plugin_data, fname, 0, len, algo);
    if (b == NULL)
    {
        g_free (a);
        return -1;
    }

    same = strcmp (a, b) == 0 ? 1 : 0;
    g_free (a);
    g_free (b);

    return same;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * How much of @fname at @dest is a verified prefix of the same file at @src,
 * when neither of them is a local file.
 *
 * Both sides are asked for a digest of the bytes the destination already has,
 * in the strongest algorithm both will answer to. If those disagree, the split
 * point is bracketed: back off by a quarter at a time until a probe agrees,
 * then halve three times. Any correct prefix will do; the exact boundary is
 * not worth more round trips.
 *
 * Returns the length that is certainly right, possibly 0, or -1 when nothing
 * can be established.
 */
static gint64
plugin_panel_stream_resume_offset (const WPanel *src, const WPanel *dest, const char *fname)
{
    static const char *const algos[] = { "sha256", "md5", "cksum" };
    const char *algo = NULL;
    gint64 src_size;
    gint64 dest_size;
    gint64 good = 0;
    gint64 bad;
    gint64 probe;
    gsize i;
    int step;
    int agrees = -1;

    if (src->plugin->stat_entry == NULL || src->plugin->digest_range == NULL
        || dest->plugin->stat_entry == NULL || dest->plugin->digest_range == NULL)
        return -1;

    {
        struct stat st;

        if (!dest->plugin->stat_entry (dest->plugin_data, fname, &st))
            return -1;
        dest_size = (gint64) st.st_size;

        if (!src->plugin->stat_entry (src->plugin_data, fname, &st))
            return -1;
        src_size = (gint64) st.st_size;
    }

    if (dest_size <= 0)
        return -1;
    /* Not shorter than the source is not a partial copy: nothing here
       truncates, so an old tail would be left past the end. */
    if (src_size < 0 || dest_size >= src_size)
        return -1;

    /* Settle on the strongest algorithm both sides answer to. */
    for (i = 0; i < G_N_ELEMENTS (algos); i++)
    {
        agrees = plugin_panel_prefix_agrees (src, dest, fname, dest_size, algos[i]);
        if (agrees >= 0)
        {
            algo = algos[i];
            break;
        }
    }

    if (algo == NULL)
        return -1;

    if (agrees == 1)
        return dest_size;

    bad = dest_size;
    probe = dest_size;

    while (probe > 0)
    {
        probe = probe * 3 / 4;
        if (probe <= 0)
            break;

        agrees = plugin_panel_prefix_agrees (src, dest, fname, probe, algo);
        if (agrees < 0)
            return 0;

        if (agrees == 1)
        {
            good = probe;
            break;
        }

        bad = probe;
    }

    for (step = 0; step < 3 && bad - good > 1; step++)
    {
        gint64 mid = good + (bad - good) / 2;

        agrees = plugin_panel_prefix_agrees (src, dest, fname, mid, algo);
        if (agrees < 0)
            break;

        if (agrees == 1)
            good = mid;
        else
            bad = mid;
    }

    return good;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Decide what to do about one file going into a plugin panel. Returns the
 * action, having already carried it out in the PP_ACT_RESUME case.
 */
static plugin_panel_action_t
plugin_panel_put_decide (const WPanel *panel, const WPanel *dest, const char *fname,
                         gboolean allow_resume, plugin_panel_overwrite_state_t *state)
{
    gboolean src_is_plugin;
    char *full_path = NULL;
    plugin_panel_existing_t info;
    plugin_panel_action_t act;

    memset (&info, 0, sizeof (info));

    src_is_plugin = panel->is_plugin_panel && panel->plugin != NULL && panel->plugin_data != NULL;

    /* Only a local source has a path to hand to anybody: a plugin panel's cwd
       still names whatever directory was underneath before the plugin opened. */
    if (!src_is_plugin)
        full_path = mc_build_filename (vfs_path_as_str (panel->cwd_vpath), fname, (char *) NULL);

    if (allow_resume && dest->plugin->exists != NULL
        && dest->plugin->exists (dest->plugin_data, fname))
    {
        info.src_path = plugin_panel_entry_path (panel, fname);
        info.dest_path = plugin_panel_entry_path (dest, fname);

        if (src_is_plugin)
        {
            if (panel->plugin->stat_entry != NULL)
                info.have_src = panel->plugin->stat_entry (panel->plugin_data, fname, &info.src_st);
        }
        else
            info.have_src = stat (full_path, &info.src_st) == 0;

        if (dest->plugin->stat_entry != NULL)
            info.have_dest = dest->plugin->stat_entry (dest->plugin_data, fname, &info.dest_st);

        act = plugin_panel_ask_existing (&info, state);
    }
    else
        act = plugin_panel_plugin_action (dest, NULL, fname, state);

    if (act == PP_ACT_RESUME)
    {
        gint64 from = -1;

        if (src_is_plugin)
            from = plugin_panel_stream_resume_offset (panel, dest, fname);
        else if (dest->plugin->resume_offset != NULL && dest->plugin->resume_copy != NULL)
            from = dest->plugin->resume_offset (dest->plugin_data, full_path, fname, FALSE);

        if (from <= 0)
        {
            message (D_ERROR, MSG_ERROR,
                     _ ("%s\nis not the beginning of the source, so there is nothing to\n"
                        "continue. It was left alone."),
                     fname);
            act = PP_ACT_SKIP;
        }
        else if (src_is_plugin)
        {
            if (!plugin_panel_stream_one (panel, dest, fname, from))
                message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s to plugin"), fname);
        }
        else if (dest->plugin->resume_copy (dest->plugin_data, full_path, fname, FALSE, from)
                 != MC_PPR_OK)
            message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s to plugin"), fname);
    }

    plugin_panel_existing_clear (&info);
    g_free (full_path);

    return act;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Work out where one entry should land.
 *
 * @dest is a directory when several files are going there, and the new name of
 * the file when only one is. A relative @dest is taken against the other
 * panel's directory: the current panel is the plugin and has no local
 * directory to measure from.
 */
static char *
plugin_panel_dest_path (const char *dest, const char *name, gboolean only_one)
{
    char *base;
    char *dir;
    char *result;

    if (g_path_is_absolute (dest))
        dir = g_strdup (dest);
    else
        dir = mc_build_filename (vfs_path_as_str (other_panel->cwd_vpath), dest, (char *) NULL);

    /* A single file goes to exactly the name given, unless that name is a
       directory that already exists or was written with a trailing slash. */
    if (only_one && !g_str_has_suffix (dest, PATH_SEP_STR))
    {
        struct stat st;

        if (stat (dir, &st) != 0 || !S_ISDIR (st.st_mode))
            return dir;
    }

    base = g_path_get_basename (name);
    result = mc_build_filename (dir, base, (char *) NULL);
    g_free (base);
    g_free (dir);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static void
plugin_panel_item_free (gpointer data)
{
    plugin_panel_item_t *item = (plugin_panel_item_t *) data;

    g_free (item->name);
    g_free (item);
}

/* --------------------------------------------------------------------------------------------- */

static void
plugin_panel_item_add (GPtrArray *items, const file_entry_t *fe)
{
    plugin_panel_item_t *item;

    item = g_new (plugin_panel_item_t, 1);
    item->name = g_strdup (fe->fname->str);
    item->mode = fe->st.st_mode;
    g_ptr_array_add (items, item);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Collect the marked items, or the one under the cursor when none are marked.
 * The mode comes along: without it a directory cannot be told from a file, and
 * asking a plugin for a local copy of a directory yields an empty file.
 *
 * @return newly allocated GPtrArray of plugin_panel_item_t (caller frees with g_ptr_array_free)
 */

static GPtrArray *
plugin_panel_collect_items (WPanel *panel, gboolean single)
{
    GPtrArray *items;

    items = g_ptr_array_new_with_free_func (plugin_panel_item_free);

    /* The Single variants of Copy/Move/Delete act on the item under the cursor
       whatever is marked. */
    if (!single && panel->marked > 0)
    {
        int i;

        for (i = 0; i < panel->dir.len; i++)
            if (panel->dir.list[i].f.marked != 0)
                plugin_panel_item_add (items, &panel->dir.list[i]);
    }
    else
    {
        const file_entry_t *fe = panel_current_entry (panel);

        if (fe != NULL)
            plugin_panel_item_add (items, fe);
    }

    return items;
}

/* --------------------------------------------------------------------------------------------- */

/** How deep a plugin tree is followed. A listing that points back at itself
    would otherwise never end. */
#define PP_COPY_MAX_DEPTH 32

/** Copy one item out of the plugin to @dest_path. Files go through the plugin
    callbacks; a directory is unpacked by the plugin if it can, and walked here
    if it cannot. */
static mc_pp_result_t plugin_panel_copy_item (WPanel *panel, const char *name, mode_t mode,
                                              const char *dest_path, int depth);

/* --------------------------------------------------------------------------------------------- */

/** Copy a single file out of the plugin to @dest_path. */
static mc_pp_result_t
plugin_panel_copy_file (WPanel *panel, const char *name, const char *dest_path)
{
    mc_pp_result_t r = MC_PPR_NOT_SUPPORTED;

    if (panel->plugin->copy_to_local != NULL)
        r = panel->plugin->copy_to_local (panel->plugin_data, name, dest_path);

    /* An item the plugin will not take is left to the local copy path. */
    if (r == MC_PPR_NOT_SUPPORTED && panel->plugin->get_local_copy != NULL)
    {
        char *local_path = NULL;

        r = panel->plugin->get_local_copy (panel->plugin_data, name, &local_path);
        if (r == MC_PPR_OK && local_path != NULL)
        {
            if (!copy_local_file (local_path, dest_path))
                r = MC_PPR_FAILED;
            unlink (local_path);
        }
        else if (r == MC_PPR_OK)
            r = MC_PPR_FAILED;

        g_free (local_path);
    }

    return r;
}

/* --------------------------------------------------------------------------------------------- */

/** Walk a directory of the plugin and copy what is in it. The plugin is left
    standing where it stood. */
static mc_pp_result_t
plugin_panel_walk_dir (WPanel *panel, const char *name, const char *dest_path, int depth)
{
    dir_list list;
    mc_pp_result_t result = MC_PPR_OK;
    int i;

    if (depth >= PP_COPY_MAX_DEPTH)
        return MC_PPR_FAILED;

    if (panel->plugin->chdir == NULL || panel->plugin->get_items == NULL
        || (panel->plugin->flags & MC_PPF_NAVIGATE) == 0)
        return MC_PPR_NOT_SUPPORTED;

    if (panel->plugin->chdir (panel->plugin_data, name) != MC_PPR_OK)
        return MC_PPR_FAILED;

    if (!dir_list_init (&list))
    {
        panel->plugin->chdir (panel->plugin_data, "..");
        return MC_PPR_FAILED;
    }

    if (panel->plugin->get_items (panel->plugin_data, &list) != MC_PPR_OK)
        result = MC_PPR_FAILED;

    for (i = 0; i < list.len && result == MC_PPR_OK; i++)
    {
        const file_entry_t *fe = &list.list[i];
        const char *child = fe->fname->str;
        char *child_dest;

        /* The host puts ".." at the top; a name with a separator in it would
           write outside the destination. */
        if (DIR_IS_DOT (child) || DIR_IS_DOTDOT (child) || strchr (child, PATH_SEP) != NULL)
            continue;

        child_dest = mc_build_filename (dest_path, child, (char *) NULL);
        result = plugin_panel_copy_item (panel, child, fe->st.st_mode, child_dest, depth + 1);
        g_free (child_dest);
    }

    dir_list_free_list (&list);

    if (panel->plugin->chdir (panel->plugin_data, "..") != MC_PPR_OK)
        result = MC_PPR_FAILED;

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
plugin_panel_copy_item (WPanel *panel, const char *name, mode_t mode, const char *dest_path,
                        int depth)
{
    mc_pp_result_t r;

    if (!S_ISDIR (mode))
    {
        r = plugin_panel_copy_file (panel, name, dest_path);

        /* The listing knows the mode; a temp file on the way here does not. */
        if (r == MC_PPR_OK && (mode & 0777) != 0)
            chmod (dest_path, mode & 0777);

        return r;
    }

    /* A plugin that can hand over a whole subtree does it in one go. */
    if (panel->plugin->copy_to_local != NULL)
    {
        r = panel->plugin->copy_to_local (panel->plugin_data, name, dest_path);
        if (r != MC_PPR_NOT_SUPPORTED)
            return r;
    }

    if (g_mkdir_with_parents (dest_path, 0755) != 0)
        return MC_PPR_FAILED;

    r = plugin_panel_walk_dir (panel, name, dest_path, depth);

    /* The mode goes on last: a directory that keeps us out cannot be filled. */
    if (r == MC_PPR_OK && (mode & 0777) != 0)
        chmod (dest_path, mode & 0777);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
plugin_panel_copy_cmd (WPanel *panel, gboolean single)
{
    char *dest_dir;
    const char *default_dest;
    GPtrArray *items;
    plugin_panel_overwrite_state_t overwrite = { PP_OVERWRITE_ASK };
    guint i;

    if (panel->plugin == NULL || panel->plugin_data == NULL)
        return;

    if (panel->plugin->copy_to_local == NULL && panel->plugin->get_local_copy == NULL)
    {
        message (D_ERROR, MSG_ERROR, _ ("This plugin does not support copying files"));
        return;
    }

    default_dest = vfs_path_as_str (other_panel->cwd_vpath);
    dest_dir = input_expand_dialog (_ ("Copy"), _ ("Copy to:"), MC_HISTORY_FM_PLUGIN_COPY,
                                    default_dest, INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);
    if (dest_dir == NULL || dest_dir[0] == '\0')
    {
        g_free (dest_dir);
        return;
    }

    items = plugin_panel_collect_items (panel, single);

    for (i = 0; i < items->len; i++)
    {
        const plugin_panel_item_t *item =
            (const plugin_panel_item_t *) g_ptr_array_index (items, i);
        const char *name = item->name;
        char *dest_path;
        plugin_panel_action_t act;
        mc_pp_result_t r;

        if (overwrite.mode == PP_OVERWRITE_ABORT)
            break;

        /* A relative destination means "inside this panel", which for a plugin
           is the plugin itself. Only an absolute one leaves. */
        if (!g_path_is_absolute (dest_dir) && panel->plugin->copy_within != NULL)
        {
            char *dest_name;

            dest_name = (items->len == 1) ? g_strdup (dest_dir)
                                          : mc_build_filename (dest_dir, name, (char *) NULL);

            /* Both ends are inside the plugin, so there is no local file to
               measure a resume against. */
            if (plugin_panel_plugin_action (panel, NULL, dest_name, &overwrite) == PP_ACT_WRITE)
            {
                r = panel->plugin->copy_within (panel->plugin_data, name, dest_name);
                if (r != MC_PPR_OK)
                    message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s"), name);
            }

            g_free (dest_name);
            continue;
        }

        dest_path = plugin_panel_dest_path (dest_dir, name, items->len == 1);

        act = plugin_panel_local_action (panel, name, dest_path, &overwrite);

        if (act == PP_ACT_RESUME)
        {
            gint64 from;

            from = panel->plugin->resume_offset (panel->plugin_data, name, dest_path, TRUE);
            r = panel->plugin->resume_copy (panel->plugin_data, name, dest_path, TRUE, from);
            if (r != MC_PPR_OK)
                message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s"), name);

            g_free (dest_path);
            continue;
        }

        if (act != PP_ACT_WRITE)
        {
            g_free (dest_path);
            continue;
        }

        r = plugin_panel_copy_item (panel, name, item->mode, dest_path, 0);
        if (r != MC_PPR_OK && r != MC_PPR_SKIPPED)
            message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s"), name);

        g_free (dest_path);
    }

    g_ptr_array_free (items, TRUE);
    g_free (dest_dir);
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */

void
plugin_panel_delete_cmd (WPanel *panel, gboolean single)
{
    GPtrArray *items, *names;
    mc_pp_result_t r;
    guint i;

    if (panel->plugin == NULL || panel->plugin_data == NULL)
        return;

    if (panel->plugin->delete_items == NULL)
    {
        message (D_ERROR, MSG_ERROR, _ ("This plugin does not support deleting files"));
        return;
    }

    items = plugin_panel_collect_items (panel, single);

    if (items->len == 0)
    {
        g_ptr_array_free (items, TRUE);
        return;
    }

    names = g_ptr_array_new ();
    for (i = 0; i < items->len; i++)
        g_ptr_array_add (names, ((plugin_panel_item_t *) g_ptr_array_index (items, i))->name);

    /* Confirmation */
    if (confirm_delete)
    {
        int result;

        if (names->len == 1)
            result = query_dialog (_ ("Delete"), _ ("Delete file from plugin panel?"), D_ERROR, 2,
                                   _ ("&Yes"), _ ("&No"));
        else
            result = query_dialog (_ ("Delete"), _ ("Delete tagged files from plugin panel?"),
                                   D_ERROR, 2, _ ("&Yes"), _ ("&No"));

        if (result != 0)
        {
            g_ptr_array_free (names, TRUE);
            g_ptr_array_free (items, TRUE);
            return;
        }
    }

    r = panel->plugin->delete_items (panel->plugin_data, (const char **) names->pdata,
                                     (int) names->len);
    if (r != MC_PPR_OK)
        message (D_ERROR, MSG_ERROR, _ ("Delete failed"));

    g_ptr_array_free (names, TRUE);
    g_ptr_array_free (items, TRUE);
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */

void
plugin_panel_create_cmd (WPanel *panel)
{
    mc_pp_result_t r;

    if (panel->plugin == NULL || panel->plugin_data == NULL)
        return;

    if (panel->plugin->create_item == NULL || (panel->plugin->flags & MC_PPF_CREATE) == 0)
    {
        message (D_ERROR, MSG_ERROR, _ ("This plugin does not support creating items"));
        return;
    }

    r = panel->plugin->create_item (panel->plugin_data);
    if (r == MC_PPR_OK)
    {
        char *focus = NULL;

        /* mc's own mkdir names the new directory to update_panels() so the
           cursor lands on it. A plugin cannot reach that argument, so it leaves
           the name here instead. */
        if (panel->plugin_host != NULL)
        {
            focus = panel->plugin_host->focus_after;
            panel->plugin_host->focus_after = NULL;
        }

        update_panels (UP_OPTIMIZE, focus != NULL ? focus : UP_KEEPSEL);
        g_free (focus);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Shift-F4 on a plugin panel. The name is asked for up front: the destination
 * is not a filesystem the editor can reach, so an untitled buffer would have
 * nowhere to be saved to.
 */
void
plugin_panel_edit_new_cmd (WPanel *panel)
{
    char *name;
    char *local_path;
    const char *base;
    const char *ext;
    vfs_path_t *tmp_vpath = NULL;
    vfs_path_t *local_vpath;
    int fd;

    if (panel->plugin == NULL || panel->plugin_data == NULL)
        return;

    if ((panel->plugin->flags & MC_PPF_PUT_FILES) == 0
        || (panel->plugin->put_file == NULL && panel->plugin->save_file == NULL))
    {
        message (D_ERROR, MSG_ERROR, _ ("This plugin does not support creating files"));
        return;
    }

    /* Plain input, not the expanding one: the name goes to a plugin, so a
       tilde or a $ in it is a character and must not be resolved here. */
    name = input_dialog (_ ("Edit file"), _ ("Enter file name:"), MC_HISTORY_EDIT_LOAD, "",
                         INPUT_COMPLETE_NONE);
    if (name == NULL || name[0] == '\0')
    {
        g_free (name);
        return;
    }

    /* Give the temporary file the extension the user just typed: syntax
       highlighting and the mc.ext rules go by the name. */
    base = strrchr (name, PATH_SEP);
    base = base != NULL ? base + 1 : name;
    ext = strrchr (base, '.');

    fd = mc_mkstemps (&tmp_vpath, "mcedit", ext);
    if (fd < 0)
    {
        g_free (name);
        return;
    }
    close (fd);

    local_path = g_strdup (vfs_path_as_str (tmp_vpath));
    vfs_path_free (tmp_vpath, TRUE);

    /* The editor is handed a name with nothing behind it, so that whether the
       file exists afterwards answers "did the user save?". */
    unlink (local_path);

    local_vpath = vfs_path_from_str (local_path);
    edit_file_at_line (local_vpath, use_internal_edit, 0);
    vfs_path_free (local_vpath, TRUE);

    if (g_file_test (local_path, G_FILE_TEST_EXISTS))
    {
        mc_pp_result_t r;

        if (panel->plugin->put_file != NULL)
            r = panel->plugin->put_file (panel->plugin_data, local_path, name);
        else
            r = panel->plugin->save_file (panel->plugin_data, local_path, name);

        /* MC_PPR_NOT_SUPPORTED depends on where in the plugin the panel
           stands, not on the plugin as a whole. */
        if (r == MC_PPR_NOT_SUPPORTED)
            message (D_ERROR, MSG_ERROR, _ ("This plugin does not support creating files"));
        else if (r != MC_PPR_OK)
            message (D_ERROR, MSG_ERROR, _ ("Cannot save %s back to plugin"), name);
        else
            update_panels (UP_OPTIMIZE, name);

        unlink (local_path);
    }

    g_free (local_path);
    g_free (name);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
plugin_panel_confirm_put (const WPanel *panel, gboolean move_op)
{
    int result;
    const char *title = move_op ? _ ("Move") : _ ("Copy");

    if (panel->marked <= 0)
        result = query_dialog (
            title, move_op ? _ ("Move file to plugin panel?") : _ ("Copy file to plugin panel?"),
            D_NORMAL, 2, _ ("&Yes"), _ ("&No"));
    else
        result = query_dialog (title,
                               move_op ? _ ("Move tagged files to plugin panel?")
                                       : _ ("Copy tagged files to plugin panel?"),
                               D_NORMAL, 2, _ ("&Yes"), _ ("&No"));

    return (result == 0);
}

/* --------------------------------------------------------------------------------------------- */

void
plugin_panel_move_cmd (WPanel *panel, gboolean single)
{
    char *dest_dir;
    const char *default_dest;
    GPtrArray *items;
    GPtrArray *moved_names;
    plugin_panel_overwrite_state_t overwrite = { PP_OVERWRITE_ASK };
    guint i;

    if (panel->plugin == NULL || panel->plugin_data == NULL)
        return;

    /* Moving deletes the source once the copy is believed to have arrived; a
       plugin that cannot vouch for that sets MC_PPF_NO_MOVE. */
    if ((panel->plugin->flags & MC_PPF_NO_MOVE) != 0
        || (panel->plugin->copy_to_local == NULL && panel->plugin->get_local_copy == NULL)
        || panel->plugin->delete_items == NULL)
    {
        message (D_ERROR, MSG_ERROR, _ ("This plugin does not support moving files"));
        return;
    }

    default_dest = vfs_path_as_str (other_panel->cwd_vpath);
    dest_dir = input_expand_dialog (_ ("Move"), _ ("Move to:"), MC_HISTORY_FM_PLUGIN_MOVE,
                                    default_dest, INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);
    if (dest_dir == NULL || dest_dir[0] == '\0')
    {
        g_free (dest_dir);
        return;
    }

    items = plugin_panel_collect_items (panel, single);
    moved_names = g_ptr_array_new_with_free_func (g_free);

    for (i = 0; i < items->len; i++)
    {
        const plugin_panel_item_t *item =
            (const plugin_panel_item_t *) g_ptr_array_index (items, i);
        const char *name = item->name;
        char *dest_path;
        mc_pp_result_t r;
        gboolean ok = FALSE;

        if (overwrite.mode == PP_OVERWRITE_ABORT)
            break;

        dest_path = plugin_panel_dest_path (dest_dir, name, items->len == 1);

        /* No resume: the source is deleted afterwards, and a continued
           transfer is verified only up to the join. */
        if (plugin_panel_local_action (panel, name, dest_path, &overwrite) != PP_ACT_WRITE)
        {
            g_free (dest_path);
            continue;
        }

        r = plugin_panel_copy_item (panel, name, item->mode, dest_path, 0);
        if (r == MC_PPR_OK)
            ok = TRUE;
        else if (r != MC_PPR_SKIPPED)
            message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s"), name);

        g_free (dest_path);

        if (ok)
            g_ptr_array_add (moved_names, g_strdup (name));
    }

    /* Delete successfully moved files from the plugin */
    if (moved_names->len > 0)
    {
        mc_pp_result_t r;

        r = panel->plugin->delete_items (panel->plugin_data, (const char **) moved_names->pdata,
                                         (int) moved_names->len);
        if (r != MC_PPR_OK)
            message (D_ERROR, MSG_ERROR, _ ("Move succeeded but delete from plugin failed"));
    }

    g_ptr_array_free (items, TRUE);
    g_ptr_array_free (moved_names, TRUE);
    g_free (dest_dir);
    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Move one file straight from one plugin panel into another.
 *
 * put_file() cannot do this: it takes a local pathname, so a plugin source
 * would have to be spilled to disk first.
 *
 * When both sides report a checksum they are compared. Neither side is
 * required to produce one, and its absence means the copy is unconfirmed, not
 * that it failed.
 */
static gboolean
plugin_panel_stream_one (const WPanel *src, const WPanel *dest, const char *fname, gint64 offset)
{
    void *rh, *wh;
    gint64 size = -1;
    char buf[BUF_8K];
    char *src_digest = NULL;
    char *dst_digest = NULL;
    gboolean ok = TRUE;

    rh = src->plugin->read_open (src->plugin_data, fname, offset, &size);
    if (rh == NULL)
        return FALSE;

    /* read_open reports what is left from the offset; the destination needs to
       know how long the file will end up being. */
    wh = dest->plugin->write_open (dest->plugin_data, fname, size + offset, offset);
    if (wh == NULL)
    {
        (void) src->plugin->read_close (src->plugin_data, rh, NULL);
        return FALSE;
    }

    while (TRUE)
    {
        gssize n;

        n = src->plugin->read_chunk (src->plugin_data, rh, buf, sizeof (buf));
        if (n < 0)
        {
            ok = FALSE;
            break;
        }
        if (n == 0)
            break;

        if (!dest->plugin->write_chunk (dest->plugin_data, wh, buf, (gsize) n))
        {
            ok = FALSE;
            break;
        }
    }

    if (!src->plugin->read_close (src->plugin_data, rh, &src_digest))
        ok = FALSE;
    if (!dest->plugin->write_close (dest->plugin_data, wh, &dst_digest))
        ok = FALSE;

    if (ok && src_digest != NULL && dst_digest != NULL && strcmp (src_digest, dst_digest) != 0)
    {
        message (D_ERROR, MSG_ERROR, _ ("%s: the copy does not match the source"), fname);
        ok = FALSE;
    }

    g_free (src_digest);
    g_free (dst_digest);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

/** TRUE when the file can go across without touching local disk. */
static gboolean
plugin_panel_can_stream (const WPanel *src, const WPanel *dest)
{
    return (src->is_plugin_panel && src->plugin != NULL && src->plugin_data != NULL
            && src->plugin->read_open != NULL && src->plugin->read_chunk != NULL
            && src->plugin->read_close != NULL && dest->plugin->write_open != NULL
            && dest->plugin->write_chunk != NULL && dest->plugin->write_close != NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */

void
plugin_panel_put_cmd (WPanel *panel)
{
    const WPanel *dest;
    plugin_panel_overwrite_state_t overwrite = { PP_OVERWRITE_ASK };
    int i;

    dest = other_panel;

    if (!dest->is_plugin_panel || dest->plugin == NULL || dest->plugin_data == NULL)
        return;

    if (dest->plugin->put_file == NULL)
    {
        message (D_ERROR, MSG_ERROR, _ ("This plugin does not support receiving files"));
        return;
    }

    /* validate before confirmation: single-file case may have nothing to copy */
    if (panel->marked == 0)
    {
        const file_entry_t *fe;

        fe = panel_current_entry (panel);
        if (fe == NULL
            || (S_ISDIR (fe->st.st_mode) && (dest->plugin->flags & MC_PPF_LOCAL_FILES) == 0))
            return;
    }

    if (!plugin_panel_confirm_put (panel, FALSE))
        return;

    if (panel->marked > 0)
    {
        for (i = 0; i < panel->dir.len; i++)
        {
            const file_entry_t *fe = &panel->dir.list[i];
            char *full_path;
            mc_pp_result_t r;

            if (!fe->f.marked)
                continue;

            if (overwrite.mode == PP_OVERWRITE_ABORT)
                break;

            if (S_ISDIR (fe->st.st_mode) && (dest->plugin->flags & MC_PPF_LOCAL_FILES) == 0)
                continue;

            if (plugin_panel_put_decide (panel, dest, fe->fname->str, TRUE, &overwrite)
                != PP_ACT_WRITE)
                continue;

            if (plugin_panel_can_stream (panel, dest))
            {
                if (!plugin_panel_stream_one (panel, dest, fe->fname->str, 0))
                    message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s to plugin"), fe->fname->str);
                continue;
            }

            full_path = mc_build_filename (vfs_path_as_str (panel->cwd_vpath), fe->fname->str,
                                           (char *) NULL);
            r = dest->plugin->put_file (dest->plugin_data, full_path, fe->fname->str);
            if (r != MC_PPR_OK)
                message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s to plugin"), fe->fname->str);

            g_free (full_path);
        }
    }
    else
    {
        const file_entry_t *fe;
        char *full_path;
        mc_pp_result_t r;

        fe = panel_current_entry (panel);
        /* already validated above, but keep the guard */
        if (fe == NULL
            || (S_ISDIR (fe->st.st_mode) && (dest->plugin->flags & MC_PPF_LOCAL_FILES) == 0))
            return;

        if (plugin_panel_put_decide (panel, dest, fe->fname->str, TRUE, &overwrite) != PP_ACT_WRITE)
        {
            update_panels (UP_OPTIMIZE, UP_KEEPSEL);
            return;
        }

        if (plugin_panel_can_stream (panel, dest))
        {
            /* Must not return from here: the panels are refreshed at the end
               of the function. */
            if (!plugin_panel_stream_one (panel, dest, fe->fname->str, 0))
                message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s to plugin"), fe->fname->str);
        }
        else
        {
            full_path = mc_build_filename (vfs_path_as_str (panel->cwd_vpath), fe->fname->str,
                                           (char *) NULL);
            r = dest->plugin->put_file (dest->plugin_data, full_path, fe->fname->str);
            if (r != MC_PPR_OK)
                message (D_ERROR, MSG_ERROR, _ ("Cannot copy %s to plugin"), fe->fname->str);

            g_free (full_path);
        }
    }

    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */

void
plugin_panel_put_move_cmd (WPanel *panel)
{
    const WPanel *dest;
    plugin_panel_overwrite_state_t overwrite = { PP_OVERWRITE_ASK };
    int i;

    dest = other_panel;

    if (!dest->is_plugin_panel || dest->plugin == NULL || dest->plugin_data == NULL)
        return;

    if (dest->plugin->put_file == NULL)
    {
        message (D_ERROR, MSG_ERROR, _ ("This plugin does not support receiving files"));
        return;
    }

    /* validate before confirmation: single-file case may have nothing to move */
    if (panel->marked == 0)
    {
        const file_entry_t *fe;

        fe = panel_current_entry (panel);
        if (fe == NULL
            || (S_ISDIR (fe->st.st_mode) && (dest->plugin->flags & MC_PPF_LOCAL_FILES) == 0))
            return;
    }

    if (!plugin_panel_confirm_put (panel, TRUE))
        return;

    if (panel->marked > 0)
    {
        for (i = 0; i < panel->dir.len; i++)
        {
            const file_entry_t *fe = &panel->dir.list[i];
            char *full_path;
            mc_pp_result_t r;

            if (!fe->f.marked)
                continue;

            if (overwrite.mode == PP_OVERWRITE_ABORT)
                break;

            if (S_ISDIR (fe->st.st_mode) && (dest->plugin->flags & MC_PPF_LOCAL_FILES) == 0)
                continue;

            /* No resume: the source is deleted once this is believed to have
               arrived, so only a complete arrival will do. */
            if (plugin_panel_put_decide (panel, dest, fe->fname->str, FALSE, &overwrite)
                != PP_ACT_WRITE)
                continue;

            full_path = mc_build_filename (vfs_path_as_str (panel->cwd_vpath), fe->fname->str,
                                           (char *) NULL);
            r = dest->plugin->put_file (dest->plugin_data, full_path, fe->fname->str);
            if (r != MC_PPR_OK)
            {
                message (D_ERROR, MSG_ERROR, _ ("Cannot move %s to plugin"), fe->fname->str);
                g_free (full_path);
                continue;
            }

            if ((dest->plugin->flags & MC_PPF_LOCAL_FILES) == 0 && unlink (full_path) != 0)
                message (D_ERROR, MSG_ERROR, _ ("Cannot delete local file %s"), fe->fname->str);

            g_free (full_path);
        }
    }
    else
    {
        const file_entry_t *fe;
        char *full_path;
        mc_pp_result_t r;

        fe = panel_current_entry (panel);
        /* already validated above, but keep the guard */
        if (fe == NULL
            || (S_ISDIR (fe->st.st_mode) && (dest->plugin->flags & MC_PPF_LOCAL_FILES) == 0))
            return;

        if (plugin_panel_put_decide (panel, dest, fe->fname->str, FALSE, &overwrite)
            != PP_ACT_WRITE)
            return;

        full_path =
            mc_build_filename (vfs_path_as_str (panel->cwd_vpath), fe->fname->str, (char *) NULL);
        r = dest->plugin->put_file (dest->plugin_data, full_path, fe->fname->str);
        if (r != MC_PPR_OK)
        {
            message (D_ERROR, MSG_ERROR, _ ("Cannot move %s to plugin"), fe->fname->str);
            g_free (full_path);
            return;
        }

        if ((dest->plugin->flags & MC_PPF_LOCAL_FILES) == 0 && unlink (full_path) != 0)
            message (D_ERROR, MSG_ERROR, _ ("Cannot delete local file %s"), fe->fname->str);

        g_free (full_path);
    }

    update_panels (UP_OPTIMIZE, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */
