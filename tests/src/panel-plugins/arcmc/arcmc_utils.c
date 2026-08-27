/*
   src/panel-plugins/arcmc - tests for archive manager utility functions

   Copyright (C) 2026
   Free Software Foundation, Inc.

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

#define TEST_SUITE_NAME "/src/panel-plugins/arcmc"

#include "tests/mctest.h"

#include <string.h>

/* --------------------------------------------------------------------------------------------- */
/* Minimal type and data definitions copied from arcmc sources for isolated testing.              */
/* --------------------------------------------------------------------------------------------- */

/* Format IDs (from arcmc-types.h) */
enum
{
    ARCMC_FMT_ZIP = 0,
    ARCMC_FMT_7Z = 1,
    ARCMC_FMT_TAR_GZ = 2,
    ARCMC_FMT_TAR_BZ2 = 3,
    ARCMC_FMT_TAR_XZ = 4,
    ARCMC_FMT_TAR = 5,
    ARCMC_FMT_CPIO = 6,
};

/* Copied utility functions under test                                                           */
/* --------------------------------------------------------------------------------------------- */

/* ---- get_parent_dir (archive-io.c) ---- */

static char *
get_parent_dir (const char *current_dir)
{
    const char *slash;

    if (current_dir == NULL || current_dir[0] == '\0')
        return g_strdup ("");

    slash = strrchr (current_dir, '/');
    if (slash == NULL)
        return g_strdup ("");

    return g_strndup (current_dir, (gsize) (slash - current_dir));
}

/* ---- build_child_path (archive-io.c) ---- */

static char *
build_child_path (const char *current_dir, const char *name)
{
    if (current_dir == NULL || current_dir[0] == '\0')
        return g_strdup (name);

    return g_strdup_printf ("%s/%s", current_dir, name);
}

/* ---- is_direct_child (archive-io.c) ---- */

static const char *
is_direct_child (const char *entry_path, const char *dir)
{
    size_t dir_len;
    const char *rest;

    if (dir == NULL || dir[0] == '\0')
    {
        /* root: direct child if no '/' in path */
        if (strchr (entry_path, '/') == NULL)
            return entry_path;
        return NULL;
    }

    dir_len = strlen (dir);

    if (strncmp (entry_path, dir, dir_len) != 0)
        return NULL;

    if (entry_path[dir_len] != '/')
        return NULL;

    rest = entry_path + dir_len + 1;

    /* must not contain further '/' (i.e., must be direct child) */
    if (rest[0] == '\0' || strchr (rest, '/') != NULL)
        return NULL;

    return rest;
}

/* ---- is_under_dir (archive-io.c) ---- */

static gboolean
is_under_dir (const char *entry_path, const char *dir)
{
    size_t dir_len;

    if (dir == NULL || dir[0] == '\0')
        return TRUE;

    dir_len = strlen (dir);

    return strncmp (entry_path, dir, dir_len) == 0 && entry_path[dir_len] == '/';
}

/* ---- arcmc_detect_fmt_id (archive-io.c, static) ---- */

static int
arcmc_detect_fmt_id (const char *filename)
{
    static const struct
    {
        const char *ext;
        int fmt;
    } map[] = {
        { ".tar.gz", ARCMC_FMT_TAR_GZ },   { ".tgz", ARCMC_FMT_TAR_GZ },
        { ".tar.bz2", ARCMC_FMT_TAR_BZ2 }, { ".tbz2", ARCMC_FMT_TAR_BZ2 },
        { ".tar.xz", ARCMC_FMT_TAR_XZ },   { ".txz", ARCMC_FMT_TAR_XZ },
        { ".tar", ARCMC_FMT_TAR },         { ".zip", ARCMC_FMT_ZIP },
        { ".jar", ARCMC_FMT_ZIP },         { ".war", ARCMC_FMT_ZIP },
        { ".ear", ARCMC_FMT_ZIP },         { ".7z", ARCMC_FMT_7Z },
        { ".cpio", ARCMC_FMT_CPIO },
    };

    size_t flen, i;

    flen = strlen (filename);

    for (i = 0; i < G_N_ELEMENTS (map); i++)
    {
        size_t elen = strlen (map[i].ext);

        if (flen >= elen && g_ascii_strcasecmp (filename + flen - elen, map[i].ext) == 0)
            return map[i].fmt;
    }

    return -1;
}

/* ---- arcmc_check_bin_available (archive-io.c) ---- */

static gboolean
arcmc_check_bin_available (const char *bin_name)
{
    char *full_path;

    if (bin_name == NULL || bin_name[0] == '\0')
        return FALSE;

    full_path = g_find_program_in_path (bin_name);
    if (full_path != NULL)
    {
        g_free (full_path);
        return TRUE;
    }

    return FALSE;
}

/* ======================================================================================= */
/*                                     TEST DATA                                           */
/* ======================================================================================= */

/* ---- get_parent_dir ---- */

/* @DataSource("test_get_parent_dir_ds") */
static const struct test_get_parent_dir_ds
{
    const char *input;
    const char *expected;
} test_get_parent_dir_ds[] = {
    { "dir/subdir", "dir" }, /* 0: simple parent */
    { "single", "" },        /* 1: no slash -> root */
    { "", "" },              /* 2: empty string */
    { NULL, "" },            /* 3: NULL */
    { "a/b/c", "a/b" },      /* 4: nested path */
};

/* ---- build_child_path ---- */

