/*
   Panel plugin mcpeek for the M-Commander
   .NET assembly browser panel plugin

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

#include <config.h>

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "lib/global.h"
#include "lib/panel-plugin.h"
#include "lib/mcconfig.h"
#include "lib/plugin-prefs.h"
#include "lib/tty/key.h"
#include "lib/util.h"
#include "lib/vfs/vfs.h"
#include "lib/widget.h"

#include "src/viewer/mcviewer.h"

#include "mcpeek-cs.h"
#include "mcpeek-find.h"
#include "mcpeek-il.h"
#include "mcpeek-resolve.h"
#include "mcpeek-meta.h"
#include "mcpeek-sig.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define REFERENCES_DIR "References"
#define RESOURCES_DIR  "Resources"
#define IL_DIR         "IL"
#define PROPERTIES_DIR "Properties"

/*** file scope type declarations ****************************************************************/

typedef enum
{
    LEVEL_SET,        /* a directory of assemblies */
    LEVEL_ASSEMBLY,   /* the assembly root: a project directory when there is
                         a decompiler, the metadata tree when there is not */
    LEVEL_SRC_NS,     /* the .cs of one namespace */
    LEVEL_SRC_PROPS,  /* the Properties directory of an exported project */
    LEVEL_IL,         /* the metadata tree, when sources have the root */
    LEVEL_NAMESPACE,  /* types of one namespace */
    LEVEL_TYPE,       /* members of one type */
    LEVEL_REFERENCES, /* the AssemblyRef table */
    LEVEL_RESOURCES,  /* the ManifestResource table */
    LEVEL_USES        /* where a type or member is used */
} mcpeek_level_t;

typedef struct
{
    char *name;
    gboolean is_dir;
    guint32 token;
    const char *kind;
    off_t size;
    char *aux;       /* a resolved reference: the file it points at */
    guint32 use_idx; /* a search result: its number in the results, from 1 */
} mcpeek_entry_t;

typedef struct
{
    mc_panel_host_t *host;

    char *root_dir;  /* the assembly set */
    char *asm_name;  /* basename of the open assembly, NULL at set level */
    char *ns;        /* current namespace, NULL outside it */
    GArray *nesting; /* guint32 TypeDef rids, innermost last */

    mcpeek_image_t *img;
    mcpeek_meta_t meta;
    mc_pp_input_stream_t *stream; /* owned when the panel came from a stream */
    char *temp_file;              /* a stream with no file behind it, drained here */
    char *stream_name;            /* what that stream was called */

    gboolean cs_mode; /* a decompiler was found: the root is a project */

    mcpeek_level_t level;
    GPtrArray *entries;
    GPtrArray *asm_stack; /* assemblies entered through a reference */
    GPtrArray *crumbs;    /* names walked into, so going up can return to one */
    char *focus;          /* the entry to put the cursor on after going up */

    GPtrArray *uses;             /* mcpeek_use_t of the last search */
    mcpeek_target_t *target;     /* what that search was for */
    mcpeek_level_t level_before; /* where the search was started from */

    char *title;
    char column_buf[128];
} mcpeek_data_t;

/*** forward declarations (file scope functions) *************************************************/

static void *mcpeek_open (mc_panel_host_t *host, const char *open_path);
static void mcpeek_close (void *plugin_data);
static mc_pp_result_t mcpeek_get_items (void *plugin_data, void *list_ptr);
static mc_pp_result_t mcpeek_chdir (void *plugin_data, const char *path);
static const char *mcpeek_get_title (void *plugin_data);
static const mc_panel_column_t *mcpeek_get_columns (void *plugin_data, size_t *count);
static const char *mcpeek_get_column_value (void *plugin_data, const char *fname,
                                            const char *column_id);
static const char *mcpeek_get_default_format (void *plugin_data);
static char *mcpeek_get_location (void *plugin_data);
static gboolean mcpeek_is_file_listing (void *plugin_data);
static mc_pp_result_t mcpeek_handle_key (void *plugin_data, int key);
static mc_pp_result_t mcpeek_reload (void *plugin_data);
static const char *mcpeek_get_focus_name (void *plugin_data);
static mc_pp_result_t mcpeek_get_help_info (void *plugin_data, const char **filename,
                                            const char **node);
static char *assembly_path (const mcpeek_data_t *data);
static void uses_clear (mcpeek_data_t *data);
static mc_pp_result_t mcpeek_goto_use (mcpeek_data_t *data, const mcpeek_entry_t *e);
static mc_pp_result_t mcpeek_enter (void *plugin_data, const char *fname, const struct stat *st);
static mc_pp_result_t mcpeek_get_local_copy (void *plugin_data, const char *fname,
                                             char **local_path);
static mc_pp_result_t mcpeek_get_quick_view (void *plugin_data, const char *fname,
                                             const struct stat *st, char **local_path);
static mc_pp_result_t mcpeek_view (void *plugin_data, const char *fname, const struct stat *st,
                                   gboolean plain_view);
static mc_pp_result_t mcpeek_copy_to_local (void *plugin_data, const char *fname,
                                            const char *local_path);
static void mcpeek_configure (void);
static void mcpeek_shutdown (void);
static gboolean mcpeek_may_open_name (const char *display_name);
static void *mcpeek_open_stream (mc_panel_host_t *host, const char *display_name,
                                 mc_pp_input_stream_t *stream);

/*** file scope variables ************************************************************************/

static const mc_panel_column_t mcpeek_columns[] = {
    { "kind", N_ ("Kind"), 8, FALSE, J_LEFT, TRUE },
    { "token", N_ ("Token"), 10, FALSE, J_LEFT, TRUE },
    { "asm", N_ ("Assembly"), 16, FALSE, J_LEFT, TRUE },
};

/* Resolved once from mcpeek.ini; 0 disables the search. */
static int key_find_uses = 0;
static int key_export_project = 0;
static gboolean keys_resolved = FALSE;

static const mc_pp_file_operation_t mcpeek_file_operations[] = {
    {
        .name = "browse",
        .kind = MC_PP_FILE_OPERATION_OPEN,
        .may_open_name = mcpeek_may_open_name,
        .open_input_stream = mcpeek_open_stream,
        .view_input_stream = NULL,
        .show = NULL,
    },
};

