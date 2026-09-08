/*
   src/panel-plugins/docker - tests for the links among the files of a container

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

#define TEST_SUITE_NAME "/src/panel-plugins/docker"

#include "tests/mctest.h"

#include <string.h>

#include "src/filemanager/panel.h"
#include "src/panel-plugins/docker/docker-internal.h"

/* Source-under-test. */
#include "src/panel-plugins/docker/container-files.c"

/* --------------------------------------------------------------------------------------------- */
/* What container-files.c takes from the rest of the plugin. */

void
docker_item_free (gpointer p)
{
    docker_item_t *item = (docker_item_t *) p;

    g_free (item->name);
    g_free (item->id);
    g_free (item->link_target);
    g_free (item->link_dir);
    g_free (item);
}

GPtrArray *
docker_items_clone (const GPtrArray *items)
{
    GPtrArray *copy = g_ptr_array_new_with_free_func (docker_item_free);
    guint i;

    for (i = 0; items != NULL && i < items->len; i++)
    {
        const docker_item_t *item = (const docker_item_t *) g_ptr_array_index (items, i);
        docker_item_t *c = g_new0 (docker_item_t, 1);

        c->name = g_strdup (item->name);
        c->id = g_strdup (item->id);
        c->is_dir = item->is_dir;
        c->is_link = item->is_link;
        c->size = item->size;
        c->link_target = g_strdup (item->link_target);
        g_ptr_array_add (copy, c);
    }

    return copy;
}

const docker_item_t *
find_item_by_name (const docker_data_t *d, const char *name)
{
    (void) d;
    (void) name;
    return NULL;
}

void
set_view (docker_data_t *d, docker_view_t new_view)
{
    d->view = new_view;
}

gboolean
docker_conn_run (const docker_connection_t *conn, const char *docker_args, char **output,
                 char **err_text)
{
    (void) conn;
    (void) docker_args;
    (void) output;
    (void) err_text;
    return FALSE;
}

char *
docker_conn_build_pipe_cmd (const docker_connection_t *conn, const char *docker_args)
{
    (void) conn;
    (void) docker_args;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static docker_data_t data;
#define CID "c1"

static void
setup (void)
{
    memset (&data, 0, sizeof (data));
    data.files_cache =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
}

static void
teardown (void)
{
    g_hash_table_destroy (data.files_cache);
}

static void
dir (const char *cwd, const char *name)
{
    files_cache_add_dir_item (&data, CID, cwd, name, TRUE, FALSE, 0, NULL);
    {
        char *path = mc_pp_join_path (cwd, name);

        files_cache_get_or_create_dir (&data, CID, path);
        g_free (path);
    }
}

static void
file (const char *cwd, const char *name)
{
    files_cache_add_dir_item (&data, CID, cwd, name, FALSE, FALSE, 7, NULL);
}

static void
add_link (const char *cwd, const char *name, const char *target)
{
    files_cache_add_dir_item (&data, CID, cwd, name, FALSE, TRUE, 0, target);
}

static const docker_item_t *
item (const char *cwd, const char *name)
{
    return files_cache_item (&data, CID, cwd, name);
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_normalize)
{
    char *p;

    p = files_path_normalize ("/usr/./bin/../lib//x/");
    ck_assert_str_eq (p, "/usr/lib/x");
    g_free (p);
    p = files_path_normalize ("/../a");
    ck_assert_str_eq (p, "/a");
    g_free (p);
    p = files_path_normalize ("/");
    ck_assert_str_eq (p, "/");
    g_free (p);
    p = files_path_normalize ("a/b");
    ck_assert_str_eq (p, "/a/b");
    g_free (p);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_links_are_resolved)
{
    GPtrArray *root;

    dir ("/", "usr");
    dir ("/usr", "bin");
    file ("/usr/bin", "sh");
    add_link ("/", "bin", "usr/bin");
    add_link ("/", "sbin", "/usr/bin");
    add_link ("/", "tolink", "bin");
    add_link ("/", "tosh", "usr/bin/sh");
    add_link ("/", "dangling", "nowhere");
    add_link ("/", "loop1", "loop2");
    add_link ("/", "loop2", "loop1");
    dir ("/", "x");
    add_link ("/x", "up", "../usr/bin");

    root = files_cache_lookup (&data, CID, "/");
    files_cache_resolve_links (&data, CID, "/", root);

    {
        guint i;

        for (i = 0; i < root->len; i++)
        {
            const docker_item_t *it = (const docker_item_t *) g_ptr_array_index (root, i);

            if (strcmp (it->name, "bin") == 0 || strcmp (it->name, "sbin") == 0
                || strcmp (it->name, "tolink") == 0)
            {
                ck_assert_msg (it->link_to_dir, "%s", it->name);
                ck_assert_str_eq (it->link_dir, "/usr/bin");
            }
            else if (strcmp (it->name, "tosh") == 0)
            {
                ck_assert (!it->link_to_dir);
                ck_assert (!it->stale_link);
            }
            else if (it->is_link)
                ck_assert_msg (it->stale_link, "%s", it->name);
        }
    }
    g_ptr_array_unref (root);

    {
        GPtrArray *x = files_cache_lookup (&data, CID, "/x");
        const docker_item_t *up;

        files_cache_resolve_links (&data, CID, "/x", x);
        up = (const docker_item_t *) g_ptr_array_index (x, 0);
        ck_assert_str_eq (up->name, "up");
        ck_assert (up->link_to_dir);
        ck_assert_str_eq (up->link_dir, "/usr/bin");
        g_ptr_array_unref (x);
    }

    ck_assert (item ("/", "usr") != NULL);
    ck_assert (item ("/", "none") == NULL);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);

    tcase_add_test (tc_core, test_normalize);
    tcase_add_test (tc_core, test_links_are_resolved);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