/* @DataSource("test_build_child_path_ds") */
static const struct test_build_child_path_ds
{
    const char *current_dir;
    const char *name;
    const char *expected;
} test_build_child_path_ds[] = {
    { "dir", "file", "dir/file" }, /* 0: simple join */
    { "", "file", "file" },        /* 1: empty dir */
    { NULL, "file", "file" },      /* 2: NULL dir */
    { "a/b", "c", "a/b/c" },       /* 3: nested join */
};

/* ---- is_direct_child ---- */

/* @DataSource("test_is_direct_child_ds") */
static const struct test_is_direct_child_ds
{
    const char *entry_path;
    const char *dir;
    const char *expected;
} test_is_direct_child_ds[] = {
    { "dir/file", "dir", "file" },   /* 0: direct child */
    { "dir/sub/file", "dir", NULL }, /* 1: nested - not direct */
    { "file", "", "file" },          /* 2: root dir (empty) */
    { "file", NULL, "file" },        /* 3: root dir (NULL) */
    { "other/file", "dir", NULL },   /* 4: wrong prefix */
};

/* ---- is_under_dir ---- */

/* @DataSource("test_is_under_dir_ds") */
static const struct test_is_under_dir_ds
{
    const char *path;
    const char *dir;
    gboolean expected;
} test_is_under_dir_ds[] = {
    { "dir/file", "dir", TRUE },     /* 0: direct child */
    { "dir/sub/file", "dir", TRUE }, /* 1: deeper below */
    { "dir", "dir", FALSE },         /* 2: the directory itself */
    { "dirty/file", "dir", FALSE },  /* 3: name only starts the same */
    { "other/file", "dir", FALSE },  /* 4: elsewhere */
    { "anything", "", TRUE },        /* 5: root holds everything */
    { "a/b/c", NULL, TRUE },         /* 6: root as NULL */
};

/* ---- arcmc_detect_fmt_id ---- */

/* @DataSource("test_detect_fmt_ds") */
static const struct test_detect_fmt_ds
{
    const char *filename;
    int expected;
} test_detect_fmt_ds[] = {
    { "f.tar.gz", ARCMC_FMT_TAR_GZ }, /* 0: tar.gz */
    { "f.tgz", ARCMC_FMT_TAR_GZ },    /* 1: tgz alias */
    { "f.zip", ARCMC_FMT_ZIP },       /* 2: zip */
    { "f.jar", ARCMC_FMT_ZIP },       /* 3: jar -> zip format */
    { "f.war", ARCMC_FMT_ZIP },       /* 4: war -> zip format */
    { "f.ear", ARCMC_FMT_ZIP },       /* 5: ear -> zip format */
    { "f.7z", ARCMC_FMT_7Z },         /* 6: 7z */
    { "f.txt", -1 },                  /* 7: unknown */
};

/* ---- arcmc_check_bin_available ---- */

/* @DataSource("test_check_bin_ds") */
static const struct test_check_bin_ds
{
    const char *bin_name;
    gboolean expected;
} test_check_bin_ds[] = {
    { "ls", TRUE },               /* 0: ls always exists */
    { "nonexistent_xyz", FALSE }, /* 1: nonexistent binary */
    { NULL, FALSE },              /* 2: NULL */
    { "", FALSE },                /* 3: empty string */
};

/* ======================================================================================= */
/*                                       TESTS                                             */
/* ======================================================================================= */

/* @Test(dataSource = "test_get_parent_dir_ds") */
START_PARAMETRIZED_TEST (test_get_parent_dir, test_get_parent_dir_ds)
{
    char *result;

    result = get_parent_dir (data->input);
    mctest_assert_str_eq (result, data->expected);

    g_free (result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_build_child_path_ds") */
START_PARAMETRIZED_TEST (test_build_child_path, test_build_child_path_ds)
{
    char *result;

    result = build_child_path (data->current_dir, data->name);
    mctest_assert_str_eq (result, data->expected);

    g_free (result);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_is_direct_child_ds") */
START_PARAMETRIZED_TEST (test_is_direct_child, test_is_direct_child_ds)
{
    const char *result;

    result = is_direct_child (data->entry_path, data->dir);

    if (data->expected == NULL)
    {
        mctest_assert_null (result);
    }
    else
    {
        mctest_assert_str_eq (result, data->expected);
    }
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_is_under_dir_ds") */
START_PARAMETRIZED_TEST (test_is_under_dir, test_is_under_dir_ds)
{
    gboolean result;

    result = is_under_dir (data->path, data->dir);
    ck_assert_int_eq (result, data->expected);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_detect_fmt_ds") */
START_PARAMETRIZED_TEST (test_detect_fmt_id, test_detect_fmt_ds)
{
    int result;

    result = arcmc_detect_fmt_id (data->filename);
    ck_assert_int_eq (result, data->expected);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_check_bin_ds") */
START_PARAMETRIZED_TEST (test_check_bin_available, test_check_bin_ds)
{
    gboolean result;

    result = arcmc_check_bin_available (data->bin_name);
    ck_assert_int_eq (result, data->expected);
}
END_PARAMETRIZED_TEST

/* ======================================================================================= */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    /* path utilities */
    mctest_add_parameterized_test (tc_core, test_get_parent_dir, test_get_parent_dir_ds);
    mctest_add_parameterized_test (tc_core, test_build_child_path, test_build_child_path_ds);
    mctest_add_parameterized_test (tc_core, test_is_direct_child, test_is_direct_child_ds);
    mctest_add_parameterized_test (tc_core, test_is_under_dir, test_is_under_dir_ds);

    /* format detection */
    mctest_add_parameterized_test (tc_core, test_detect_fmt_id, test_detect_fmt_ds);

    /* binary availability */
    mctest_add_parameterized_test (tc_core, test_check_bin_available, test_check_bin_ds);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
