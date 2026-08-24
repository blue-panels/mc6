/*
   Runtime-neutral viewer controller adapter

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

/** \file runtime-viewer-source.c
 *  \brief Runtime-neutral viewer controller adapter
 */

#include <config.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include "lib/extension-runtime.h"
#include "viewer/mcviewer.h"

#include "runtime-viewer-source.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MC_RUNTIME_VIEWER_SPEC_V1_MIN_SIZE                                                         \
    (G_STRUCT_OFFSET (mc_runtime_viewer_spec_t, auto_scroll_bottom)                                \
     + sizeof (((mc_runtime_viewer_spec_t *) NULL)->auto_scroll_bottom))
#define MC_RUNTIME_VIEWER_SOURCE_V1_MIN_SIZE                                                       \
    (G_STRUCT_OFFSET (mc_runtime_viewer_source_t, stages_count)                                    \
     + sizeof (((mc_runtime_viewer_source_t *) NULL)->stages_count))
#define MC_RUNTIME_VIEWER_CONTROLLER_V1_MIN_SIZE                                                   \
    (G_STRUCT_OFFSET (mc_runtime_viewer_controller_t, spec_free)                                   \
     + sizeof (((mc_runtime_viewer_controller_t *) NULL)->spec_free))

/*** file scope type declarations ****************************************************************/

