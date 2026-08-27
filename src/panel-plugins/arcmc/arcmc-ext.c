/*
   Archive browser panel plugin -external archiver registry.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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

#include <string.h>

#include "lib/global.h"
#include "lib/mcconfig.h"

#include "arcmc-types.h"
#include "arcmc-config.h"
#include "arcmc-ext.h"

/*** file scope macro definitions ****************************************************************/

#define ARCMC_SECTION_EXT       "arcmc-ext"
#define ARCMC_SECTION_EXT_PARAM "arcmc-ext-params-"

/*** file scope type declarations ****************************************************************/

typedef struct
{
    const char *name;
    const char *ext;
    const char *pack_bin;
    const char *pack_args;
    const char *unpack_bin;
    const char *unpack_args;
    const char *test_bin;
    const char *test_args;
    const char *extfs_helper;
    const char *list_file_arg;
} arcmc_ext_archiver_default_t;

/*** file scope variables ************************************************************************/

static const arcmc_ext_archiver_default_t ext_archiver_defaults[] = {
    { "RAR", ".rar", "rar", "a -r", "unrar", "x -o+", "unrar", "t", "urar", "@%s" },
    { "ARJ", ".arj", "arj", "a -r", "arj", "x -y", "arj", "t", "uarj", "!%s" },
    { "ACE", ".ace", NULL, NULL, "unace", "x -o", "unace", "t", "uace", NULL },
    { "ARC", ".arc", "arc", "a", "arc", "x", NULL, NULL, "uarc", NULL },
    { "ALZ", ".alz", NULL, NULL, "unalz", "", NULL, NULL, "ualz", NULL },
    { "ZOO", ".zoo", "zoo", "a", "zoo", "x", NULL, NULL, "uzoo", NULL },
    { "HA", ".ha", "ha", "a", "ha", "x", "ha", "t", "uha", NULL },
    { "WIM", ".wim", NULL, NULL, "wimlib-imagex", "extract", NULL, NULL, "uwim", NULL },
    { "LHA", ".lha", "lha", "a", "lha", "x", "lha", "t", "ulha", "@%s" },
    { "LZH", ".lzh", "lha", "a", "lha", "x", "lha", "t", "ulha", "@%s" },
    { "DEB", ".deb", NULL, NULL, "dpkg-deb", "-x", NULL, NULL, "deb", NULL },
    { "RPM", ".rpm", NULL, NULL, NULL, NULL, NULL, NULL, "rpm", NULL },
    { "INO", ".exe", NULL, NULL, NULL, NULL, "innoextract", "--test --silent", "uinno", NULL },
};

/*** global variables ****************************************************************************/

arcmc_ext_archiver_t *ext_archivers = NULL;
size_t ext_archivers_count = 0;
gboolean *arcmc_ext_enabled = NULL;

/*** file scope functions ************************************************************************/

static void
arcmc_ext_archiver_clear (arcmc_ext_archiver_t *a)
{
    g_free (a->name);
    g_free (a->ext);
    g_free (a->pack_bin);
    g_free (a->pack_args);
    g_free (a->unpack_bin);
    g_free (a->unpack_args);
    g_free (a->test_bin);
    g_free (a->test_args);
    g_free (a->extfs_helper);
    g_free (a->list_file_arg);
}

/* --------------------------------------------------------------------------------------------- */

static arcmc_ext_archiver_t *
arcmc_ext_archiver_append (const char *name, const char *extension)
{
    arcmc_ext_archiver_t *a;

    ext_archivers = g_renew (arcmc_ext_archiver_t, ext_archivers, ext_archivers_count + 1);
    a = &ext_archivers[ext_archivers_count++];
    memset (a, 0, sizeof (*a));
    a->name = g_strdup (name);
    a->ext = g_strdup (extension);

    return a;
}

/* --------------------------------------------------------------------------------------------- */

