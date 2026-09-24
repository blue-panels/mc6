/*
   Tests for mctree model.

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

#define TEST_SUITE_NAME "/src/mctree-model"

#include <string.h>

#include "tests/mctest.h"

#include "src/mctree/mctree-model.h"

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_visible_rows_follow_expanded_state)
{
    mctree_model_t *model;
    mctree_node_t *root;
    mctree_node_t *field;
    GArray *rows;

    model = mctree_model_new (0);
    root = mctree_model_add_node (model, NULL, MCTREE_NODE_ROOT, "JSON", NULL);
    field = mctree_model_add_node (model, root, MCTREE_NODE_FIELD, "name", NULL);
    mctree_model_add_node (model, field, MCTREE_NODE_SCALAR, NULL, "value");

    rows = mctree_model_build_visible_rows (model);
    ck_assert_uint_eq (rows->len, 0);
    g_array_free (rows, TRUE);

    mctree_model_expand_to_depth (model, 2);
    rows = mctree_model_build_visible_rows (model);
    ck_assert_uint_eq (rows->len, 2);
    ck_assert_ptr_eq (g_array_index (rows, mctree_visible_row_t, 0).node, field);
    ck_assert_int_eq (g_array_index (rows, mctree_visible_row_t, 1).depth, 1);

    g_array_free (rows, TRUE);
    mctree_model_free (model);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_scalar_preview_is_bounded)
{
    mctree_model_t *model;
    mctree_node_t *root;
    mctree_node_t *scalar;

    model = mctree_model_new (8);
    root = mctree_model_add_node (model, NULL, MCTREE_NODE_ROOT, "root", NULL);
    scalar = mctree_model_add_node (model, root, MCTREE_NODE_SCALAR, NULL, "abcdefghijklmnop");

    ck_assert_str_eq (scalar->value, "abcde...");
    ck_assert_uint_eq (scalar->original_value_len, 16);
    mctest_assert_true (scalar->value_truncated);

    mctree_model_free (model);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_transparent_object_containers_are_hidden)
{
    mctree_model_t *model;
    mctree_node_t *root;
    mctree_node_t *root_object;
    mctree_node_t *field;
    mctree_node_t *object;
    mctree_node_t *leaf;
    GArray *rows;

    model = mctree_model_new (0);
    root = mctree_model_add_node (model, NULL, MCTREE_NODE_ROOT, "YAML", NULL);
    root_object = mctree_model_add_node (model, root, MCTREE_NODE_OBJECT, NULL, NULL);
    field = mctree_model_add_node (model, root_object, MCTREE_NODE_FIELD, "customer", NULL);
    object = mctree_model_add_node (model, field, MCTREE_NODE_OBJECT, NULL, NULL);
    leaf = mctree_model_add_node (model, object, MCTREE_NODE_FIELD, "first_name", "Dorothy");

    root->expanded = TRUE;
    field->expanded = TRUE;

    ck_assert_uint_eq (mctree_node_child_count (field), 1);
    ck_assert_uint_eq (mctree_node_descendant_count (field), 1);

    rows = mctree_model_build_visible_rows (model);
    ck_assert_uint_eq (rows->len, 2);
    ck_assert_ptr_eq (g_array_index (rows, mctree_visible_row_t, 0).node, field);
    ck_assert_int_eq (g_array_index (rows, mctree_visible_row_t, 0).depth, 0);
    ck_assert_ptr_eq (g_array_index (rows, mctree_visible_row_t, 1).node, leaf);
    ck_assert_int_eq (g_array_index (rows, mctree_visible_row_t, 1).depth, 1);

    g_array_free (rows, TRUE);
    mctree_model_free (model);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_child_and_descendant_counts)
{
    mctree_model_t *model;
    mctree_node_t *root;
    mctree_node_t *left;

    model = mctree_model_new (0);
    root = mctree_model_add_node (model, NULL, MCTREE_NODE_ROOT, "root", NULL);
    left = mctree_model_add_node (model, root, MCTREE_NODE_OBJECT, "left", NULL);
    mctree_model_add_node (model, left, MCTREE_NODE_SCALAR, "leaf", "1");
    mctree_model_add_node (model, root, MCTREE_NODE_OBJECT, "right", NULL);

    ck_assert_uint_eq (mctree_node_child_count (root), 2);
    ck_assert_uint_eq (mctree_node_descendant_count (root), 3);

    mctree_model_free (model);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* Match on the node key, the way a filter over the rows of the tree does. */