static const mc_panel_plugin_t mcpeek_plugin = {
    .api_version = MC_PANEL_PLUGIN_API_VERSION,
    .name = "mcpeek",
    .display_name = N_ (".NET assembly browser"),
    .proto = "peek",
    .prefix = "peek:",
    .flags = MC_PPF_NAVIGATE | MC_PPF_GET_FILES | MC_PPF_CUSTOM_TITLE | MC_PPF_SHOW_IN_MENU
        | MC_PPF_VIEW_ON_ENTER | MC_PPF_COPY_TREE | MC_PPF_NO_MOVE,

    .open = mcpeek_open,
    .close = mcpeek_close,
    .get_items = mcpeek_get_items,

    .file_operations = mcpeek_file_operations,
    .file_operation_count = G_N_ELEMENTS (mcpeek_file_operations),

    .chdir = mcpeek_chdir,
    .enter = mcpeek_enter,
    .get_local_copy = mcpeek_get_local_copy,
    .get_quick_view = mcpeek_get_quick_view,
    .view = mcpeek_view,
    .copy_to_local = mcpeek_copy_to_local,
    .configure = mcpeek_configure,
    .shutdown = mcpeek_shutdown,
    .get_title = mcpeek_get_title,
    .get_columns = mcpeek_get_columns,
    .get_column_value = mcpeek_get_column_value,
    .get_default_format = mcpeek_get_default_format,
    .get_focus_name = mcpeek_get_focus_name,
    .get_help_info = mcpeek_get_help_info,
    .handle_key = mcpeek_handle_key,
    .reload = mcpeek_reload,
    .get_location = mcpeek_get_location,
    .is_file_listing = mcpeek_is_file_listing,
    .default_sort_id = "name",
};

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
entry_free (gpointer p)
{
    mcpeek_entry_t *e = (mcpeek_entry_t *) p;

    g_free (e->name);
    g_free (e->aux);
    g_free (e);
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_add (mcpeek_data_t *data, const char *name, gboolean is_dir, const char *kind, guint32 token,
           off_t size)
{
    mcpeek_entry_t *e;

    e = g_new0 (mcpeek_entry_t, 1);
    e->name = g_strdup (name);
    e->is_dir = is_dir;
    e->kind = kind;
    e->token = token;
    e->size = size;

    g_ptr_array_add (data->entries, e);
}

/* --------------------------------------------------------------------------------------------- */

/* Attach a resolved path to the entry just added. */
static void
entry_set_aux (mcpeek_data_t *data, char *aux)
{
    mcpeek_entry_t *e;

    if (data->entries->len == 0)
    {
        g_free (aux);
        return;
    }

    e = g_ptr_array_index (data->entries, data->entries->len - 1);
    g_free (e->aux);
    e->aux = aux;
}

/* --------------------------------------------------------------------------------------------- */

static void
entry_set_use_idx (mcpeek_data_t *data, guint32 idx)
{
    mcpeek_entry_t *e;

    if (data->entries->len == 0)
        return;

    e = g_ptr_array_index (data->entries, data->entries->len - 1);
    e->use_idx = idx;
}

/* --------------------------------------------------------------------------------------------- */

static void
report_error (const mcpeek_data_t *data, const GError *error)
{
    if (data->host != NULL && data->host->message != NULL && !mc_pp_quiet_messages ())
        data->host->message (data->host, D_ERROR, _ ("mcpeek"),
                             error != NULL ? error->message : _ ("mcpeek failed"));
}

/* --------------------------------------------------------------------------------------------- */

static const mcpeek_entry_t *
entry_find (const mcpeek_data_t *data, const char *name)
{
    guint i;

    if (data->entries == NULL)
        return NULL;

    for (i = 0; i < data->entries->len; i++)
    {
        const mcpeek_entry_t *e = g_ptr_array_index (data->entries, i);

        if (strcmp (e->name, name) == 0)
            return e;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcpeek_unload (mcpeek_data_t *data)
{
    if (data->img != NULL)
    {
        mcpeek_image_free (data->img);
        data->img = NULL;
    }
    memset (&data->meta, 0, sizeof (data->meta));

    MC_PTR_FREE (data->asm_name);
    MC_PTR_FREE (data->ns);
    if (data->nesting != NULL)
        g_array_set_size (data->nesting, 0);
}

/* --------------------------------------------------------------------------------------------- */

/* Open another assembly in place of the current one.  A failure leaves the
   current one loaded: the panel keeps showing what it showed. */
static gboolean
mcpeek_load_path (mcpeek_data_t *data, const char *path)
{
    mcpeek_image_t *img;
    mcpeek_meta_t meta;
    GError *error = NULL;

    memset (&meta, 0, sizeof (meta));

    img = mcpeek_image_open (path, &error);
    if (img != NULL && !mcpeek_meta_init (&meta, img, &error))
    {
        mcpeek_image_free (img);
        img = NULL;
    }

    if (img == NULL)
    {
        if (data->host != NULL && data->host->message != NULL && !mc_pp_quiet_messages ())
            data->host->message (data->host, D_ERROR, _ ("mcpeek"),
                                 error != NULL ? error->message : _ ("cannot read assembly"));
        g_clear_error (&error);
        return FALSE;
    }
    g_clear_error (&error);

    mcpeek_unload (data);

    data->img = img;
    data->meta = meta;

    g_free (data->root_dir);
    data->root_dir = g_path_get_dirname (path);
    data->asm_name = g_path_get_basename (path);
    data->cs_mode = mcpeek_cs_command () != NULL;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcpeek_load (mcpeek_data_t *data, const char *name)
{
    char *path;
    gboolean ok;

    path = g_build_filename (data->root_dir, name, (char *) NULL);
    ok = mcpeek_load_path (data, path);
    g_free (path);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static guint32
current_type_rid (const mcpeek_data_t *data)
{
    if (data->nesting == NULL || data->nesting->len == 0)
        return 0;

    return g_array_index (data->nesting, guint32, data->nesting->len - 1);
}

/* --------------------------------------------------------------------------------------------- */

/* A TypeDef is listed at namespace level only when it is not nested inside
   another type: nested ones show up under their enclosing type instead. */
static gboolean
type_is_nested (const mcpeek_meta_t *meta, guint32 type_rid)
{
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_NESTEDCLASS);
    for (i = 1; i <= n; i++)
        if (mcpeek_meta_col (meta, MCPEEK_T_NESTEDCLASS, i, MCPEEK_NESTED_NESTED) == type_rid)
            return TRUE;

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

/* Size of a method body: the code size out of its tiny or fat header. */
static off_t
method_body_size (const mcpeek_data_t *data, guint32 rva)
{
    const guint8 *p;

    if (rva == 0)
        return 0;

    p = mcpeek_rva_ptr (data->img, rva, 1);
    if (p == NULL)
        return 0;

    if ((p[0] & 3) == 2)
        return p[0] >> 2; /* tiny: size in the top six bits */

    p = mcpeek_rva_ptr (data->img, rva, 12);
    if (p == NULL)
        return 0;

    {
        guint32 code_size;

        memcpy (&code_size, p + 4, 4);
        return GUINT32_FROM_LE (code_size);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
build_set (mcpeek_data_t *data)
{
    GDir *dir;
    const char *name;

    dir = g_dir_open (data->root_dir, 0, NULL);
    if (dir == NULL)
        return;

    while ((name = g_dir_read_name (dir)) != NULL)
    {
        char *path;
        vfs_path_t *vpath;
        struct stat st;

        if (!mcpeek_may_open_name (name))
            continue;

        path = g_build_filename (data->root_dir, name, (char *) NULL);
        vpath = vfs_path_from_str (path);
        if (mc_stat (vpath, &st) == 0 && S_ISREG (st.st_mode) && mcpeek_image_is_managed (path))
            entry_add (data, name, FALSE, "asm", 0, st.st_size);
        vfs_path_free (vpath, TRUE);
        g_free (path);
    }

    g_dir_close (dir);
}

/* --------------------------------------------------------------------------------------------- */

static void
build_assembly (mcpeek_data_t *data)
{
    const mcpeek_meta_t *meta = &data->meta;
    GHashTable *seen;
    guint32 i, n;

    if (mcpeek_meta_rows (meta, MCPEEK_T_ASSEMBLYREF) != 0)
        entry_add (data, REFERENCES_DIR, TRUE, "refs", 0, 0);
    if (mcpeek_meta_rows (meta, MCPEEK_T_MANIFESTRESOURCE) != 0)
        entry_add (data, RESOURCES_DIR, TRUE, "res", 0, 0);

    /* with a decompiler the root is what the sources would look like, and
       the metadata tree moves one level down */
    if (data->cs_mode)
    {
        char *proj;

        entry_add (data, IL_DIR, TRUE, "il", 0, 0);
        entry_add (data, PROPERTIES_DIR, TRUE, "props", 0, 0);

        proj = g_strdup (data->asm_name);
        {
            char *dot = strrchr (proj, '.');

            if (dot != NULL)
                *dot = '\0';
        }
        {
            char *label = g_strconcat (proj, ".csproj", (char *) NULL);

            entry_add (data, label, FALSE, "proj", 0, 0);
            g_free (label);
        }
        g_free (proj);
    }

    seen = g_hash_table_new (g_str_hash, g_str_equal);

    n = mcpeek_meta_rows (meta, MCPEEK_T_TYPEDEF);
    for (i = 1; i <= n; i++)
    {
        const char *ns;

        if (type_is_nested (meta, i))
            continue;

        ns = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAMESPACE));

        if (*ns == '\0')
        {
            const char *name;

            /* the compiler-generated <Module> pseudo-type is noise */
            name = mcpeek_meta_string (
                meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAME));
            if (strcmp (name, "<Module>") == 0)
                continue;

            if (data->cs_mode)
            {
                char *label = g_strconcat (name, ".cs", (char *) NULL);

                entry_add (data, label, FALSE, "src", 0x02000000 | i, 0);
                entry_set_aux (data, g_strdup (name));
                g_free (label);
            }
            else
                entry_add (data, name, TRUE, "type", 0x02000000 | i, 0);
        }
        else if (!g_hash_table_contains (seen, ns))
        {
            g_hash_table_add (seen, (gpointer) ns);
            entry_add (data, ns, TRUE, "ns", 0, 0);
        }
    }

    g_hash_table_destroy (seen);
}

/* --------------------------------------------------------------------------------------------- */

/* The metadata tree, one level down when sources hold the root. */
static void
build_il (mcpeek_data_t *data)
{
    const mcpeek_meta_t *meta = &data->meta;
    GHashTable *seen;
    guint32 i, n;

    seen = g_hash_table_new (g_str_hash, g_str_equal);

    n = mcpeek_meta_rows (meta, MCPEEK_T_TYPEDEF);
    for (i = 1; i <= n; i++)
    {
        const char *ns;

        if (type_is_nested (meta, i))
            continue;

        ns = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAMESPACE));

        if (*ns == '\0')
        {
            const char *name;

            name = mcpeek_meta_string (
                meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAME));
            if (strcmp (name, "<Module>") != 0)
                entry_add (data, name, TRUE, "type", 0x02000000 | i, 0);
        }
        else if (!g_hash_table_contains (seen, ns))
        {
            g_hash_table_add (seen, (gpointer) ns);
            entry_add (data, ns, TRUE, "ns", 0, 0);
        }
    }

    g_hash_table_destroy (seen);
}

/* --------------------------------------------------------------------------------------------- */

/* One file per top-level type, the way the decompiler lays a project out: a
   nested type lives inside the file of the type that encloses it. */
static void
build_src_namespace (mcpeek_data_t *data)
{
    const mcpeek_meta_t *meta = &data->meta;
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_TYPEDEF);
    for (i = 1; i <= n; i++)
    {
        const char *ns, *name;
        char *label;

        if (type_is_nested (meta, i))
            continue;

        ns = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAMESPACE));
        if (strcmp (ns, data->ns) != 0)
            continue;

        name = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAME));

        label = g_strconcat (name, ".cs", (char *) NULL);
        entry_add (data, label, FALSE, "src", 0x02000000 | i, 0);
        entry_set_aux (data, g_strconcat (ns, ".", name, (char *) NULL));
        g_free (label);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
