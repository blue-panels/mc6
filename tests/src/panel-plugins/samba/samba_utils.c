/*
   src/panel-plugins/samba - tests for Samba utility functions

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

#define TEST_SUITE_NAME "/src/panel-plugins/samba"

#include "tests/mctest.h"

#include "lib/panel-plugin.h"
#include "lib/util.h"

typedef struct
{
    gboolean at_root;
    char *current_url;
    GPtrArray *entries;
    char *current_label;
    char *focus_name;
    char *title_buf;
    char *help_filename;
} samba_data_t;

/* --------------------------------------------------------------------------------------------- */
/* Copied utility functions under test                                                           */
/* --------------------------------------------------------------------------------------------- */

static char *
smb_url_up (const char *url)
{
    const char *after_scheme;
    const char *last_slash;

    after_scheme = url + 6; /* past "smb://" */

    if (*after_scheme == '\0')
        return NULL;

    last_slash = strrchr (after_scheme, '/');
    if (last_slash == NULL)
        return NULL; /* at smb://SERVER level -- no parent */

    return g_strndup (url, (gsize) (last_slash - url));
}

/* --------------------------------------------------------------------------------------------- */

static void
samba_update_title (samba_data_t *data)
{
    g_free (data->title_buf);

    if (data->at_root || data->current_url == NULL)
    {
        data->title_buf = g_strdup ("/");
        return;
    }

    /* Strip "smb:/" to get path for display */
    if (strlen (data->current_url) <= 5)
        data->title_buf = g_strdup ("/");
    else
        data->title_buf = g_strdup (data->current_url + 5);
}

/* --------------------------------------------------------------------------------------------- */

static void
samba_leave_connection (samba_data_t *data)
{
    data->at_root = TRUE;
    g_free (data->current_url);
    data->current_url = NULL;
    data->focus_name = data->current_label;
    data->current_label = NULL;

    if (data->entries != NULL)
    {
        g_ptr_array_free (data->entries, TRUE);
        data->entries = NULL;
    }
    samba_update_title (data);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
samba_get_focus_name (void *plugin_data)
{
    samba_data_t *data = (samba_data_t *) plugin_data;

    return data->focus_name;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
samba_get_help_info (void *plugin_data, const char **filename, const char **node)
{
    samba_data_t *data = (samba_data_t *) plugin_data;

    if (filename != NULL && data != NULL && data->help_filename != NULL
        && exist_file (data->help_filename))
        *filename = data->help_filename;
    else if (filename != NULL)
        *filename = NULL;
    if (node != NULL)
        *node = "[Samba Plugin]";

    if (filename != NULL && *filename == NULL)
        return MC_PPR_NOT_SUPPORTED;

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* @DataSource("test_smb_url_up_ds") */
static const struct test_smb_url_up_ds
{
    const char *input;
    const char *expected;
} test_smb_url_up_ds[] = {
    { "smb://", NULL },
    { "smb://SERVER", NULL },
    { "smb://SERVER/", "smb://SERVER" },
    { "smb://SERVER/SHARE", "smb://SERVER" },
    { "smb://SERVER/SHARE/", "smb://SERVER/SHARE" },
    { "smb://SERVER/SHARE/path", "smb://SERVER/SHARE" },
    { "smb://SERVER/SHARE/path/nested", "smb://SERVER/SHARE/path" },
};

/* --------------------------------------------------------------------------------------------- */

/* @Test(dataSource = "test_smb_url_up_ds") */
START_PARAMETRIZED_TEST (test_smb_url_up, test_smb_url_up_ds)
{
    char *actual;

    actual = smb_url_up (data->input);

    if (data->expected == NULL)
    {
        mctest_assert_null (actual);
    }
    else
    {
        mctest_assert_str_eq (actual, data->expected);
    }

    g_free (actual);
}
END_PARAMETRIZED_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_samba_get_help_info_existing_file)
{
    samba_data_t plugin_data;
    char template_path[] = "/tmp/mc-samba-help-XXXXXX";
    int fd;
    const char *filename = NULL;
    const char *node = NULL;
    mc_pp_result_t result;

    fd = g_mkstemp (template_path);
    ck_assert_int_ne (fd, -1);
    close (fd);

    plugin_data.help_filename = template_path;

    result = samba_get_help_info (&plugin_data, &filename, &node);

    ck_assert_int_eq (result, MC_PPR_OK);
    mctest_assert_str_eq (filename, template_path);
    mctest_assert_str_eq (node, "[Samba Plugin]");

    unlink (template_path);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_samba_get_help_info_missing_file)
{
    samba_data_t plugin_data;
    const char *filename = "sentinel";
    const char *node = NULL;
    mc_pp_result_t result;

    plugin_data.help_filename = (char *) "/tmp/mc-samba-help-does-not-exist";

    result = samba_get_help_info (&plugin_data, &filename, &node);

    ck_assert_int_eq (result, MC_PPR_NOT_SUPPORTED);
    mctest_assert_null (filename);
    mctest_assert_str_eq (node, "[Samba Plugin]");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_samba_get_help_info_null_data)
{
    const char *filename = "sentinel";
    const char *node = NULL;
    mc_pp_result_t result;

    result = samba_get_help_info (NULL, &filename, &node);

    ck_assert_int_eq (result, MC_PPR_NOT_SUPPORTED);
    mctest_assert_null (filename);
    mctest_assert_str_eq (node, "[Samba Plugin]");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_samba_leave_connection_focuses_label)
{
    samba_data_t data;
    const char *focus;

    memset (&data, 0, sizeof (data));
    data.at_root = FALSE;
    data.current_url = g_strdup ("smb://SERVER/SHARE");
    data.current_label = g_strdup ("Second");
    data.entries = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (data.entries, g_strdup ("docs"));
    samba_update_title (&data);
    mctest_assert_str_eq (data.title_buf, "/SERVER/SHARE");

    samba_leave_connection (&data);

    ck_assert_int_eq (data.at_root, TRUE);
    mctest_assert_null (data.current_url);
    mctest_assert_null (data.entries);
    mctest_assert_null (data.current_label);
    mctest_assert_str_eq (data.title_buf, "/");

    focus = samba_get_focus_name (&data);
    mctest_assert_str_eq (focus, "Second");

    g_free (data.focus_name);
    g_free (data.title_buf);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_samba_leave_connection_without_label)
{
    samba_data_t data;

    memset (&data, 0, sizeof (data));
    data.current_url = g_strdup ("smb://SERVER");

    samba_leave_connection (&data);

    ck_assert_int_eq (data.at_root, TRUE);
    mctest_assert_null (samba_get_focus_name (&data));

    g_free (data.title_buf);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    mctest_add_parameterized_test (tc_core, test_smb_url_up, test_smb_url_up_ds);
    tcase_add_test (tc_core, test_samba_get_help_info_existing_file);
    tcase_add_test (tc_core, test_samba_get_help_info_missing_file);
    tcase_add_test (tc_core, test_samba_get_help_info_null_data);
    tcase_add_test (tc_core, test_samba_leave_connection_focuses_label);
    tcase_add_test (tc_core, test_samba_leave_connection_without_label);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
