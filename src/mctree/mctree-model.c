/*
   Structured tree model for document-like content.

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

#include <config.h>

#include <string.h>

#include "src/mctree/mctree-model.h"

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mctree_node_free (mctree_node_t *node)
{
    if (node == NULL)
        return;

    g_free (node->key);
    g_free (node->value);
    if (node->children != NULL)
        g_ptr_array_free (node->children, TRUE);
    g_free (node);
}

/* --------------------------------------------------------------------------------------------- */

static char *
mctree_value_preview (const char *value, gsize limit, gsize *original_len, gboolean *truncated)
{
    gsize len;

    if (original_len != NULL)
        *original_len = 0;
    if (truncated != NULL)
        *truncated = FALSE;

    if (value == NULL)
        return NULL;

    len = strlen (value);
    if (original_len != NULL)
        *original_len = len;

    if (limit == 0 || len <= limit)
        return g_strdup (value);

    if (truncated != NULL)
        *truncated = TRUE;

    if (limit <= 3)
        return g_strndup (value, limit);

    return g_strdup_printf ("%.*s...", (int) (limit - 3), value);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mctree_node_is_transparent_container (const mctree_node_t *node)
{
    if (node == NULL || node->key != NULL || node->value != NULL || node->parent == NULL)
        return FALSE;

    if (node->type != MCTREE_NODE_OBJECT && node->type != MCTREE_NODE_ARRAY)
        return FALSE;

    return node->parent->type == MCTREE_NODE_ROOT || node->parent->type == MCTREE_NODE_FIELD
        || node->parent->type == MCTREE_NODE_ITEM;
}

/* --------------------------------------------------------------------------------------------- */

static void mctree_model_add_visible_transparent_children (const mctree_node_t *node, int depth,
                                                           GArray *rows);
static void mctree_model_add_visible_children (const mctree_node_t *node, int depth, GArray *rows);

static void
mctree_model_add_visible_node (const mctree_node_t *node, int depth, GArray *rows)
{
    mctree_visible_row_t row;

    if (node->filter_hidden)
        return;

    if (mctree_node_is_transparent_container (node))
    {
        mctree_model_add_visible_transparent_children (node, depth, rows);
        return;
    }

    row.node = (mctree_node_t *) node;
    row.depth = depth;
    g_array_append_val (rows, row);
    mctree_model_add_visible_children (node, depth + 1, rows);
}

/* --------------------------------------------------------------------------------------------- */

static void
mctree_model_add_visible_transparent_children (const mctree_node_t *node, int depth, GArray *rows)
{
    guint i;

    if (node == NULL || node->children == NULL)
        return;

    for (i = 0; i < node->children->len; i++)
        mctree_model_add_visible_node (g_ptr_array_index (node->children, i), depth, rows);
}

/* --------------------------------------------------------------------------------------------- */

static void
mctree_model_add_visible_children (const mctree_node_t *node, int depth, GArray *rows)
{
    if (node == NULL || node->children == NULL || !node->expanded)
        return;

    mctree_model_add_visible_transparent_children (node, depth, rows);
}

/* --------------------------------------------------------------------------------------------- */

static void
mctree_node_expand_to_depth (mctree_node_t *node, int current_depth, int target_depth)
{
    guint i;

    if (node == NULL)
        return;

    node->expanded = current_depth < target_depth;

    if (node->children == NULL)
        return;

    for (i = 0; i < node->children->len; i++)
        mctree_node_expand_to_depth (g_ptr_array_index (node->children, i), current_depth + 1,
                                     target_depth);
}

/* --------------------------------------------------------------------------------------------- */

/* Mark the nodes the filter keeps and open the way to them: a node stays when
   it matches, when a match is somewhere below it, or when it sits inside a
   match (a matched node keeps its whole subtree to browse).  Returns TRUE when
   this subtree holds a match. */
static gboolean
mctree_node_filter_mark (mctree_node_t *node, mctree_node_match_fn match, void *user_data,
                         gboolean inside_hit, guint *hits)
{
    gboolean hit_below = FALSE;
    guint i;

    node->filter_hit = node->parent != NULL && match (node, user_data);
    if (node->filter_hit)
        (*hits)++;

    if (node->children != NULL)
        for (i = 0; i < node->children->len; i++)
            if (mctree_node_filter_mark (g_ptr_array_index (node->children, i), match, user_data,
                                         inside_hit || node->filter_hit, hits))
                hit_below = TRUE;

    node->filter_hidden = !node->filter_hit && !hit_below && !inside_hit;

    /* the path down to a match is opened, the match itself keeps its own state */
    if (hit_below)
        node->expanded = TRUE;

    return node->filter_hit || hit_below;
}

/* --------------------------------------------------------------------------------------------- */

static guint
mctree_node_descendant_count_int (const mctree_node_t *node)
{
    guint i, count;

    if (node == NULL || node->children == NULL)
        return 0;

    count = 0;
    for (i = 0; i < node->children->len; i++)
    {
        mctree_node_t *child = g_ptr_array_index (node->children, i);

        if (!mctree_node_is_transparent_container (child))
            count++;
        count += mctree_node_descendant_count_int (child);
    }

    return count;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mctree_model_t *
mctree_model_new (gsize scalar_preview_limit)
{
    mctree_model_t *model;

    model = g_new0 (mctree_model_t, 1);
    model->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) mctree_node_free);
    model->scalar_preview_limit =
        (scalar_preview_limit == 0) ? MCTREE_DEFAULT_SCALAR_PREVIEW_LIMIT : scalar_preview_limit;

    return model;
}