build_src_props (mcpeek_data_t *data)
{
    entry_add (data, "AssemblyInfo.cs", FALSE, "asminfo", 0, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
build_namespace (mcpeek_data_t *data)
{
    const mcpeek_meta_t *meta = &data->meta;
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_TYPEDEF);
    for (i = 1; i <= n; i++)
    {
        const char *ns;

        if (type_is_nested (meta, i))
            continue;

        ns = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAMESPACE));
        if (strcmp (ns, data->ns) != 0)
            continue;

        entry_add (data,
                   mcpeek_meta_string (
                       meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAME)),
                   TRUE, "type", 0x02000000 | i, 0);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The row of a map table that belongs to type @rid: a property or event list
   is reached through PropertyMap or EventMap, not from the TypeDef row. */
static guint32
map_row_for_type (const mcpeek_meta_t *meta, int map_table, guint32 rid)
{
    guint32 i, n;

    n = mcpeek_meta_rows (meta, map_table);
    for (i = 1; i <= n; i++)
        if (mcpeek_meta_col (meta, map_table, i, MCPEEK_MAP_PARENT) == rid)
            return i;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
build_properties (mcpeek_data_t *data, guint32 rid)
{
    const mcpeek_meta_t *meta = &data->meta;
    guint32 map, first, last, i;

    map = map_row_for_type (meta, MCPEEK_T_PROPERTYMAP, rid);
    if (map == 0)
        return;

    first = mcpeek_meta_col (meta, MCPEEK_T_PROPERTYMAP, map, MCPEEK_MAP_LIST);
    last =
        mcpeek_meta_list_end (meta, MCPEEK_T_PROPERTYMAP, map, MCPEEK_MAP_LIST, MCPEEK_T_PROPERTY);

    for (i = first; i != 0 && i < last; i++)
    {
        char *type, *params, *label;
        const char *name;

        type = mcpeek_sig_property (
            meta, mcpeek_meta_col (meta, MCPEEK_T_PROPERTY, i, MCPEEK_PROPERTY_TYPE), &params);
        name = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_PROPERTY, i, MCPEEK_PROPERTY_NAME));
        /* an indexer is told from its overloads by its parameters */
        if (params != NULL)
            label = g_strdup_printf ("%s[%s] { } : %s", name, params, type);
        else
            label = g_strdup_printf ("%s { } : %s", name, type);
        entry_add (data, label, FALSE, "prop", 0x17000000 | i, 0);
        g_free (label);
        g_free (params);
        g_free (type);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
build_events (mcpeek_data_t *data, guint32 rid)
{
    const mcpeek_meta_t *meta = &data->meta;
    guint32 map, first, last, i;

    map = map_row_for_type (meta, MCPEEK_T_EVENTMAP, rid);
    if (map == 0)
        return;

    first = mcpeek_meta_col (meta, MCPEEK_T_EVENTMAP, map, MCPEEK_MAP_LIST);
    last = mcpeek_meta_list_end (meta, MCPEEK_T_EVENTMAP, map, MCPEEK_MAP_LIST, MCPEEK_T_EVENT);

    for (i = first; i != 0 && i < last; i++)
    {
        char *type, *label;
        int t_table;
        guint32 t_rid;
        guint32 coded;

        coded = mcpeek_meta_col (meta, MCPEEK_T_EVENT, i, 2);
        if (mcpeek_meta_decode (MCPEEK_CI_TYPEDEFORREF, coded, &t_table, &t_rid))
            type = mcpeek_type_name (meta, t_table, t_rid, FALSE);
        else
            type = g_strdup ("?");

        label = g_strdup_printf (
            "%s : %s",
            mcpeek_meta_string (meta, mcpeek_meta_col (meta, MCPEEK_T_EVENT, i, MCPEEK_EVENT_NAME)),
            type);
        entry_add (data, label, FALSE, "event", 0x14000000 | i, 0);
        g_free (label);
        g_free (type);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
build_type (mcpeek_data_t *data)
{
    const mcpeek_meta_t *meta = &data->meta;
    guint32 rid = current_type_rid (data);
    guint32 first, last, i, n;

    if (rid == 0)
        return;

    /* nested types */
    n = mcpeek_meta_rows (meta, MCPEEK_T_NESTEDCLASS);
    for (i = 1; i <= n; i++)
        if (mcpeek_meta_col (meta, MCPEEK_T_NESTEDCLASS, i, MCPEEK_NESTED_ENCLOSING) == rid)
        {
            guint32 nested;

            nested = mcpeek_meta_col (meta, MCPEEK_T_NESTEDCLASS, i, MCPEEK_NESTED_NESTED);
            entry_add (
                data,
                mcpeek_meta_string (
                    meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, nested, MCPEEK_TYPEDEF_NAME)),
                TRUE, "type", 0x02000000 | nested, 0);
        }

    /* fields */
    first = mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, rid, MCPEEK_TYPEDEF_FIELDLIST);
    last = mcpeek_meta_list_end (meta, MCPEEK_T_TYPEDEF, rid, MCPEEK_TYPEDEF_FIELDLIST,
                                 MCPEEK_T_FIELD);
    for (i = first; i != 0 && i < last; i++)
    {
        char *type, *label;

        type = mcpeek_sig_field (meta,
                                 mcpeek_meta_col (meta, MCPEEK_T_FIELD, i, MCPEEK_FIELD_SIGNATURE));
        label = g_strdup_printf (
            "%s : %s",
            mcpeek_meta_string (meta, mcpeek_meta_col (meta, MCPEEK_T_FIELD, i, MCPEEK_FIELD_NAME)),
            type);
        entry_add (data, label, FALSE, "field", 0x04000000 | i, 0);
        g_free (label);
        g_free (type);
    }

    build_properties (data, rid);
    build_events (data, rid);

    /* methods */
    first = mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, rid, MCPEEK_TYPEDEF_METHODLIST);
    last = mcpeek_meta_list_end (meta, MCPEEK_T_TYPEDEF, rid, MCPEEK_TYPEDEF_METHODLIST,
                                 MCPEEK_T_METHODDEF);
    for (i = first; i != 0 && i < last; i++)
    {
        char *sig, *label;
        guint32 rva;

        sig = mcpeek_sig_method (
            meta, mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, i, MCPEEK_METHOD_SIGNATURE));
        label = g_strconcat (
            mcpeek_meta_string (meta,
                                mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, i, MCPEEK_METHOD_NAME)),
            sig, (char *) NULL);
        rva = mcpeek_meta_col (meta, MCPEEK_T_METHODDEF, i, MCPEEK_METHOD_RVA);
        entry_add (data, label, FALSE, "method", 0x06000000 | i, method_body_size (data, rva));
        g_free (label);
        g_free (sig);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* An embedded resource lives in the CLI resources block: its row gives an
   offset into it, and the first four bytes there are the length.  A resource
   held in another file of the assembly has an Implementation and no bytes
   here. */
static const guint8 *
resource_data (mcpeek_data_t *data, guint32 rid, gsize *len)
{
    guint32 offset;
    const guint8 *p;
    guint32 size;

    *len = 0;

    if (data->img == NULL || data->img->resources_rva == 0)
        return NULL;
    if (mcpeek_meta_col (&data->meta, MCPEEK_T_MANIFESTRESOURCE, rid, 3) != 0)
        return NULL;

    offset = mcpeek_meta_col (&data->meta, MCPEEK_T_MANIFESTRESOURCE, rid, MCPEEK_RESOURCE_OFFSET);
    if (offset > data->img->resources_size)
        return NULL;

    p = mcpeek_rva_ptr (data->img, data->img->resources_rva + offset, 4);
    if (p == NULL)
        return NULL;

    memcpy (&size, p, 4);
    size = GUINT32_FROM_LE (size);

    p = mcpeek_rva_ptr (data->img, data->img->resources_rva + offset + 4, size);
    if (p == NULL)
        return NULL;

    *len = size;
    return p;
}

/* --------------------------------------------------------------------------------------------- */