static arcmc_ext_archiver_t *
arcmc_ext_archiver_by_name_mutable (const char *name)
{
    size_t i;

    for (i = 0; i < ext_archivers_count; i++)
        if (g_ascii_strcasecmp (ext_archivers[i].name, name) == 0)
            return &ext_archivers[i];

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static char *
arcmc_ext_normalize_extension (char *extension)
{
    char *normalized;

    if (extension == NULL)
        return NULL;

    g_strstrip (extension);
    if (extension[0] == '\0' || strcmp (extension, ".") == 0 || strchr (extension, '/') != NULL)
    {
        g_free (extension);
        return NULL;
    }

    if (extension[0] == '.')
        return extension;

    normalized = g_strconcat (".", extension, (char *) NULL);
    g_free (extension);
    return normalized;
}

/* --------------------------------------------------------------------------------------------- */

static char *
arcmc_ext_load_extension (mc_config_t *cfg, const char *section)
{
    char *extension;

    extension = mc_config_get_string (cfg, section, "extension", NULL);
    if (extension == NULL)
        extension = mc_config_get_string (cfg, section, "ext", NULL);

    return arcmc_ext_normalize_extension (extension);
}

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_ext_load_string (mc_config_t *cfg, const char *section, const char *key, char **field)
{
    char *value;

    value = mc_config_get_string (cfg, section, key, NULL);
    if (value == NULL)
        return;

    g_free (*field);
    if (value[0] != '\0')
        *field = value;
    else
    {
        g_free (value);
        *field = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_ext_load_group (mc_config_t *cfg, const char *section)
{
    const char *name = section + strlen (ARCMC_SECTION_EXT_PARAM);
    arcmc_ext_archiver_t *a;
    char *extension;

    if (name[0] == '\0')
        return;

    a = arcmc_ext_archiver_by_name_mutable (name);
    extension = arcmc_ext_load_extension (cfg, section);

    /* A previously unknown name becomes a registry row when its suffix is supplied. */
    if (a == NULL)
    {
        if (extension == NULL)
            return;
        a = arcmc_ext_archiver_append (name, extension);
    }
    else if (extension != NULL)
    {
        g_free (a->ext);
        a->ext = g_strdup (extension);
    }

    g_free (extension);
    arcmc_ext_load_string (cfg, section, "pack_bin", &a->pack_bin);
    arcmc_ext_load_string (cfg, section, "pack_args", &a->pack_args);
    arcmc_ext_load_string (cfg, section, "unpack_bin", &a->unpack_bin);
    arcmc_ext_load_string (cfg, section, "unpack_args", &a->unpack_args);
    arcmc_ext_load_string (cfg, section, "test_bin", &a->test_bin);
    arcmc_ext_load_string (cfg, section, "test_args", &a->test_args);
    arcmc_ext_load_string (cfg, section, "extfs_helper", &a->extfs_helper);
    arcmc_ext_load_string (cfg, section, "list_file_arg", &a->list_file_arg);
}

/* --------------------------------------------------------------------------------------------- */

static void
arcmc_ext_save_string (mc_config_t *cfg, const char *section, const char *key, const char *value)
{
    mc_config_set_string (cfg, section, key, value != NULL ? value : "");
}

/*** public functions ****************************************************************************/

void
arcmc_ext_archivers_load (mc_config_t *cfg)
{
    gchar **groups;
    size_t i;

    arcmc_ext_archivers_free ();

    for (i = 0; i < G_N_ELEMENTS (ext_archiver_defaults); i++)
    {
        const arcmc_ext_archiver_default_t *d = &ext_archiver_defaults[i];
        arcmc_ext_archiver_t *a = arcmc_ext_archiver_append (d->name, d->ext);

        a->pack_bin = g_strdup (d->pack_bin);
        a->pack_args = g_strdup (d->pack_args);
        a->unpack_bin = g_strdup (d->unpack_bin);
        a->unpack_args = g_strdup (d->unpack_args);
        a->test_bin = g_strdup (d->test_bin);
        a->test_args = g_strdup (d->test_args);
        a->extfs_helper = g_strdup (d->extfs_helper);
        a->list_file_arg = g_strdup (d->list_file_arg);
    }

    if (cfg != NULL)
    {
        gchar **group;

        groups = mc_config_get_groups (cfg, NULL);
        for (group = groups; *group != NULL; group++)
            if (g_str_has_prefix (*group, ARCMC_SECTION_EXT_PARAM))
                arcmc_ext_load_group (cfg, *group);
        g_strfreev (groups);
    }

    arcmc_ext_enabled = g_new (gboolean, ext_archivers_count);
    for (i = 0; i < ext_archivers_count; i++)
        arcmc_ext_enabled[i] =
            cfg == NULL || mc_config_get_bool (cfg, ARCMC_SECTION_EXT, ext_archivers[i].name, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

void
arcmc_ext_archivers_save (mc_config_t *cfg)
{
    size_t i;

    if (cfg == NULL)
        return;

    for (i = 0; i < ext_archivers_count; i++)
    {
        const arcmc_ext_archiver_t *a = &ext_archivers[i];
        char *section = g_strconcat (ARCMC_SECTION_EXT_PARAM, a->name, (char *) NULL);

        mc_config_set_bool (cfg, ARCMC_SECTION_EXT, a->name,
                            arcmc_ext_enabled == NULL || arcmc_ext_enabled[i]);
        arcmc_ext_save_string (cfg, section, "extension", a->ext);
        arcmc_ext_save_string (cfg, section, "pack_bin", a->pack_bin);
        arcmc_ext_save_string (cfg, section, "pack_args", a->pack_args);
        arcmc_ext_save_string (cfg, section, "unpack_bin", a->unpack_bin);
        arcmc_ext_save_string (cfg, section, "unpack_args", a->unpack_args);
        arcmc_ext_save_string (cfg, section, "test_bin", a->test_bin);
        arcmc_ext_save_string (cfg, section, "test_args", a->test_args);
        arcmc_ext_save_string (cfg, section, "extfs_helper", a->extfs_helper);
        arcmc_ext_save_string (cfg, section, "list_file_arg", a->list_file_arg);
        g_free (section);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
arcmc_ext_archivers_free (void)
{
    size_t i;

    for (i = 0; i < ext_archivers_count; i++)
        arcmc_ext_archiver_clear (&ext_archivers[i]);

    g_free (ext_archivers);
    ext_archivers = NULL;
    ext_archivers_count = 0;

    g_free (arcmc_ext_enabled);
    arcmc_ext_enabled = NULL;
}

/* --------------------------------------------------------------------------------------------- */

const arcmc_ext_archiver_t *
arcmc_ext_archiver_by_name (const char *name)
{
    if (name == NULL)
        return NULL;

    return arcmc_ext_archiver_by_name_mutable (name);
}

/* --------------------------------------------------------------------------------------------- */

const arcmc_ext_archiver_t *
arcmc_find_ext_archiver (const char *archive_path)
{
    const arcmc_ext_archiver_t *result = NULL;
    const char *basename_ptr;
    size_t basename_len, result_len = 0;
    size_t i;

    if (archive_path == NULL)
        return NULL;

    basename_ptr = strrchr (archive_path, '/');
    basename_ptr = basename_ptr != NULL ? basename_ptr + 1 : archive_path;
    basename_len = strlen (basename_ptr);

    /* Prefer the longest suffix when configured formats overlap. */
    for (i = 0; i < ext_archivers_count; i++)
    {
        size_t extension_len = strlen (ext_archivers[i].ext);

        if (extension_len > result_len && basename_len >= extension_len
            && g_ascii_strcasecmp (basename_ptr + basename_len - extension_len,
                                   ext_archivers[i].ext)
                == 0)
        {
            result = &ext_archivers[i];
            result_len = extension_len;
        }
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
