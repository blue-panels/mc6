/*
   src/panel-plugins/arcmc - tests for the external archiver registry

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

#define TEST_SUITE_NAME "/src/panel-plugins/arcmc/ext-registry"

#include "tests/mctest.h"

#include "lib/mcconfig.h"

#include "src/panel-plugins/arcmc/arcmc-config.h"
#include "src/panel-plugins/arcmc/arcmc-ext.h"

/*** file scope variables ************************************************************************/

static mc_config_t *cfg;

/*** fixtures *************************************************************************************/

static void
setup (void)
{
    cfg = mc_config_init (NULL, FALSE);
    mctest_assert_not_null (cfg);
}

/* --------------------------------------------------------------------------------------------- */

static void
teardown (void)
{
    arcmc_ext_archivers_free ();
    mc_config_deinit (cfg);
}

/*** tests ****************************************************************************************/

START_TEST (test_defaults_include_innoextract)
{
    const arcmc_ext_archiver_t *a;

    arcmc_ext_archivers_load (NULL);

    a = arcmc_ext_archiver_by_name ("INO");
    mctest_assert_not_null (a);
    ck_assert_str_eq (a->ext, ".exe");
    ck_assert_str_eq (a->test_bin, "innoextract");
    ck_assert_str_eq (a->extfs_helper, "uinno");
    mctest_assert_true (a->enabled);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_config_adds_arbitrary_extension)
{
    const arcmc_ext_archiver_t *a;
    size_t default_count;

    arcmc_ext_archivers_load (NULL);
    default_count = ext_archivers_count;

    mc_config_set_bool (cfg, "arcmc-ext", "CUSTOM", FALSE);
    mc_config_set_string (cfg, "arcmc-ext-params-CUSTOM", "extension", "pkgx");
    mc_config_set_string (cfg, "arcmc-ext-params-CUSTOM", "unpack_bin", "pkgx-unpack");
    mc_config_set_string (cfg, "arcmc-ext-params-CUSTOM", "unpack_args", "extract --all");
    mc_config_set_string (cfg, "arcmc-ext-params-CUSTOM", "test_bin", "pkgx-test");
    mc_config_set_string (cfg, "arcmc-ext-params-CUSTOM", "extfs_helper", "upkgx");

    arcmc_ext_archivers_load (cfg);

    ck_assert_uint_eq (ext_archivers_count, default_count + 1);
    a = arcmc_ext_archiver_by_name ("custom");
    mctest_assert_not_null (a);
    ck_assert_str_eq (a->ext, ".pkgx");
    ck_assert_str_eq (a->unpack_bin, "pkgx-unpack");
    ck_assert_str_eq (a->unpack_args, "extract --all");
    ck_assert_str_eq (a->test_bin, "pkgx-test");
    ck_assert_str_eq (a->extfs_helper, "upkgx");

    ck_assert_str_eq (a->name, "CUSTOM");
    mctest_assert_false (a->enabled);
    ck_assert_ptr_eq (arcmc_find_ext_archiver ("/tmp/archive.PKGX"), a);

    /* Reloading the same ini rebuilds the registry instead of duplicating rows. */
    arcmc_ext_archivers_load (cfg);
    ck_assert_uint_eq (ext_archivers_count, default_count + 1);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_longest_suffix_wins)
{
    const arcmc_ext_archiver_t *a;

    mc_config_set_string (cfg, "arcmc-ext-params-SHORT", "extension", ".pkgx");
    mc_config_set_string (cfg, "arcmc-ext-params-LONG", "extension", ".tar.pkgx");

    arcmc_ext_archivers_load (cfg);

    a = arcmc_find_ext_archiver ("/tmp/archive.tar.PKGX");
    mctest_assert_not_null (a);
    ck_assert_str_eq (a->name, "LONG");
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_canonical_case_wins_and_save_removes_aliases)
{
    const arcmc_ext_archiver_t *a;
    char *extension;

    mc_config_set_bool (cfg, "arcmc-ext", "rar", FALSE);
    mc_config_set_bool (cfg, "arcmc-ext", "RAR", TRUE);
    mc_config_set_string (cfg, "arcmc-ext-params-rar", "extension", ".lower");
    mc_config_set_string (cfg, "arcmc-ext-params-RAR", "extension", ".upper");

    arcmc_ext_archivers_load (cfg);

    a = arcmc_ext_archiver_by_name ("rar");
    mctest_assert_not_null (a);
    ck_assert_str_eq (a->name, "RAR");
    ck_assert_str_eq (a->ext, ".upper");
    mctest_assert_true (a->enabled);

    arcmc_ext_archivers_save (cfg);
    mctest_assert_false (mc_config_has_group (cfg, "arcmc-ext-params-rar"));
    mctest_assert_false (mc_config_has_param (cfg, "arcmc-ext", "rar"));
    mctest_assert_true (mc_config_has_group (cfg, "arcmc-ext-params-RAR"));
    mctest_assert_true (mc_config_has_param (cfg, "arcmc-ext", "RAR"));
    extension =
        mc_config_get_string (cfg, "arcmc-ext-params-RAR", "extension", NULL);
    ck_assert_str_eq (extension, ".upper");
    g_free (extension);

    arcmc_ext_archivers_load (cfg);
    a = arcmc_ext_archiver_by_name ("RAR");
    mctest_assert_not_null (a);
    ck_assert_str_eq (a->ext, ".upper");
    mctest_assert_true (a->enabled);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_lowercase_alias_is_loaded_and_canonicalized)
{
    const arcmc_ext_archiver_t *a;

    mc_config_set_bool (cfg, "arcmc-ext", "rar", FALSE);
    mc_config_set_string (cfg, "arcmc-ext-params-rar", "extension", ".lower");

    arcmc_ext_archivers_load (cfg);

    a = arcmc_ext_archiver_by_name ("RAR");
    mctest_assert_not_null (a);
    ck_assert_str_eq (a->ext, ".lower");
    mctest_assert_false (a->enabled);

    arcmc_ext_archivers_save (cfg);
    mctest_assert_false (mc_config_has_group (cfg, "arcmc-ext-params-rar"));
    mctest_assert_false (mc_config_has_param (cfg, "arcmc-ext", "rar"));
    mctest_assert_true (mc_config_has_group (cfg, "arcmc-ext-params-RAR"));
    mctest_assert_true (mc_config_has_param (cfg, "arcmc-ext", "RAR"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_custom_format_requires_extension)
{
    size_t default_count;

    arcmc_ext_archivers_load (NULL);
    default_count = ext_archivers_count;
    mc_config_set_string (cfg, "arcmc-ext-params-NOEXT", "unpack_bin", "noext");

    arcmc_ext_archivers_load (cfg);

    ck_assert_uint_eq (ext_archivers_count, default_count);
    ck_assert_ptr_null (arcmc_ext_archiver_by_name ("NOEXT"));
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_registry_save_persists_extension)
{
    char *extension;

    mc_config_set_string (cfg, "arcmc-ext-params-CUSTOM", "ext", "pkgx");
    arcmc_ext_archivers_load (cfg);
    arcmc_ext_archivers_save (cfg);

    extension = mc_config_get_string (cfg, "arcmc-ext-params-CUSTOM", "extension", NULL);
    mctest_assert_not_null (extension);
    ck_assert_str_eq (extension, ".pkgx");
    g_free (extension);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    str_init_strings ("UTF-8");

    tc_core = tcase_create ("Core");
    tcase_add_checked_fixture (tc_core, setup, teardown);
    tcase_add_test (tc_core, test_defaults_include_innoextract);
    tcase_add_test (tc_core, test_config_adds_arbitrary_extension);
    tcase_add_test (tc_core, test_longest_suffix_wins);
    tcase_add_test (tc_core, test_canonical_case_wins_and_save_removes_aliases);
    tcase_add_test (tc_core, test_lowercase_alias_is_loaded_and_canonicalized);
    tcase_add_test (tc_core, test_custom_format_requires_extension);
    tcase_add_test (tc_core, test_registry_save_persists_extension);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