static void
build_references (mcpeek_data_t *data)
{
    const mcpeek_meta_t *meta = &data->meta;
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_ASSEMBLYREF);
    for (i = 1; i <= n; i++)
    {
        char *label;

        label = g_strdup_printf (
            "%s, %u.%u.%u.%u",
            mcpeek_meta_string (
                meta, mcpeek_meta_col (meta, MCPEEK_T_ASSEMBLYREF, i, MCPEEK_ASMREF_NAME)),
            mcpeek_meta_col (meta, MCPEEK_T_ASSEMBLYREF, i, MCPEEK_ASMREF_MAJOR),
            mcpeek_meta_col (meta, MCPEEK_T_ASSEMBLYREF, i, MCPEEK_ASMREF_MINOR),
            mcpeek_meta_col (meta, MCPEEK_T_ASSEMBLYREF, i, MCPEEK_ASMREF_BUILD),
            mcpeek_meta_col (meta, MCPEEK_T_ASSEMBLYREF, i, MCPEEK_ASMREF_REVISION));
        {
            const char *ref_name;
            char *file;

            ref_name = mcpeek_meta_string (
                meta, mcpeek_meta_col (meta, MCPEEK_T_ASSEMBLYREF, i, MCPEEK_ASMREF_NAME));
            file = mcpeek_resolve_assembly (ref_name, data->root_dir);

            /* a reference that resolves can be walked into; one that does not
               is still worth showing, marked for what it is */
            entry_add (data, label, file != NULL, file != NULL ? "ref" : "missing", 0x23000000 | i,
                       0);
            entry_set_aux (data, file);
        }
        g_free (label);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
build_resources (mcpeek_data_t *data)
{
    const mcpeek_meta_t *meta = &data->meta;
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_MANIFESTRESOURCE);
    for (i = 1; i <= n; i++)
    {
        gsize len = 0;

        (void) resource_data (data, i, &len);
        entry_add (
            data,
            mcpeek_meta_string (
                meta, mcpeek_meta_col (meta, MCPEEK_T_MANIFESTRESOURCE, i, MCPEEK_RESOURCE_NAME)),
            FALSE, "res", 0x28000000 | i, (off_t) len);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Every managed assembly of the set, plus the ones reached through a
   reference: the search is over what this panel has actually opened. */
static char **
searched_files (mcpeek_data_t *data)
{
    GPtrArray *files;
    GDir *dir;
    const char *name;
    char *here;
    guint i;

    files = g_ptr_array_new ();

    here = assembly_path (data);
    if (here != NULL)
        g_ptr_array_add (files, here);

    for (i = 0; i < data->asm_stack->len; i++)
        g_ptr_array_add (files, g_strdup (g_ptr_array_index (data->asm_stack, i)));

    dir = g_dir_open (data->root_dir, 0, NULL);
    if (dir != NULL)
    {
        while ((name = g_dir_read_name (dir)) != NULL)
        {
            char *path;

            if (!mcpeek_may_open_name (name))
                continue;

            path = g_build_filename (data->root_dir, name, (char *) NULL);
            /* the ones entered through a reference may live here too */
            for (i = 0; i < files->len; i++)
                if (strcmp (path, g_ptr_array_index (files, i)) == 0)
                    break;
            if (i < files->len)
                g_free (path);
            else if (mcpeek_image_is_managed (path))
                g_ptr_array_add (files, path);
            else
                g_free (path);
        }
        g_dir_close (dir);
    }

    g_ptr_array_add (files, NULL);

    return (char **) g_ptr_array_free (files, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
build_uses (mcpeek_data_t *data)
{
    GHashTable *seen;
    guint i;

    if (data->uses == NULL)
        return;

    seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    for (i = 0; i < data->uses->len; i++)
    {
        const mcpeek_use_t *u = g_ptr_array_index (data->uses, i);
        char *label;

        label = g_strdup_printf ("%s::%s +IL_%04x", u->type, u->method, u->il_offset);

        /* the same place in two assemblies: the file tells them apart, and
           a name has to be one entry */
        if (g_hash_table_contains (seen, label))
        {
            char *unique = g_strdup_printf ("%s [%s]", label, u->assembly);

            g_free (label);
            label = unique;
        }
        g_hash_table_add (seen, g_strdup (label));

        entry_add (data, label, FALSE, "use", 0, 0);
        entry_set_aux (data, g_strdup (u->assembly));
        entry_set_use_idx (data, i + 1);
        g_free (label);
    }

    g_hash_table_destroy (seen);
}

/* --------------------------------------------------------------------------------------------- */

/* The assembly's name for the title: a drained stream is shown under the
   name it came with, not under the temporary file it was copied into. */
static const char *
shown_asm_name (const mcpeek_data_t *data)
{
    if (data->stream_name != NULL && data->temp_file != NULL && data->asm_name != NULL)
    {
        char *path = assembly_path (data);
        gboolean is_temp = strcmp (path, data->temp_file) == 0;

        g_free (path);
        if (is_temp)
            return data->stream_name;
    }

    return data->asm_name;
}

/* --------------------------------------------------------------------------------------------- */

static void
rebuild_title (mcpeek_data_t *data)
{
    GString *s;

    s = g_string_new (data->asm_name != NULL ? shown_asm_name (data) : data->root_dir);

    switch (data->level)
    {
    case LEVEL_REFERENCES:
        g_string_append (s, "/" REFERENCES_DIR);
        break;
    case LEVEL_RESOURCES:
        g_string_append (s, "/" RESOURCES_DIR);
        break;
    case LEVEL_USES:
        g_string_assign (s, _ ("uses of "));
        if (data->target != NULL)
        {
            g_string_append (s, data->target->type);
            if (data->target->member != NULL)
            {
                g_string_append (s, "::");
                g_string_append (s, data->target->member);
            }
        }
        break;
    case LEVEL_IL:
        g_string_append (s, "/" IL_DIR);
        break;
    case LEVEL_SRC_PROPS:
        g_string_append (s, "/" PROPERTIES_DIR);
        break;
    case LEVEL_SRC_NS:
    case LEVEL_NAMESPACE:
    case LEVEL_TYPE:
        if (data->level != LEVEL_SRC_NS && data->cs_mode)
            g_string_append (s, "/" IL_DIR);
        if (data->ns != NULL && *data->ns != '\0')
            g_string_append_printf (s, "/%s", data->ns);
        if (data->level == LEVEL_TYPE && data->nesting != NULL)
        {
            guint i;

            for (i = 0; i < data->nesting->len; i++)
            {
                guint32 rid = g_array_index (data->nesting, guint32, i);

                g_string_append_printf (
                    s, "/%s",
                    mcpeek_meta_string (
                        &data->meta,
                        mcpeek_meta_col (&data->meta, MCPEEK_T_TYPEDEF, rid, MCPEEK_TYPEDEF_NAME)));
            }
        }
        break;
    default:
        break;
    }

    g_free (data->title);
    data->title = g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_get_items (void *plugin_data, void *list_ptr)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;
    guint i;

    g_ptr_array_set_size (data->entries, 0);

    switch (data->level)
    {
    case LEVEL_SET:
        build_set (data);
        break;
    case LEVEL_ASSEMBLY:
        build_assembly (data);
        break;
    case LEVEL_IL:
        build_il (data);
        break;
    case LEVEL_SRC_NS:
        build_src_namespace (data);
        break;
    case LEVEL_SRC_PROPS:
        build_src_props (data);
        break;
    case LEVEL_NAMESPACE:
        build_namespace (data);
        break;
    case LEVEL_TYPE:
        build_type (data);
        break;
    case LEVEL_REFERENCES:
        build_references (data);
        break;
    case LEVEL_RESOURCES:
        build_resources (data);
        break;
    case LEVEL_USES:
        build_uses (data);
        break;
    default:
        break;
    }

    for (i = 0; i < data->entries->len; i++)
    {
        const mcpeek_entry_t *e = g_ptr_array_index (data->entries, i);

        mc_pp_add_entry (list_ptr, e->name, (e->is_dir ? S_IFDIR | 0755 : S_IFREG | 0644), e->size,
                         0);
    }

    rebuild_title (data);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* Remember the name being walked into, and hand it back on the way up. */
static void
crumb_push (mcpeek_data_t *data, const char *name)
{
    g_ptr_array_add (data->crumbs, g_strdup (name));
}

/* --------------------------------------------------------------------------------------------- */

static void
crumb_pop (mcpeek_data_t *data)
{
    g_free (data->focus);
    data->focus = NULL;

    if (data->crumbs->len == 0)
        return;

    data->focus = g_strdup (g_ptr_array_index (data->crumbs, data->crumbs->len - 1));
    g_ptr_array_remove_index (data->crumbs, data->crumbs->len - 1);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_chdir_up (mcpeek_data_t *data)
{
    /* an assembly reached through a reference goes back to the list that
       led here, not to the directory it happens to live in */
    if (data->level == LEVEL_ASSEMBLY && data->asm_stack->len != 0)
    {
        char *prev;
        gboolean ok;

        prev = g_strdup (g_ptr_array_index (data->asm_stack, data->asm_stack->len - 1));
        ok = mcpeek_load_path (data, prev);
        g_free (prev);

        /* the failure has been reported; the panel stays where it is, and
           MC_PPR_FAILED would make the core leave the plugin instead */
        if (!ok)
            return MC_PPR_OK;

        g_ptr_array_remove_index (data->asm_stack, data->asm_stack->len - 1);
        crumb_pop (data);
        data->level = LEVEL_REFERENCES;
        return MC_PPR_OK;
    }

    crumb_pop (data);

    switch (data->level)
    {
    case LEVEL_SET:
        return MC_PPR_CLOSE;

    case LEVEL_ASSEMBLY:
        /* a panel opened on a single assembly has no set to fall back to */
        if (data->stream != NULL)
            return MC_PPR_CLOSE;
        mcpeek_unload (data);
        data->level = LEVEL_SET;
        return MC_PPR_OK;

    case LEVEL_USES:
        uses_clear (data);
        data->level = data->level_before;
        return MC_PPR_OK;

    case LEVEL_REFERENCES:
    case LEVEL_RESOURCES:
    case LEVEL_IL:
    case LEVEL_SRC_NS:
    case LEVEL_SRC_PROPS:
        MC_PTR_FREE (data->ns);
        data->level = LEVEL_ASSEMBLY;
        return MC_PPR_OK;

    case LEVEL_NAMESPACE:
        MC_PTR_FREE (data->ns);
        data->level = data->cs_mode ? LEVEL_IL : LEVEL_ASSEMBLY;
        return MC_PPR_OK;

    case LEVEL_TYPE:
        if (data->nesting->len > 1)
        {
            g_array_set_size (data->nesting, data->nesting->len - 1);
            return MC_PPR_OK;
        }
        g_array_set_size (data->nesting, 0);
        if (data->ns != NULL && *data->ns != '\0')
            data->level = LEVEL_NAMESPACE;
        else
        {
            MC_PTR_FREE (data->ns);
            data->level = data->cs_mode ? LEVEL_IL : LEVEL_ASSEMBLY;
        }
        return MC_PPR_OK;

    default:
        return MC_PPR_FAILED;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* Open the assembly a result lives in and stop on the type that declares the
   using method. */
static mc_pp_result_t
mcpeek_goto_use (mcpeek_data_t *data, const mcpeek_entry_t *e)
{
    const mcpeek_use_t *use;
    guint32 method_rid;
    guint32 owner;

    if (data->uses == NULL || e->use_idx == 0 || e->use_idx > data->uses->len || e->aux == NULL)
        return MC_PPR_FAILED;

    use = g_ptr_array_index (data->uses, e->use_idx - 1);

    /* the results go away with the load; keep what is needed of this one */
    method_rid = use->method_rid;

    if (!mcpeek_load_path (data, e->aux))
        return MC_PPR_FAILED;

    uses_clear (data);
    g_array_set_size (data->nesting, 0);
    MC_PTR_FREE (data->ns);

    owner = mcpeek_meta_owner_type (&data->meta, MCPEEK_T_METHODDEF, MCPEEK_TYPEDEF_METHODLIST,
                                    method_rid);
    if (owner == 0)
    {
        data->level = LEVEL_ASSEMBLY;
        return MC_PPR_OK;
    }

    data->ns = g_strdup (mcpeek_meta_string (
        &data->meta,
        mcpeek_meta_col (&data->meta, MCPEEK_T_TYPEDEF, owner, MCPEEK_TYPEDEF_NAMESPACE)));
    g_array_append_val (data->nesting, owner);
    data->level = LEVEL_TYPE;

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_chdir (void *plugin_data, const char *path)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;
    const mcpeek_entry_t *e;

    if (path == NULL || *path == '\0' || strcmp (path, ".") == 0)
        return MC_PPR_OK;

    if (strcmp (path, "..") == 0)
        return mcpeek_chdir_up (data);

    e = entry_find (data, path);

    /* a search result is a place, not a listing: entering it goes there */
    if (e != NULL && strcmp (e->kind, "use") == 0)
        return mcpeek_goto_use (data, e);

    if (e == NULL)
        return MC_PPR_FAILED;

    /* an assembly of the set is listed as the file it is, and opens all the
       same */
    if (data->level == LEVEL_SET && strcmp (e->kind, "asm") == 0)
    {
        if (!mcpeek_load (data, path))
            return MC_PPR_FAILED;
        crumb_push (data, path);
        data->level = LEVEL_ASSEMBLY;
        return MC_PPR_OK;
    }

    if (!e->is_dir)
        return MC_PPR_FAILED;

    if (data->level == LEVEL_REFERENCES)
    {
        char *here;

        if (e->aux == NULL)
            return MC_PPR_FAILED;

        /* the way back is recorded once there is somewhere to come back from */
        here = assembly_path (data);
        if (!mcpeek_load_path (data, e->aux))
        {
            g_free (here);
            return MC_PPR_FAILED;
        }
        if (here != NULL)
            g_ptr_array_add (data->asm_stack, here);

        crumb_push (data, path);
        data->level = LEVEL_ASSEMBLY;
        return MC_PPR_OK;
    }

    crumb_push (data, path);

    switch (data->level)
    {
    case LEVEL_ASSEMBLY:
        if (strcmp (e->kind, "refs") == 0)
        {
            data->level = LEVEL_REFERENCES;
            return MC_PPR_OK;
        }
        if (strcmp (e->kind, "res") == 0)
        {
            data->level = LEVEL_RESOURCES;
            return MC_PPR_OK;
        }
        if (strcmp (e->kind, "il") == 0)
        {
            data->level = LEVEL_IL;
            return MC_PPR_OK;
        }
        if (strcmp (e->kind, "props") == 0)
        {
            data->level = LEVEL_SRC_PROPS;
            return MC_PPR_OK;
        }
        if (strcmp (e->kind, "ns") == 0)
        {
            g_free (data->ns);
            data->ns = g_strdup (path);
            data->level = data->cs_mode ? LEVEL_SRC_NS : LEVEL_NAMESPACE;
            return MC_PPR_OK;
        }
        /* fall through */
        MC_FALLTHROUGH;

    case LEVEL_IL:
        if (strcmp (e->kind, "ns") == 0)
        {
            g_free (data->ns);
            data->ns = g_strdup (path);
            data->level = LEVEL_NAMESPACE;
            return MC_PPR_OK;
        }
        /* a type of the global namespace */
        g_free (data->ns);
        data->ns = g_strdup ("");
        /* fall through */
        MC_FALLTHROUGH;

    case LEVEL_NAMESPACE:
    case LEVEL_TYPE:
    {
        guint32 rid = e->token & 0x00FFFFFF;

        g_array_append_val (data->nesting, rid);
        data->level = LEVEL_TYPE;
        return MC_PPR_OK;
    }

    default:
        return MC_PPR_FAILED;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The text behind one entry: a method disassembles, a source decompiles,
   anything else gets a short description.  NULL with @error set when what
   should have produced the text failed; a method without a body is text. */
static char *
entry_text (mcpeek_data_t *data, const mcpeek_entry_t *e, GError **error)
{
    guint32 rid = e->token & 0x00FFFFFFu;

    if (strcmp (e->kind, "method") == 0)
    {
        char *il;

        il = mcpeek_il_method (&data->meta, rid);
        if (il != NULL)
            return il;

        return g_strdup_printf ("// %s\n// no body: abstract, extern or native\n", e->name);
    }

    if (strcmp (e->kind, "src") == 0)
    {
        char *asm_file, *text = NULL;

        asm_file = assembly_path (data);
        if (asm_file != NULL && e->aux != NULL)
            text = mcpeek_cs_type (asm_file, e->aux, error);
        else
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", _ ("no source"));
        g_free (asm_file);

        return text;
    }

    if (strcmp (e->kind, "proj") == 0 || strcmp (e->kind, "asminfo") == 0)
    {
        char *asm_file, *file, *text = NULL;
        const char *dir;

        asm_file = assembly_path (data);
        dir = asm_file != NULL ? mcpeek_cs_project_dir (asm_file) : NULL;
        g_free (asm_file);

        if (dir == NULL)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                         _ ("the project could not be written"));
            return NULL;
        }

        if (strcmp (e->kind, "proj") == 0)
            file = g_build_filename (dir, e->name, (char *) NULL);
        else
            file = g_build_filename (dir, PROPERTIES_DIR, e->name, (char *) NULL);

        (void) g_file_get_contents (file, &text, NULL, error);
        g_free (file);

        return text;
    }

    if (strcmp (e->kind, "ref") == 0 || strcmp (e->kind, "missing") == 0)
        return g_strdup_printf ("// %s\n// %s\n", e->name,
                                e->aux != NULL ? e->aux : "not found on this machine");

    return g_strdup_printf ("// %s\n// %s, token %08x\n", e->name, e->kind, e->token);
}

/* --------------------------------------------------------------------------------------------- */

/* What the viewer gets in place of text that could not be had: the reason,
   where the text would have been. */
static char *
entry_note (const mcpeek_entry_t *e, const GError *error)
{
    return g_strdup_printf ("// %s\n// %s\n", e->name,
                            error != NULL ? error->message : _ ("no text"));
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
write_entry_text (mcpeek_data_t *data, const char *fname, char **local_path)
{
    const mcpeek_entry_t *e;
    GError *error = NULL;
    char *text;
    gboolean ok;

    e = entry_find (data, fname);
    if (e == NULL || e->is_dir)
        return MC_PPR_NOT_SUPPORTED;

    /* a resource is bytes, not a listing: hand them over as they are */
    if (strcmp (e->kind, "res") == 0)
    {
        const guint8 *bytes;
        gsize len = 0;

        bytes = resource_data (data, e->token & 0x00FFFFFFu, &len);
        if (bytes == NULL)
            return MC_PPR_NOT_SUPPORTED;

        if (!mc_pp_write_temp_file ("mc-mcpeek-XXXXXX", bytes, (gssize) len, local_path))
            return MC_PPR_FAILED;

        mc_pp_rename_with_ext (local_path, e->name);
        return MC_PPR_OK;
    }

    text = entry_text (data, e, &error);
    if (text == NULL)
    {
        text = entry_note (e, error);
        g_clear_error (&error);
    }
    ok = mc_pp_write_temp_file ("mc-mcpeek-XXXXXX", text, -1, local_path);
    g_free (text);

    if (!ok)
        return MC_PPR_FAILED;

    /* the viewer picks its syntax by extension: a listing is IL, everything
       named like a file keeps the name it has */
    if (strchr (e->name, '.') != NULL
        && (strcmp (e->kind, "src") == 0 || strcmp (e->kind, "proj") == 0
            || strcmp (e->kind, "asminfo") == 0))
        mc_pp_rename_with_ext (local_path, e->name);
    else
        mc_pp_rename_with_ext (local_path, "mcpeek.il");

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* An assembly of the set and a search result are listed as files, which the
   core does not chdir into: they are walked into from here instead. */
static gboolean
entry_is_place (const mcpeek_entry_t *e)
{
    return e != NULL && (strcmp (e->kind, "asm") == 0 || strcmp (e->kind, "use") == 0);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_enter (void *plugin_data, const char *fname, const struct stat *st)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;

    (void) st;

    if (entry_is_place (entry_find (data, fname)))
        return mcpeek_chdir (data, fname);

    /* MC_PPF_VIEW_ON_ENTER turns this into "open the viewer" */
    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_get_local_copy (void *plugin_data, const char *fname, char **local_path)
{
    return write_entry_text ((mcpeek_data_t *) plugin_data, fname, local_path);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_get_quick_view (void *plugin_data, const char *fname, const struct stat *st,
                       char **local_path)
{
    (void) st;

    return write_entry_text ((mcpeek_data_t *) plugin_data, fname, local_path);
}

/* --------------------------------------------------------------------------------------------- */

/* The name the decompiler wants: namespace and the whole chain of enclosing
   types, dot separated. */
static char *
current_type_full_name (mcpeek_data_t *data)
{
    GString *s;
    guint i;

    if (data->nesting == NULL || data->nesting->len == 0)
        return NULL;

    s = g_string_new (NULL);
    if (data->ns != NULL && *data->ns != '\0')
    {
        g_string_append (s, data->ns);
        g_string_append_c (s, '.');
    }

    for (i = 0; i < data->nesting->len; i++)
    {
        guint32 rid = g_array_index (data->nesting, guint32, i);

        /* a nested type is joined to the one that encloses it with '+'; a
           dot there names a type that does not exist */
        if (i != 0)
            g_string_append_c (s, '+');
        g_string_append (s,
                         mcpeek_meta_string (&data->meta,
                                             mcpeek_meta_col (&data->meta, MCPEEK_T_TYPEDEF, rid,
                                                              MCPEEK_TYPEDEF_NAME)));
    }

    return g_string_free (s, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/* The names of the types the panel stands in, outermost first. */
static GPtrArray *
type_chain_names (const mcpeek_data_t *data)
{
    GPtrArray *names;
    guint i;

    names = g_ptr_array_new_with_free_func (g_free);
    for (i = 0; i < data->nesting->len; i++)
    {
        guint32 rid = g_array_index (data->nesting, guint32, i);

        g_ptr_array_add (
            names,
            g_strdup (mcpeek_meta_string (
                &data->meta,
                mcpeek_meta_col (&data->meta, MCPEEK_T_TYPEDEF, rid, MCPEEK_TYPEDEF_NAME))));
    }

    return names;
}

/* --------------------------------------------------------------------------------------------- */

/* The type nested in @outer that is called @name, or 0. */
static guint32
nested_typedef_named (const mcpeek_meta_t *meta, guint32 outer, const char *name)
{
    guint32 i, n;

    n = mcpeek_meta_rows (meta, MCPEEK_T_NESTEDCLASS);
    for (i = 1; i <= n; i++)
        if (mcpeek_meta_col (meta, MCPEEK_T_NESTEDCLASS, i, MCPEEK_NESTED_ENCLOSING) == outer)
        {
            guint32 in = mcpeek_meta_col (meta, MCPEEK_T_NESTEDCLASS, i, MCPEEK_NESTED_NESTED);

            if (strcmp (
                    mcpeek_meta_string (
                        meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, in, MCPEEK_TYPEDEF_NAME)),
                    name)
                == 0)
                return in;
        }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

/* Find the chain again in metadata just read: by namespace and names, as a
   row number means nothing across a rebuild.  The nesting is set on
   success and left empty otherwise. */
static gboolean
resolve_type_chain (mcpeek_data_t *data, const char *ns, const GPtrArray *names)
{
    const mcpeek_meta_t *meta = &data->meta;
    guint32 rid = 0, i, n;
    guint k;

    g_array_set_size (data->nesting, 0);
    if (names->len == 0)
        return FALSE;

    n = mcpeek_meta_rows (meta, MCPEEK_T_TYPEDEF);
    for (i = 1; i <= n && rid == 0; i++)
    {
        const char *name, *type_ns;

        if (type_is_nested (meta, i))
            continue;
        name = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAME));
        type_ns = mcpeek_meta_string (
            meta, mcpeek_meta_col (meta, MCPEEK_T_TYPEDEF, i, MCPEEK_TYPEDEF_NAMESPACE));
        if (strcmp (name, g_ptr_array_index (names, 0)) == 0
            && strcmp (type_ns, ns != NULL ? ns : "") == 0)
            rid = i;
    }

    for (k = 1; rid != 0 && k < names->len; k++)
    {
        g_array_append_val (data->nesting, rid);
        rid = nested_typedef_named (meta, rid, g_ptr_array_index (names, k));
    }

    if (rid == 0)
    {
        g_array_set_size (data->nesting, 0);
        return FALSE;
    }

    g_array_append_val (data->nesting, rid);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static char *
assembly_path (const mcpeek_data_t *data)
{
    if (data->asm_name == NULL)
        return NULL;

    return g_build_filename (data->root_dir, data->asm_name, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

/* "Scale(int) : int" names the member "Scale". */
static char *
member_simple_name (const char *label)
{
    const char *end;

    end = strpbrk (label, "( ");
    if (end == NULL)
        return g_strdup (label);

    return g_strndup (label, (gsize) (end - label));
}

/* --------------------------------------------------------------------------------------------- */

static void
view_temp_file (const char *path, int line)
{
    vfs_path_t *vpath;

    vpath = vfs_path_from_str (path);
    (void) mcview_viewer (NULL, vpath, line, 0, 0);
    vfs_path_free (vpath, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_view (void *plugin_data, const char *fname, const struct stat *st, gboolean plain_view)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;
    const mcpeek_entry_t *e;
    char *local = NULL;

    (void) st;

    e = entry_find (data, fname);
    if (e == NULL || e->is_dir)
        return MC_PPR_NOT_SUPPORTED;

    /* Shift-F3 asks for what is really there; F3 asks for what is readable */
    if (!plain_view && data->level == LEVEL_TYPE && mcpeek_cs_command () != NULL)
    {
        char *full, *asm_file, *text;
        GError *error = NULL;

        full = current_type_full_name (data);
        asm_file = assembly_path (data);

        text = full != NULL && asm_file != NULL ? mcpeek_cs_type (asm_file, full, &error) : NULL;
        g_free (full);
        g_free (asm_file);

        if (text != NULL)
        {
            gboolean ok;
            char *member;
            int line;

            member = member_simple_name (e->name);
            line = mcpeek_cs_find_member (text, member);
            g_free (member);

            ok = mc_pp_write_temp_file ("mc-mcpeek-XXXXXX", text, -1, &local);
            g_free (text);

            if (ok)
            {
                mc_pp_rename_with_ext (&local, "mcpeek.cs");
                view_temp_file (local, line);
                unlink (local);
                g_free (local);
                return MC_PPR_OK;
            }
        }
        else if (error != NULL)
        {
            if (data->host != NULL && data->host->message != NULL && !mc_pp_quiet_messages ())
                data->host->message (data->host, D_ERROR, _ ("mcpeek"), error->message);
            g_error_free (error);
        }
    }

    if (write_entry_text (data, fname, &local) != MC_PPR_OK)
        return MC_PPR_NOT_SUPPORTED;

    view_temp_file (local, 0);
    unlink (local);
    g_free (local);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* F5 on a type or a namespace writes decompiled sources; on an assembly of
   the set it copies the file itself. */
static mc_pp_result_t
mcpeek_copy_to_local (void *plugin_data, const char *fname, const char *local_path)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;
    const mcpeek_entry_t *e;
    GError *error = NULL;
    char *asm_file;
    gboolean ok;

    e = entry_find (data, fname);
    if (e == NULL)
        return MC_PPR_NOT_SUPPORTED;

    /* a reference is another assembly, and behind it the whole dependency
       graph: not something a copy should walk into */
    if (strcmp (e->kind, "refs") == 0 || strcmp (e->kind, "ref") == 0)
        return MC_PPR_FAILED;

    if (strcmp (e->kind, "asm") == 0)
    {
        char *src, *contents;
        gsize len;

        src = g_build_filename (data->root_dir, fname, (char *) NULL);
        ok = g_file_get_contents (src, &contents, &len, &error);
        g_free (src);

        if (ok)
        {
            ok = g_file_set_contents (local_path, contents, len, &error);
            g_free (contents);
        }

        if (!ok)
        {
            g_clear_error (&error);
            return MC_PPR_FAILED;
        }
        return MC_PPR_OK;
    }

    /* a file entry is its text and nothing else: what could not be had is
       a failure, not a file with a note in it */
    if (!e->is_dir && strcmp (e->kind, "res") != 0)
    {
        char *text;

        text = entry_text (data, e, &error);
        if (text == NULL)
        {
            report_error (data, error);
            g_clear_error (&error);
            return MC_PPR_FAILED;
        }

        ok = g_file_set_contents (local_path, text, -1, &error);
        g_free (text);
        if (!ok)
        {
            report_error (data, error);
            g_clear_error (&error);
            return MC_PPR_FAILED;
        }
        return MC_PPR_OK;
    }

    if (!e->is_dir || mcpeek_cs_command () == NULL)
        return MC_PPR_NOT_SUPPORTED;

    /* a type is one call of the decompiler; a namespace or Properties is a
       listing, and the core copies it entry by entry */
    if (strcmp (e->kind, "type") != 0)
        return MC_PPR_NOT_SUPPORTED;

    asm_file = assembly_path (data);
    if (asm_file == NULL)
        return MC_PPR_NOT_SUPPORTED;

    if (g_mkdir_with_parents (local_path, 0755) != 0)
    {
        g_free (asm_file);
        return MC_PPR_FAILED;
    }

    {
        char *full, *text, *file, *base;
        guint32 rid = e->token & 0x00FFFFFFu;

        g_array_append_val (data->nesting, rid);
        full = current_type_full_name (data);
        g_array_set_size (data->nesting, data->nesting->len - 1);

        text = full != NULL ? mcpeek_cs_type (asm_file, full, &error) : NULL;
        g_free (asm_file);
        g_free (full);

        if (text == NULL)
        {
            report_error (data, error);
            g_clear_error (&error);
            return MC_PPR_FAILED;
        }

        base = g_strconcat (e->name, ".cs", (char *) NULL);
        file = g_build_filename (local_path, base, (char *) NULL);
        g_free (base);

        ok = g_file_set_contents (file, text, -1, &error);
        g_free (file);
        g_free (text);

        if (!ok)
        {
            report_error (data, error);
            g_clear_error (&error);
        }

        return ok ? MC_PPR_OK : MC_PPR_FAILED;
    }
}

/* --------------------------------------------------------------------------------------------- */

/* The whole assembly as a compilable project with its PDB, into a directory
   the user names: the one thing the panel's own tree cannot stand for. */
static mc_pp_result_t
mcpeek_export_project (mcpeek_data_t *data)
{
    char *asm_file;
    char *suggested;
    char *dest;
    GError *error = NULL;
    gboolean ok;

    if (mcpeek_cs_command () == NULL)
        return MC_PPR_NOT_SUPPORTED;

    asm_file = assembly_path (data);
    if (asm_file == NULL)
        return MC_PPR_NOT_SUPPORTED;

    /* a directory named after the assembly, beside it: the sources must not
       land among the binaries, and a relative root would depend on where mc
       happens to stand */
    {
        char *root, *base, *dot;

        root = g_canonicalize_filename (data->root_dir, NULL);
        base = g_strdup (data->asm_name);
        dot = strrchr (base, '.');
        if (dot != NULL)
            *dot = '\0';
        suggested = g_strconcat (root, PATH_SEP_STR, base, ".src", (char *) NULL);
        g_free (root);
        g_free (base);
    }

    dest = input_expand_dialog (_ ("Export project"), _ ("Export the assembly as a project to:"),
                                "mcpeek-export", suggested,
                                INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD);
    g_free (suggested);
    if (dest == NULL || *dest == '\0')
    {
        g_free (dest);
        g_free (asm_file);
        return MC_PPR_SKIPPED;
    }

    ok = g_mkdir_with_parents (dest, 0755) == 0 && mcpeek_cs_export (asm_file, dest, TRUE, &error);
    g_free (dest);
    g_free (asm_file);

    if (!ok)
    {
        report_error (data, error);
        g_clear_error (&error);
        return MC_PPR_SKIPPED;
    }

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcpeek_configure (void)
{
    char *command;
    char *version;
    char *new_command = NULL;
    quick_widget_t quick_widgets[] = {
        QUICK_LABEL (N_ ("Decompiler command (empty: look for ilspycmd)"), NULL),
        QUICK_INPUT (mcpeek_cs_command () != NULL ? mcpeek_cs_command () : "", "mcpeek-cmd",
                     &new_command, NULL, FALSE, FALSE, INPUT_COMPLETE_FILENAMES),
        QUICK_SEPARATOR (TRUE),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END,
    };
    WRect r = { -1, -1, 0, 64 };
    quick_dialog_t qdlg = {
        .rect = r,
        .title = N_ (".NET assembly browser"),
        .help = "[mcpeek]",
        .widgets = quick_widgets,
        .callback = NULL,
        .mouse_callback = NULL,
    };

    version = mcpeek_cs_version ();
    if (version != NULL)
    {
        message (D_NORMAL, _ ("mcpeek"), _ ("Decompiler: %s"), version);
        g_free (version);
    }

    if (quick_dialog (&qdlg) != B_ENTER || new_command == NULL)
    {
        g_free (new_command);
        return;
    }

    command = g_strstrip (new_command);

    {
        mc_config_t *cfg;
        char *path;

        path = g_build_filename (mc_config_get_path (), "mcpeek.ini", (char *) NULL);
        cfg = mc_config_init (path, FALSE);
        if (cfg != NULL)
        {
            mc_config_set_string (cfg, "mcpeek", "decompiler", command);
            mc_config_save_file (cfg, NULL);
            mc_config_deinit (cfg);
        }
        g_free (path);
    }

    g_free (new_command);

    /* the next lookup must see the new value */
    mcpeek_cs_shutdown ();
}

/* --------------------------------------------------------------------------------------------- */

static void
mcpeek_shutdown (void)
{
    mcpeek_cs_shutdown ();
    mcpeek_resolve_shutdown ();
}

/* --------------------------------------------------------------------------------------------- */

static void
uses_clear (mcpeek_data_t *data)
{
    if (data->uses != NULL)
    {
        mcpeek_uses_free (data->uses);
        data->uses = NULL;
    }
    if (data->target != NULL)
    {
        mcpeek_target_free (data->target);
        data->target = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_find_uses_of_current (mcpeek_data_t *data)
{
    const GString *current;
    const mcpeek_entry_t *e;
    char **files;
    guint32 token;

    if (data->img == NULL || data->host == NULL || data->host->get_current == NULL)
        return MC_PPR_NOT_SUPPORTED;

    current = data->host->get_current (data->host);
    if (current == NULL || current->str == NULL)
        return MC_PPR_NOT_SUPPORTED;

    e = entry_find (data, current->str);
    if (e == NULL || e->token == 0)
        return MC_PPR_NOT_SUPPORTED;

    token = e->token;

    /* a type is listed as a directory, and its token is what the search wants */
    if (strcmp (e->kind, "type") != 0 && strcmp (e->kind, "method") != 0
        && strcmp (e->kind, "field") != 0)
        return MC_PPR_NOT_SUPPORTED;

    uses_clear (data);

    {
        char *path = assembly_path (data);

        data->target = mcpeek_find_target (&data->meta, path, token);
        g_free (path);
    }

    if (data->target == NULL)
        return MC_PPR_FAILED;

    if (data->host->set_hint != NULL)
        data->host->set_hint (data->host, _ ("Searching..."));

    files = searched_files (data);
    data->uses = mcpeek_find_uses (data->target, files);
    g_strfreev (files);

    if (data->uses->len == 0)
    {
        if (data->host->message != NULL)
            data->host->message (data->host, D_NORMAL, _ ("mcpeek"), _ ("No uses found"));
        uses_clear (data);
        return MC_PPR_OK;
    }

    if (data->level != LEVEL_USES)
        data->level_before = data->level;
    data->level = LEVEL_USES;

    /* in parentheses: with ncurses, refresh is a macro */
    if (data->host->refresh != NULL)
        (data->host->refresh) (data->host);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

/* Ctrl-R: the assembly is held mapped, so re-listing it would show what was
   there when it was opened.  The file is opened again instead. */
static mc_pp_result_t
mcpeek_reload (void *plugin_data)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;
    char *path;
    GPtrArray *names;
    char *ns = NULL;
    gboolean ok;

    if (data->img == NULL)
        return MC_PPR_OK;

    path = assembly_path (data);
    if (path == NULL)
        return MC_PPR_OK;

    /* where the panel stands has to survive the reopen, by name: the rows
       of a rebuilt assembly are numbered anew */
    names = type_chain_names (data);
    if (data->ns != NULL)
        ns = g_strdup (data->ns);

    uses_clear (data);
    if (data->level == LEVEL_USES)
        data->level = data->level_before;

    ok = mcpeek_load_path (data, path);
    g_free (path);

    if (!ok)
    {
        g_free (ns);
        g_ptr_array_free (names, TRUE);
        return MC_PPR_FAILED;
    }

    data->ns = ns;
    if (!resolve_type_chain (data, ns, names) && data->level == LEVEL_TYPE)
        data->level = ns != NULL && *ns != '\0' ? LEVEL_NAMESPACE : LEVEL_ASSEMBLY;
    g_ptr_array_free (names, TRUE);

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_handle_key (void *plugin_data, int key)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;

    if (!keys_resolved)
    {
        keys_resolved = TRUE;
        key_find_uses = mc_plugin_prefs_load_hotkey ("mcpeek.ini", "keys", "find_uses", "alt-f7",
                                                     ALT (KEY_F (7)), NULL);
        key_export_project = mc_plugin_prefs_load_hotkey ("mcpeek.ini", "keys", "export_project",
                                                          "alt-f5", ALT (KEY_F (5)), NULL);
    }

    if (key_find_uses != 0 && key == key_find_uses)
        return mcpeek_find_uses_of_current (data);
    if (key_export_project != 0 && key == key_export_project)
        return mcpeek_export_project (data);

    /* Ctrl-PgDn on a file entry would hand its text to a nested panel; on an
       assembly or a search result it walks in like Enter does */
    if (key == CK_CdChild && data->host != NULL && data->host->get_current != NULL)
    {
        const GString *current = data->host->get_current (data->host);

        if (current != NULL && current->str != NULL
            && entry_is_place (entry_find (data, current->str)))
        {
            /* a failed load has been reported; the listing stays as it is */
            (void) mcpeek_chdir (data, current->str);
            return MC_PPR_OK;
        }
    }

    return MC_PPR_NOT_SUPPORTED;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
mcpeek_get_title (void *plugin_data)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;

    return data->title != NULL ? data->title : "peek:";
}

/* --------------------------------------------------------------------------------------------- */

static const mc_panel_column_t *
mcpeek_get_columns (void *plugin_data, size_t *count)
{
    (void) plugin_data;

    *count = G_N_ELEMENTS (mcpeek_columns);
    return mcpeek_columns;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
mcpeek_get_column_value (void *plugin_data, const char *fname, const char *column_id)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;
    const mcpeek_entry_t *e;

    e = entry_find (data, fname);
    if (e == NULL)
        return "";

    if (strcmp (column_id, "kind") == 0)
        return e->kind;

    if (strcmp (column_id, "asm") == 0)
    {
        char *base;

        if (e->aux == NULL || strcmp (e->kind, "use") != 0)
            return "";

        base = g_path_get_basename (e->aux);
        g_strlcpy (data->column_buf, base, sizeof (data->column_buf));
        g_free (base);
        return data->column_buf;
    }

    if (strcmp (column_id, "token") == 0)
    {
        if (e->token == 0)
            return "";
        g_snprintf (data->column_buf, sizeof (data->column_buf), "%08x", e->token);
        return data->column_buf;
    }

    return "";
}

/* --------------------------------------------------------------------------------------------- */

static const char *
mcpeek_get_default_format (void *plugin_data)
{
    (void) plugin_data;

    return "half type name | kind | token | size";
}

/* --------------------------------------------------------------------------------------------- */

static char *
mcpeek_get_location (void *plugin_data)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;

    if (data->asm_name == NULL)
        return g_strconcat ("peek:", data->root_dir, (char *) NULL);

    return g_strconcat ("peek:", data->root_dir, PATH_SEP_STR, data->asm_name, (char *) NULL);
}

/* --------------------------------------------------------------------------------------------- */

static mc_pp_result_t
mcpeek_get_help_info (void *plugin_data, const char **filename, const char **node)
{
    static const char help_path[] = MC_PLUGIN_DIR "/mcpeek_panel.hlp";

    (void) plugin_data;

    *filename = help_path;
    *node = "mcpeek";

    return MC_PPR_OK;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
mcpeek_get_focus_name (void *plugin_data)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;

    return data->focus;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcpeek_is_file_listing (void *plugin_data)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;

    /* the set, and the levels that stand for a directory of sources */
    return data->level == LEVEL_SET || data->level == LEVEL_SRC_NS || data->level == LEVEL_SRC_PROPS
        || (data->level == LEVEL_ASSEMBLY && data->cs_mode);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcpeek_may_open_name (const char *display_name)
{
    const char *dot;

    if (display_name == NULL)
        return FALSE;

    dot = strrchr (display_name, '.');
    if (dot == NULL)
        return FALSE;

    return g_ascii_strcasecmp (dot, ".dll") == 0 || g_ascii_strcasecmp (dot, ".exe") == 0
        || g_ascii_strcasecmp (dot, ".winmd") == 0;
}

/* --------------------------------------------------------------------------------------------- */

static mcpeek_data_t *
mcpeek_data_new (mc_panel_host_t *host)
{
    mcpeek_data_t *data;

    data = g_new0 (mcpeek_data_t, 1);
    data->host = host;
    data->entries = g_ptr_array_new_with_free_func (entry_free);
    data->crumbs = g_ptr_array_new_with_free_func (g_free);
    data->asm_stack = g_ptr_array_new_with_free_func (g_free);
    data->nesting = g_array_new (FALSE, FALSE, sizeof (guint32));

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void *
mcpeek_open (mc_panel_host_t *host, const char *open_path)
{
    mcpeek_data_t *data;
    char *path;
    vfs_path_t *vpath;
    struct stat st;
    gboolean is_file;

    path = g_strdup (open_path != NULL && *open_path != '\0' ? open_path : ".");
    if (strncmp (path, "peek:", 5) == 0)
    {
        char *stripped = g_strdup (path + 5);

        g_free (path);
        path = stripped;
    }

    data = mcpeek_data_new (host);

    vpath = vfs_path_from_str (path);
    is_file = mc_stat (vpath, &st) == 0 && S_ISREG (st.st_mode);
    vfs_path_free (vpath, TRUE);

    if (is_file)
    {
        char *dir = g_path_get_dirname (path);
        char *base = g_path_get_basename (path);

        data->root_dir = dir;
        if (!mcpeek_load (data, base))
        {
            g_free (base);
            g_free (path);
            mcpeek_close (data);
            return NULL;
        }
        g_free (base);
        data->level = LEVEL_ASSEMBLY;
    }
    else
    {
        data->root_dir = g_strdup (path);
        data->level = LEVEL_SET;
    }

    g_free (path);
    rebuild_title (data);

    return data;
}

/* --------------------------------------------------------------------------------------------- */

/* Copy a stream that has no file behind it into one, so that it can be
   mapped.  The whole assembly is read: metadata sits at the end of the file
   as often as not, and a panel walks it in no particular order. */
static gboolean
drain_stream (mc_pp_input_stream_t *stream, char **path)
{
    void *handle = NULL;
    GError *error = NULL;
    int fd;
    gboolean ok = TRUE;

    *path = NULL;

    if (stream->ops == NULL || stream->ops->open == NULL || stream->ops->read == NULL)
        return FALSE;

    if (stream->ops->open (stream, &handle, &error) != MC_PPR_OK)
    {
        g_clear_error (&error);
        return FALSE;
    }

    fd = g_file_open_tmp ("mc-mcpeek-XXXXXX", path, &error);
    if (fd == -1)
    {
        g_clear_error (&error);
        if (stream->ops->close != NULL)
            stream->ops->close (stream, handle);
        return FALSE;
    }

    while (ok)
    {
        char buf[64 * 1024];
        gssize n;

        n = stream->ops->read (stream, handle, buf, sizeof (buf), &error);
        if (n < 0)
        {
            g_clear_error (&error);
            ok = FALSE;
            break;
        }
        if (n == 0)
            break;

        if (write (fd, buf, (size_t) n) != n)
            ok = FALSE;
    }

    close (fd);
    if (stream->ops->close != NULL)
        stream->ops->close (stream, handle);

    if (!ok)
    {
        unlink (*path);
        MC_PTR_FREE (*path);
    }

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static void *
mcpeek_open_stream (mc_panel_host_t *host, const char *display_name, mc_pp_input_stream_t *stream)
{
    const char *local;
    gboolean is_temp = FALSE;
    char *drained = NULL;
    mcpeek_data_t *data;

    /* An assembly is read by mapping it, so it has to be a file.  A stream
       from an archive, a container or a remote panel has none, and is copied
       out here; that copy lives as long as the panel does. */
    local = mc_pp_input_stream_local_path (stream, &is_temp);
    if (local == NULL)
    {
        if (!drain_stream (stream, &drained))
            return NULL;
        local = drained;
    }

    data = (mcpeek_data_t *) mcpeek_open (host, local);
    if (data == NULL)
    {
        if (drained != NULL)
        {
            unlink (drained);
            g_free (drained);
        }
        return NULL;
    }

    data->stream = stream;
    data->temp_file = drained;
    if (drained != NULL && display_name != NULL && *display_name != '\0')
    {
        data->stream_name = g_path_get_basename (display_name);
        rebuild_title (data);
    }

    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
mcpeek_close (void *plugin_data)
{
    mcpeek_data_t *data = (mcpeek_data_t *) plugin_data;

    if (data == NULL)
        return;

    uses_clear (data);
    mcpeek_unload (data);

    if (data->stream != NULL)
        mc_pp_input_stream_free (data->stream);
    if (data->temp_file != NULL)
    {
        unlink (data->temp_file);
        g_free (data->temp_file);
    }
    g_free (data->stream_name);

    if (data->entries != NULL)
        g_ptr_array_free (data->entries, TRUE);
    if (data->crumbs != NULL)
        g_ptr_array_free (data->crumbs, TRUE);
    if (data->asm_stack != NULL)
        g_ptr_array_free (data->asm_stack, TRUE);
    g_free (data->focus);
    if (data->nesting != NULL)
        g_array_free (data->nesting, TRUE);

    g_free (data->root_dir);
    g_free (data->title);
    g_free (data);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const mc_panel_plugin_t *mc_panel_plugin_register (void);

const mc_panel_plugin_t *
mc_panel_plugin_register (void)
{
    return &mcpeek_plugin;
}

/* --------------------------------------------------------------------------------------------- */