typedef struct
{
    mcview_source_controller_t native_controller;
    mc_runtime_plugin_context_t *context;
    guint64 controller_id;
    mc_runtime_viewer_controller_dispatch_t dispatch;
    mc_runtime_viewer_controller_dispatch_v2_t dispatch_v2;
    mc_runtime_viewer_spec_free_t spec_free;
    char *current_temp;
    char *draft_temp;
    char *help_file;
    char *help_node;
} runtime_viewer_controller_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static gboolean
runtime_viewer_error (const char **error, const char *value)
{
    if (error != NULL)
        *error = value;
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
runtime_viewer_dispatch (runtime_viewer_controller_t *controller,
                         mc_runtime_viewer_controller_operation_t operation,
                         const mc_runtime_viewer_viewport_t *viewport, int key,
                         mc_runtime_viewer_spec_t *spec, gboolean *handled, const char **error)
{
    if (controller->dispatch_v2 != NULL)
        return controller->dispatch_v2 (controller->context, controller->controller_id, operation,
                                        viewport, key, spec, handled, error);
    if (controller->dispatch == NULL)
        return FALSE;
    return controller->dispatch (controller->context, controller->controller_id, operation, key,
                                 spec, handled, error);
}

/* --------------------------------------------------------------------------------------------- */

static void
runtime_viewer_remove_temp (char **path)
{
    if (*path != NULL)
    {
        (void) g_unlink (*path);
        g_clear_pointer (path, g_free);
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
runtime_viewer_process_command (const mc_runtime_viewer_process_t *process)
{
    GString *command;
    guint i;

    if (process == NULL || process->argc == 0 || process->argv == NULL)
        return NULL;
    command = g_string_new ("");
    if (process->cwd != NULL)
    {
        char *quoted = g_shell_quote (process->cwd);
        g_string_append_printf (command, "cd -- %s && ", quoted);
        g_free (quoted);
    }
    for (i = 0; i < process->argc; i++)
    {
        char *quoted;

        if (process->argv[i] == NULL)
        {
            g_string_free (command, TRUE);
            return NULL;
        }
        quoted = g_shell_quote (process->argv[i]);
        if (i != 0)
            g_string_append_c (command, ' ');
        g_string_append (command, quoted);
        g_free (quoted);
    }
    return g_string_free (command, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
runtime_viewer_write_bytes (const char *bytes, gsize length, char **path)
{
    int fd;
    gsize written = 0;

    fd = g_file_open_tmp ("mc-lua-viewer-XXXXXX", path, NULL);
    if (fd == -1)
        return FALSE;
    while (written < length)
    {
        ssize_t count = write (fd, bytes + written, length - written);

        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
        {
            close (fd);
            runtime_viewer_remove_temp (path);
            return FALSE;
        }
        written += (gsize) count;
    }
    if (close (fd) != 0)
    {
        runtime_viewer_remove_temp (path);
        return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
runtime_viewer_convert_spec (const mc_runtime_viewer_spec_t *source, mcview_source_spec_t *target,
                             char **temp_path, const char *help_file, const char *default_help_node,
                             const char **error)
{
    const mc_runtime_viewer_source_t *input;

    if (source == NULL || source->struct_size < MC_RUNTIME_VIEWER_SPEC_V1_MIN_SIZE
        || source->source == NULL)
        return runtime_viewer_error (error, "invalid_source");
    input = source->source;
    if (input->struct_size < MC_RUNTIME_VIEWER_SOURCE_V1_MIN_SIZE)
        return runtime_viewer_error (error, "invalid_source");
    g_free (target->command);
    g_strfreev (target->argv);
    g_free (target->cwd);
    g_free (target->file);
    g_free (target->title);
    g_free (target->help_file);
    g_free (target->help_node);
    memset (target, 0, sizeof (*target));
    target->title = g_strdup (source->title);
    target->help_file = g_strdup (help_file);
    target->help_node =
        g_strdup (source->help_node != NULL ? source->help_node : default_help_node);
    target->auto_scroll_bottom = source->auto_scroll_bottom;
    if (source->struct_size >= G_STRUCT_OFFSET (mc_runtime_viewer_spec_t, initial_display)
            + sizeof (source->initial_display))
    {
        if (source->initial_display != MC_RUNTIME_VIEWER_DISPLAY_TEXT
            && source->initial_display != MC_RUNTIME_VIEWER_DISPLAY_TERMINAL)
            return runtime_viewer_error (error, "invalid_source");
        target->initial_terminal = source->initial_display == MC_RUNTIME_VIEWER_DISPLAY_TERMINAL;
    }
    switch (input->kind)
    {
    case MC_RUNTIME_VIEWER_SOURCE_BYTES:
        if (input->bytes_length > 64U * 1024U * 1024U
            || (input->bytes_length != 0 && input->bytes == NULL)
            || !runtime_viewer_write_bytes (input->bytes, input->bytes_length, temp_path))
            return runtime_viewer_error (error, "open_failed");
        target->file = g_strdup (*temp_path);
        return TRUE;
    case MC_RUNTIME_VIEWER_SOURCE_FILE:
        if (input->path == NULL || !g_path_is_absolute (input->path) || input->unlink_on_close)
            return runtime_viewer_error (error, "invalid_source");
        target->file = g_strdup (input->path);
        return TRUE;
    case MC_RUNTIME_VIEWER_SOURCE_PROCESS:
        if (input->process.argc == 0 || input->process.argv == NULL)
            return runtime_viewer_error (error, "invalid_source");
        target->argv = g_new0 (char *, input->process.argc + 1);
        for (guint i = 0; i < input->process.argc; i++)
        {
            if (input->process.argv[i] == NULL)
                return runtime_viewer_error (error, "invalid_source");
            target->argv[i] = g_strdup (input->process.argv[i]);
        }
        target->argc = input->process.argc;
        target->cwd = g_strdup (input->process.cwd);
        if (input->struct_size
            >= G_STRUCT_OFFSET (mc_runtime_viewer_source_t, process_stderr_separate)
                + sizeof (input->process_stderr_separate))
            target->separate_stderr = input->process_stderr_separate;
        return TRUE;
    case MC_RUNTIME_VIEWER_SOURCE_PIPELINE:
    {
        GString *pipeline;
        guint i;

        if (input->stages_count < 2 || input->stages_count > 16 || input->stages == NULL)
            return runtime_viewer_error (error, "invalid_source");
        pipeline = g_string_new ("");
        for (i = 0; i < input->stages_count; i++)
        {
            char *command;

            if (input->stages[i].kind != MC_RUNTIME_VIEWER_SOURCE_PROCESS
                || (command = runtime_viewer_process_command (&input->stages[i].process)) == NULL)
            {
                g_string_free (pipeline, TRUE);
                return runtime_viewer_error (error, "invalid_source");
            }
            if (i != 0)
                g_string_append (pipeline, " | ");
            g_string_append (pipeline, command);
            g_free (command);
        }
        target->command = g_string_free (pipeline, FALSE);
        return TRUE;
    }
    default:
        return runtime_viewer_error (error, "invalid_source");
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
runtime_viewer_options (void *data, mcview_source_spec_t *draft)
{
    runtime_viewer_controller_t *controller = data;
    mc_runtime_viewer_spec_t ignored = { .struct_size = sizeof (ignored) };
    gboolean handled = FALSE;

    (void) draft;
    return runtime_viewer_dispatch (controller, MC_RUNTIME_VIEWER_CONTROLLER_OPTIONS, NULL, 0,
                                    &ignored, &handled, NULL)
        && handled;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
runtime_viewer_prepare (void *data, mcview_source_spec_t *draft, char **err_out)
{
    runtime_viewer_controller_t *controller = data;
    mc_runtime_viewer_spec_t result = { .struct_size = sizeof (result) };
    gboolean handled = FALSE;
    const char *error = NULL;
    gboolean ok;

    runtime_viewer_remove_temp (&controller->draft_temp);
    ok = runtime_viewer_dispatch (controller, MC_RUNTIME_VIEWER_CONTROLLER_PREPARE, NULL, 0,
                                  &result, &handled, &error)
        && runtime_viewer_convert_spec (&result, draft, &controller->draft_temp,
                                        controller->help_file, controller->help_node, &error);
    if (controller->spec_free != NULL)
        controller->spec_free (controller->context, &result);
    if (!ok && err_out != NULL)
        *err_out = g_strdup (error != NULL ? error : "invalid_source");
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
runtime_viewer_prepare_viewport (void *data, mcview_source_spec_t *draft, guint columns,
                                 guint lines, char **err_out)
{
    runtime_viewer_controller_t *controller = data;
    mc_runtime_viewer_spec_t result = { .struct_size = sizeof (result) };
    mc_runtime_viewer_viewport_t viewport = {
        .struct_size = sizeof (viewport),
        .operation_version = 1,
        .columns = MAX (columns, 1U),
        .lines = MAX (lines, 1U),
    };
    gboolean handled = FALSE;
    const char *error = NULL;
    gboolean ok;

    runtime_viewer_remove_temp (&controller->draft_temp);
    ok = runtime_viewer_dispatch (controller, MC_RUNTIME_VIEWER_CONTROLLER_PREPARE_VIEWPORT,
                                  &viewport, 0, &result, &handled, &error)
        && runtime_viewer_convert_spec (&result, draft, &controller->draft_temp,
                                        controller->help_file, controller->help_node, &error);
    if (controller->spec_free != NULL)
        controller->spec_free (controller->context, &result);
    if (ok)
    {
        runtime_viewer_remove_temp (&controller->current_temp);
        controller->current_temp = g_steal_pointer (&controller->draft_temp);
    }
    else if (err_out != NULL)
        *err_out = g_strdup (error != NULL ? error : "invalid_source");
    return ok;
}

/* --------------------------------------------------------------------------------------------- */

static void
runtime_viewer_commit (void *data)
{
    runtime_viewer_controller_t *controller = data;
    mc_runtime_viewer_spec_t ignored = { .struct_size = sizeof (ignored) };
    gboolean handled = FALSE;

    runtime_viewer_remove_temp (&controller->current_temp);
    controller->current_temp = g_steal_pointer (&controller->draft_temp);
    (void) runtime_viewer_dispatch (controller, MC_RUNTIME_VIEWER_CONTROLLER_COMMIT, NULL, 0,
                                    &ignored, &handled, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
runtime_viewer_rollback (void *data)
{
    runtime_viewer_controller_t *controller = data;
    mc_runtime_viewer_spec_t ignored = { .struct_size = sizeof (ignored) };
    gboolean handled = FALSE;

    runtime_viewer_remove_temp (&controller->draft_temp);
    (void) runtime_viewer_dispatch (controller, MC_RUNTIME_VIEWER_CONTROLLER_ROLLBACK, NULL, 0,
                                    &ignored, &handled, NULL);
}

/* --------------------------------------------------------------------------------------------- */

static void
runtime_viewer_free (void *data)
{
    runtime_viewer_controller_t *controller = data;
    mc_runtime_viewer_spec_t ignored = { .struct_size = sizeof (ignored) };
    gboolean handled = FALSE;

    runtime_viewer_remove_temp (&controller->draft_temp);
    runtime_viewer_remove_temp (&controller->current_temp);
    (void) runtime_viewer_dispatch (controller, MC_RUNTIME_VIEWER_CONTROLLER_CLOSE, NULL, 0,
                                    &ignored, &handled, NULL);
    g_free (controller->help_file);
    g_free (controller->help_node);
    g_free (controller);
}

/* --------------------------------------------------------------------------------------------- */

static mcv_key_result_t
runtime_viewer_key (void *data, int key)
{
    runtime_viewer_controller_t *controller = data;
    mc_runtime_viewer_spec_t ignored = { .struct_size = sizeof (ignored) };
    gboolean handled = FALSE;

    if (!runtime_viewer_dispatch (controller, MC_RUNTIME_VIEWER_CONTROLLER_KEY, NULL, key, &ignored,
                                  &handled, NULL))
        return MCV_KEY_PASS;
    return handled ? MCV_KEY_HANDLED : MCV_KEY_PASS;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
runtime_viewer_controller_open (mc_runtime_plugin_context_t *context,
                                const mc_runtime_viewer_controller_t *source, const char **error)
{
    static const mcview_source_controller_t native_controller_template = {
        .open_options = runtime_viewer_options,
        .prepare = runtime_viewer_prepare,
        .commit = runtime_viewer_commit,
        .rollback = runtime_viewer_rollback,
        .free = runtime_viewer_free,
        .handle_key = runtime_viewer_key,
        .prepare_viewport = runtime_viewer_prepare_viewport,
        .rebuild_on_resize = FALSE,
    };
    runtime_viewer_controller_t *controller;
    mcview_source_spec_t *initial;
    mc_runtime_viewer_viewport_policy_t policy = MC_RUNTIME_VIEWER_VIEWPORT_NONE;
    WView *target = NULL;

    if (source == NULL || source->struct_size < MC_RUNTIME_VIEWER_CONTROLLER_V1_MIN_SIZE
        || source->api_version != 1 || source->spec_free == NULL)
        return runtime_viewer_error (error, "invalid_controller");
    controller = g_new0 (runtime_viewer_controller_t, 1);
    controller->native_controller = native_controller_template;
    controller->context = context;
    controller->controller_id = source->controller_id;
    controller->dispatch = source->dispatch;
    controller->spec_free = source->spec_free;
    if (source->struct_size
        >= G_STRUCT_OFFSET (mc_runtime_viewer_controller_t, help_node) + sizeof (source->help_node))
    {
        controller->help_file = g_strdup (source->help_file);
        controller->help_node = g_strdup (source->help_node);
    }
    if (source->struct_size >= G_STRUCT_OFFSET (mc_runtime_viewer_controller_t, viewport_policy)
            + sizeof (source->viewport_policy))
    {
        controller->dispatch_v2 = source->dispatch_v2;
        policy = source->viewport_policy;
        controller->native_controller.rebuild_on_resize =
            policy == MC_RUNTIME_VIEWER_VIEWPORT_REBUILD;
    }
    if (source->struct_size >= G_STRUCT_OFFSET (mc_runtime_viewer_controller_t, target_viewer)
                + sizeof (source->target_viewer)
        && mc_runtime_handle_is_valid (&source->target_viewer))
    {
        target =
            (WView *) mc_runtime_handle_resolve (&source->target_viewer, MC_RUNTIME_HANDLE_VIEWER);
        if (target == NULL)
        {
            runtime_viewer_free (controller);
            return runtime_viewer_error (error, "closed");
        }
    }
    if (policy == MC_RUNTIME_VIEWER_VIEWPORT_REBUILD)
    {
        if (source->initial_spec != NULL || controller->dispatch_v2 == NULL)
        {
            runtime_viewer_free (controller);
            return runtime_viewer_error (error, "invalid_controller");
        }
        initial = NULL;
    }
    else
    {
        if (source->initial_spec == NULL || source->dispatch == NULL)
        {
            runtime_viewer_free (controller);
            return runtime_viewer_error (error, "invalid_controller");
        }
        initial = g_new0 (mcview_source_spec_t, 1);
    }
    if (initial != NULL
        && !runtime_viewer_convert_spec (source->initial_spec, initial, &controller->current_temp,
                                         controller->help_file, controller->help_node, error))
    {
        mcview_source_spec_free (initial);
        runtime_viewer_free (controller);
        return FALSE;
    }
    if (target != NULL)
    {
        char *attach_error = NULL;
        gboolean ok;

        ok = mcview_source_controller_attach (target, initial, &controller->native_controller,
                                              controller, &attach_error);
        if (!ok && error != NULL)
            *error = "open_failed";
        g_free (attach_error);
        return ok;
    }
    if (!mcview_viewer_with_controller (initial, &controller->native_controller, controller, 0))
        return runtime_viewer_error (error, "open_failed");
    return TRUE;
}
