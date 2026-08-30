/*
   mcstruct: saved byte fragments, Shift-F2 / Ctrl-F2

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

/* A fragment is the bytes of a row or a hex block, kept under a number that
   is never reused, with the structure, the file and the time they came from.
   The set lives in the cache, so every mcstruct sees the same fragments. */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "mcstruct_ui_priv.h"

#include "lib/mcconfig.h"

/*** file scope macro definitions ****************************************************************/

#define STORE_GROUP    "store"
#define MAX_LIST_LINES 16

/*** file scope type declarations ****************************************************************/

typedef struct
{
    int id;
    char *name;
    char *sname;
    off_t size;
    char *source;
    char *time;
    gint64 stamp;
} frag_t;

/*** file scope variables ************************************************************************/

/* the list item that stands for "a file" */
static const char from_file_item = 0;

/*** file scope functions ************************************************************************/

static char *
store_dir (void)
{
    return g_build_filename (mc_config_get_cache_path (), "mcstruct", "bytes", (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static char *
bin_path (int id)
{
    char *dir = store_dir ();
    char *name = g_strdup_printf ("%d.bin", id);
    char *path = g_build_filename (dir, name, (char *) NULL);

    g_free (name);
    g_free (dir);
    return path;
}

/* --------------------------------------------------------------------------------------------- */

static mc_config_t *
store_open (void)
{
    char *dir = store_dir ();
    char *path;
    mc_config_t *cfg;

    g_mkdir_with_parents (dir, 0700);
    path = g_build_filename (dir, "index.ini", (char *) NULL);
    cfg = mc_config_init (path, FALSE);
    g_free (path);
    g_free (dir);
    return cfg;
}

/* --------------------------------------------------------------------------------------------- */

static void
frag_free (gpointer data)
{
    frag_t *f = data;

    g_free (f->name);
    g_free (f->sname);
    g_free (f->source);
    g_free (f->time);
    g_free (f);
}

/* --------------------------------------------------------------------------------------------- */

static gint
frag_cmp (gconstpointer a, gconstpointer b)
{
    return (*(const frag_t *const *) b)->id - (*(const frag_t *const *) a)->id;
}

/* --------------------------------------------------------------------------------------------- */

static GPtrArray *
store_list (mc_config_t *cfg)
{
    GPtrArray *list = g_ptr_array_new_with_free_func (frag_free);
    gsize n, i;
    gchar **groups = mc_config_get_groups (cfg, &n);

    for (i = 0; i < n; i++)
    {
        frag_t *f;
        char *end;
        long id = strtol (groups[i], &end, 10);

        if (*end != '\0' || id <= 0)
            continue;
        f = g_new0 (frag_t, 1);
        f->id = (int) id;
        f->name = mc_config_get_string (cfg, groups[i], "name", "");
        f->sname = mc_config_get_string (cfg, groups[i], "struct", "");
        f->size = mc_config_get_int (cfg, groups[i], "size", 0);
        f->source = mc_config_get_string (cfg, groups[i], "source", "");
        f->time = mc_config_get_string (cfg, groups[i], "time", "");
        f->stamp = mc_config_get_int (cfg, groups[i], "stamp", 0);
        g_ptr_array_add (list, f);
    }
    g_strfreev (groups);
    g_ptr_array_sort (list, frag_cmp);
    return list;
}

/* --------------------------------------------------------------------------------------------- */

static void
store_delete (mc_config_t *cfg, int id)
{
    char *path = bin_path (id);
    char *group = g_strdup_printf ("%d", id);

    g_unlink (path);
    mc_config_del_group (cfg, group);
    g_free (group);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

static void
store_save (mc_config_t *cfg, const char *what)
{
    GError *error = NULL;

    if (!mc_config_save_file (cfg, &error))
    {
        message (D_ERROR, what, "%s", error->message);
        g_error_free (error);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* fragments older than the setting are gone; @list is trimmed with the store */
static void
store_expire (mc_config_t *cfg, GPtrArray *list, int days)
{
    gint64 limit;
    guint i;
    gboolean changed = FALSE;

    if (days <= 0)
        return;
    limit = g_get_real_time () / G_USEC_PER_SEC - (gint64) days * 86400;
    for (i = 0; i < list->len;)
    {
        const frag_t *f = g_ptr_array_index (list, i);

        if (f->stamp >= limit)
            i++;
        else
        {
            store_delete (cfg, f->id);
            g_ptr_array_remove_index (list, i);
            changed = TRUE;
        }
    }
    if (changed)
        store_save (cfg, _ ("Read bytes"));
}

/* --------------------------------------------------------------------------------------------- */

static unsigned char *
read_range (ui_t *ui, const char *what, off_t offset, off_t size)
{
    unsigned char *buf = g_malloc ((gsize) size);

    if (ui->fr->reader.read (ui->fr->reader.ctx, offset, buf, (gsize) size) == (gssize) size)
        return buf;
    message (D_ERROR, what, _ ("Cannot read the bytes"));
    g_free (buf);
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
write_file (const char *what, const char *path, const unsigned char *buf, off_t size)
{
    GError *error = NULL;

    if (!g_file_set_contents (path, (const char *) buf, (gssize) size, &error))
    {
        message (D_ERROR, what, "%s", error->message);
        g_error_free (error);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* the old way: any file the user names */
static void
put_to_file (ui_t *ui, const char *label, off_t offset, off_t size)
{
    char *path;
    unsigned char *buf;

    path = input_expand_dialog (_ ("Write bytes"), label, "mcstruct-bytes", "",
                                INPUT_COMPLETE_FILENAMES);
    if (path == NULL || path[0] == '\0')
    {
        g_free (path);
        return;
    }
    if (g_file_test (path, G_FILE_TEST_EXISTS)
        && query_dialog (_ ("Write bytes"), _ ("The file exists. Overwrite?"), D_NORMAL, 2,
                         _ ("&Yes"), _ ("&No"))
            != 0)
    {
        g_free (path);
        return;
    }
    buf = read_range (ui, _ ("Write bytes"), offset, size);
    if (buf != NULL)
        write_file (_ ("Write bytes"), path, buf, size);
    g_free (buf);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

/* @data (@len bytes) over the target range; the target keeps its size */
static void
apply_bytes (ui_t *ui, off_t offset, off_t size, const unsigned char *data, gsize len)
{
    unsigned char *orig;
    gsize i;

    if (len > (gsize) size)
    {
        if (query_dialog (_ ("Read bytes"),
                          _ ("The fragment is longer than the target. Use the first bytes only?"),
                          D_NORMAL, 2, _ ("&Yes"), _ ("&No"))
            != 0)
            return;
        len = (gsize) size;
    }
    orig = g_malloc (len > 0 ? len : 1);
    if (len > 0 && ui->fr->reader.read (ui->fr->reader.ctx, offset, orig, len) != (gssize) len)
    {
        message (D_ERROR, _ ("Read bytes"), _ ("Cannot read the bytes"));
        g_free (orig);
        return;
    }
    ui_note_change (ui);
    for (i = 0; i < len; i++)
        if (data[i] != orig[i])
            slv_file_reader_set_byte (ui->fr, offset + (off_t) i, data[i]);
    g_free (orig);
    ui_refresh (ui);
    widget_draw (WIDGET (ui->dlg));
    if (len < (gsize) size)
        message (D_NORMAL, _ ("Read bytes"), _ ("%u bytes read, the rest is unchanged"),
                 (unsigned) len);
}

/* --------------------------------------------------------------------------------------------- */

static void
get_from_path (ui_t *ui, const char *path, off_t offset, off_t size)
{
    char *data = NULL;
    gsize len = 0;
    GError *error = NULL;

    if (!g_file_get_contents (path, &data, &len, &error))
    {
        message (D_ERROR, _ ("Read bytes"), "%s", error->message);
        g_error_free (error);
        return;
    }
    apply_bytes (ui, offset, size, (const unsigned char *) data, len);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */

static void
get_from_file (ui_t *ui, const char *label, off_t offset, off_t size)
{
    char *path;

    path = input_expand_dialog (_ ("Read bytes"), label, "mcstruct-bytes", "",
                                INPUT_COMPLETE_FILENAMES);
    if (path != NULL && path[0] != '\0')
        get_from_path (ui, path, offset, size);
    g_free (path);
}

/* --------------------------------------------------------------------------------------------- */

/* fragments that were in @list and are not in the listbox any more are deleted from the store */
static void
delete_removed (mc_config_t *cfg, WListbox *lb, GPtrArray *list)
{
    guint i;
    gboolean changed = FALSE;

    for (i = 0; i < list->len; i++)
    {
        const frag_t *f = g_ptr_array_index (list, i);

        if (listbox_search_data (lb, f) < 0)
        {
            store_delete (cfg, f->id);
            changed = TRUE;
        }
    }
    if (changed)
        store_save (cfg, _ ("Read bytes"));
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* Shift-F2: the bytes of the current row or block into the store under the next number, or
   into a file the user names */
void
ui_cmd_put_bytes (ui_t *ui)
{
    off_t offset, size;
    char *label, *name, *sname, *title, *fname = NULL;
    mc_config_t *cfg;
    int id, rc;

    if (!ui_bytes_range (ui, _ ("Write bytes"), &offset, &size, &name, &sname))
        return;
    cfg = store_open ();
    {
        GPtrArray *list = store_list (cfg);

        store_expire (cfg, list, ui->settings.fragment_days);
        g_ptr_array_free (list, TRUE);
    }
    id = mc_config_get_int (cfg, STORE_GROUP, "next", 1);
    label = g_strdup_printf (_ ("%s: %lld bytes at %08llX"), name, (long long) size,
                             (unsigned long long) offset);
    title = g_strdup_printf (_ ("Save bytes #%d"), id);
    g_free (name);

    {
        /* clang-format off */
        quick_widget_t quick_widgets[] = {
            QUICK_LABEL (label, NULL),
            QUICK_LABELED_INPUT (N_ ("Name:"), input_label_above, sname, "mcstruct-bytes-name",
                                 &fname, NULL, FALSE, FALSE, INPUT_COMPLETE_NONE),
            QUICK_START_BUTTONS (TRUE, TRUE),
                QUICK_BUTTON (N_ ("&OK"), B_ENTER, NULL, NULL),
                QUICK_BUTTON (N_ ("To a &file..."), B_USER, NULL, NULL),
                QUICK_BUTTON (N_ ("&Cancel"), B_CANCEL, NULL, NULL),
            QUICK_END,
        };
        /* clang-format on */

        WRect r = { -1, -1, 0, 60 };

        quick_dialog_t qdlg = {
            .rect = r,
            .title = title,
            .help = NULL,
            .widgets = quick_widgets,
            .callback = NULL,
            .mouse_callback = NULL,
        };

        rc = quick_dialog (&qdlg);
    }
    g_free (title);

    if (rc == B_USER)
        put_to_file (ui, label, offset, size);
    else if (rc == B_ENTER)
    {
        unsigned char *buf = read_range (ui, _ ("Write bytes"), offset, size);

        if (buf != NULL)
        {
            char *group = g_strdup_printf ("%d", id);
            char *path = bin_path (id);
            GDateTime *now = g_date_time_new_now_local ();
            char *time = g_date_time_format (now, "%Y-%m-%d %H:%M");

            write_file (_ ("Write bytes"), path, buf, size);
            mc_config_set_string (cfg, group, "name",
                                  fname != NULL && fname[0] != '\0' ? fname : sname);
            mc_config_set_string (cfg, group, "struct", sname);
            mc_config_set_int (cfg, group, "size", (int) size);
            mc_config_set_string (cfg, group, "source", ui->display_name);
            mc_config_set_string (cfg, group, "path", ui->fr->path);
            mc_config_set_int (cfg, group, "offset", (int) offset);
            mc_config_set_string (cfg, group, "time", time);
            mc_config_set_int (cfg, group, "stamp", (int) (g_get_real_time () / G_USEC_PER_SEC));
            mc_config_set_int (cfg, STORE_GROUP, "next", id + 1);
            store_save (cfg, _ ("Write bytes"));
            g_free (time);
            g_date_time_unref (now);
            g_free (path);
            g_free (group);
        }
        g_free (buf);
    }
    g_free (fname);
    g_free (label);
    g_free (sname);
    mc_config_deinit (cfg);
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-F2: a fragment from the store, or a file, over the current row or block; Delete in the
   list drops a fragment from the store */
void
ui_cmd_get_bytes (ui_t *ui)
{
    off_t offset, size;
    char *label, *name, *sname, *title;
    mc_config_t *cfg;
    GPtrArray *list;
    Listbox *lb;
    void *chosen;
    guint i;
    int lines;

    if (!ui_bytes_range (ui, _ ("Read bytes"), &offset, &size, &name, &sname))
        return;
    label = g_strdup_printf (_ ("%s: %lld bytes at %08llX from file:"), name, (long long) size,
                             (unsigned long long) offset);
    cfg = store_open ();
    list = store_list (cfg);
    store_expire (cfg, list, ui->settings.fragment_days);
    if (list->len == 0)
    {
        get_from_file (ui, label, offset, size);
        goto done;
    }

    title = g_strdup_printf (_ ("Read bytes into %s (%s): %lld bytes at %08llX"), name, sname,
                             (long long) size, (unsigned long long) offset);
    lines = MIN ((int) list->len + 1, MAX_LIST_LINES);
    lb = listbox_window_new (lines, 78, title, NULL);
    g_free (title);
    lb->list->deletable = TRUE;
    for (i = 0; i < list->len; i++)
    {
        frag_t *f = g_ptr_array_index (list, i);
        /* str_trunc has one buffer: a copy per column */
        char *fname = g_strdup (str_trunc (f->name, 15));
        char *fsname = g_strdup (str_trunc (f->sname, 10));
        char *fsource = g_strdup (str_trunc (f->source, 15));
        /* 73 columns: the list fits 76 and cuts the middle of a longer line */
        char *text = g_strdup_printf ("#%-4d%c%7lld %-15s %-10s %-15s %s", f->id,
                                      f->size == size ? ' ' : '!', (long long) f->size, fname,
                                      fsname, fsource, f->time);

        listbox_add_item_take (lb->list, LISTBOX_APPEND_AT_END, 0, text, f, FALSE);
        g_free (fsource);
        g_free (fsname);
        g_free (fname);
    }
    listbox_add_item (lb->list, LISTBOX_APPEND_AT_END, 0, _ ("      ... a file"),
                      (void *) &from_file_item, FALSE);
    chosen = NULL;
    if (dlg_run (lb->dlg) != B_CANCEL)
    {
        const WLEntry *e = listbox_get_nth_entry (lb->list, lb->list->current);

        if (e != NULL)
            chosen = e->data;
    }
    /* what stayed in the list decides what stays in the store */
    delete_removed (cfg, lb->list, list);
    widget_destroy (WIDGET (lb->dlg));
    g_free (lb);

    if (chosen == &from_file_item)
        get_from_file (ui, label, offset, size);
    else if (chosen != NULL)
    {
        const frag_t *f = chosen;
        char *path = bin_path (f->id);

        get_from_path (ui, path, offset, size);
        g_free (path);
    }

done:
    g_ptr_array_free (list, TRUE);
    mc_config_deinit (cfg);
    g_free (label);
    g_free (name);
    g_free (sname);
}

/* --------------------------------------------------------------------------------------------- */