static gboolean
key_is (const mctree_node_t *node, void *user_data)
{
    return node->key != NULL && strcmp (node->key, (const char *) user_data) == 0;
}

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_filter_keeps_matches_with_path_and_subtree)
{
    mctree_model_t *model;
    mctree_node_t *root;
    mctree_node_t *books;
    mctree_node_t *book;
    mctree_node_t *title;
    mctree_node_t *other;
    GArray *rows;

    model = mctree_model_new (0);
    root = mctree_model_add_node (model, NULL, MCTREE_NODE_ROOT, "XML", NULL);
    books = mctree_model_add_node (model, root, MCTREE_NODE_ELEMENT, "books", NULL);
    book = mctree_model_add_node (model, books, MCTREE_NODE_ELEMENT, "book", NULL);
    title = mctree_model_add_node (model, book, MCTREE_NODE_ELEMENT, "title", "Dune");
    other = mctree_model_add_node (model, books, MCTREE_NODE_ELEMENT, "magazine", NULL);

    ck_assert_uint_eq (mctree_model_filter_apply (model, key_is, (void *) "book"), 1);

    mctest_assert_true (book->filter_hit);
    mctest_assert_false (books->filter_hit);
    // the path to the match stays, and is opened
    mctest_assert_false (books->filter_hidden);
    mctest_assert_true (books->expanded);
    mctest_assert_true (root->expanded);
    // what is inside the match stays too, so the match can be browsed
    mctest_assert_false (title->filter_hidden);
    // the rest is gone
    mctest_assert_true (other->filter_hidden);

    rows = mctree_model_build_visible_rows (model);
    ck_assert_uint_eq (rows->len, 2);
    ck_assert_ptr_eq (g_array_index (rows, mctree_visible_row_t, 0).node, books);
    ck_assert_ptr_eq (g_array_index (rows, mctree_visible_row_t, 1).node, book);
    g_array_free (rows, TRUE);

    mctree_model_filter_clear (model);
    mctest_assert_false (model->filter_on);
    mctest_assert_false (other->filter_hidden);
    mctest_assert_false (book->filter_hit);

    mctree_model_free (model);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

START_TEST (test_filter_without_matches_leaves_no_rows)
{
    mctree_model_t *model;
    mctree_node_t *root;
    mctree_node_t *field;
    GArray *rows;

    model = mctree_model_new (0);
    root = mctree_model_add_node (model, NULL, MCTREE_NODE_ROOT, "JSON", NULL);
    field = mctree_model_add_node (model, root, MCTREE_NODE_FIELD, "name", "value");
    mctree_model_expand_to_depth (model, 2);

    ck_assert_uint_eq (mctree_model_filter_apply (model, key_is, (void *) "absent"), 0);
    mctest_assert_true (field->filter_hidden);

    rows = mctree_model_build_visible_rows (model);
    ck_assert_uint_eq (rows->len, 0);
    g_array_free (rows, TRUE);

    mctree_model_free (model);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_test (tc_core, test_visible_rows_follow_expanded_state);
    tcase_add_test (tc_core, test_scalar_preview_is_bounded);
    tcase_add_test (tc_core, test_transparent_object_containers_are_hidden);
    tcase_add_test (tc_core, test_child_and_descendant_counts);
    tcase_add_test (tc_core, test_filter_keeps_matches_with_path_and_subtree);
    tcase_add_test (tc_core, test_filter_without_matches_leaves_no_rows);

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