/* --------------------------------------------------------------------------------------------- */

void
mctree_model_free (mctree_model_t *model)
{
    if (model == NULL)
        return;

    if (model->nodes != NULL)
        g_ptr_array_free (model->nodes, TRUE);
    g_free (model);
}

/* --------------------------------------------------------------------------------------------- */

mctree_node_t *
mctree_model_add_node (mctree_model_t *model, mctree_node_t *parent, mctree_node_type_t type,
                       const char *key, const char *value)
{
    mctree_node_t *node;

    if (model == NULL)
        return NULL;

    node = g_new0 (mctree_node_t, 1);
    node->type = type;
    node->key = g_strdup (key);
    node->value = mctree_value_preview (value, model->scalar_preview_limit,
                                        &node->original_value_len, &node->value_truncated);
    node->expanded = FALSE;
    node->parent = parent;
    /* Leaves are the bulk of a tree, so the child array is allocated on demand;
       every reader treats a NULL children as "no children". */
    node->children = NULL;

    g_ptr_array_add (model->nodes, node);

    if (parent != NULL)
    {
        if (parent->children == NULL)
            parent->children = g_ptr_array_new ();
        g_ptr_array_add (parent->children, node);
    }
    else if (model->root == NULL)
        model->root = node;

    return node;
}

/* --------------------------------------------------------------------------------------------- */

void
mctree_model_expand_to_depth (mctree_model_t *model, int depth)
{
    if (model == NULL || model->root == NULL)
        return;

    if (depth < 0)
        depth = 0;

    mctree_node_expand_to_depth (model->root, 0, depth);
}

/* --------------------------------------------------------------------------------------------- */

GArray *
mctree_model_build_visible_rows (const mctree_model_t *model)
{
    GArray *rows;

    rows = g_array_new (FALSE, FALSE, sizeof (mctree_visible_row_t));

    if (model == NULL || model->root == NULL)
        return rows;

    mctree_model_add_visible_children (model->root, 0, rows);
    return rows;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Keep only the nodes the predicate matches, with the path to each of them and
 * everything below them.  The root is never matched: it carries no text.
 *
 * @return the number of matched nodes.
 */
guint
mctree_model_filter_apply (mctree_model_t *model, mctree_node_match_fn match, void *user_data)
{
    guint hits = 0;

    if (model == NULL || model->root == NULL || match == NULL)
        return 0;

    model->filter_on = TRUE;
    mctree_node_filter_mark (model->root, match, user_data, FALSE, &hits);
    model->root->filter_hidden = FALSE;

    return hits;
}

/* --------------------------------------------------------------------------------------------- */

void
mctree_model_filter_clear (mctree_model_t *model)
{
    guint i;

    if (model == NULL || !model->filter_on)
        return;

    model->filter_on = FALSE;

    for (i = 0; i < model->nodes->len; i++)
    {
        mctree_node_t *node = g_ptr_array_index (model->nodes, i);

        node->filter_hit = FALSE;
        node->filter_hidden = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

guint
mctree_node_child_count (const mctree_node_t *node)
{
    guint i;
    guint count = 0;

    if (node == NULL || node->children == NULL)
        return 0;

    for (i = 0; i < node->children->len; i++)
    {
        mctree_node_t *child = g_ptr_array_index (node->children, i);

        if (mctree_node_is_transparent_container (child))
            count += mctree_node_child_count (child);
        else
            count++;
    }

    return count;
}

/* --------------------------------------------------------------------------------------------- */

guint
mctree_node_descendant_count (const mctree_node_t *node)
{
    return mctree_node_descendant_count_int (node);
}

/* --------------------------------------------------------------------------------------------- */

const char *
mctree_node_type_name (mctree_node_type_t type)
{
    switch (type)
    {
    case MCTREE_NODE_ROOT:
        return "root";
    case MCTREE_NODE_OBJECT:
        return "object";
    case MCTREE_NODE_ARRAY:
        return "array";
    case MCTREE_NODE_FIELD:
        return "field";
    case MCTREE_NODE_ITEM:
        return "item";
    case MCTREE_NODE_SCALAR:
        return "scalar";
    case MCTREE_NODE_ELEMENT:
        return "element";
    case MCTREE_NODE_ATTRIBUTE:
        return "attribute";
    case MCTREE_NODE_TEXT:
        return "text";
    default:
        return "unknown";
    }
}
