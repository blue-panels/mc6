/* shared by the skin editor model tests: a skin file in a temp directory */

#ifndef MC__SKINEDIT_TEST_COMMON_H
#define MC__SKINEDIT_TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include "lib/strutil.h"
#include "lib/vfs/vfs.h"

#include "src/vfs/local/local.c"

static char *test_dir = NULL;

/* called from main(), before the tests run: no ck_assert here */
static void
test_env_init (void)
{
    test_dir = g_dir_make_tmp ("mc-skinedit-XXXXXX", NULL);
    if (test_dir == NULL)
    {
        fprintf (stderr, "cannot create a temp directory\n");
        exit (1);
    }
    /* keep mc_config_get_data_path() out of the real home */
    g_setenv ("XDG_DATA_HOME", test_dir, TRUE);
    g_setenv ("XDG_CONFIG_HOME", test_dir, TRUE);
    g_setenv ("XDG_CACHE_HOME", test_dir, TRUE);
}

static void
test_remove_tree (const char *path)
{
    GDir *dir;
    const char *name;

    dir = g_dir_open (path, 0, NULL);
    if (dir != NULL)
    {
        while ((name = g_dir_read_name (dir)) != NULL)
        {
            char *child = g_build_filename (path, name, (char *) NULL);

            test_remove_tree (child);
            g_free (child);
        }
        g_dir_close (dir);
    }
    g_remove (path);
}

/* called from main() after the tests ran */
static G_GNUC_UNUSED void
test_env_deinit (void)
{
    test_remove_tree (test_dir);
    g_free (test_dir);
    test_dir = NULL;
}

/* the model reads and writes files through the VFS */
static G_GNUC_UNUSED void
test_vfs_init (void)
{
    str_init_strings ("UTF-8");
    vfs_init ();
    vfs_init_localfs ();
}

static G_GNUC_UNUSED void
test_vfs_deinit (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

static G_GNUC_UNUSED char *
test_write_skin (const char *name, const char *content)
{
    char *path;
    GError *error = NULL;

    path = g_build_filename (test_dir, name, (char *) NULL);
    ck_assert_msg (g_file_set_contents (path, content, -1, &error), "%s",
                   error != NULL ? error->message : "?");
    return path;
}

static G_GNUC_UNUSED char *
test_read_file (const char *path)
{
    char *content = NULL;

    ck_assert (g_file_get_contents (path, &content, NULL, NULL));
    return content;
}

#endif
