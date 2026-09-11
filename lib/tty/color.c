/*
   Color setup.
   Interface functions.

   Copyright (C) 1994-2026
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

   Written by:
   Andrew Borodin <aborodin@vmail.ru>, 2009
   Slava Zanko <slavazanko@gmail.com>, 2009
   Egmont Koblinger <egmont@gmail.com>, 2010
   Ilia Maslakov <il.smind@gmail.com>, 2009, 2026

   This file is part of the M-Commander
   a fork of GNU Midnight Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file color.c
 *  \brief Source: color setup
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  // size_t

#include "lib/global.h"

#include "tty.h"
#include "color.h"

#include "color-internal.h"

/*** global variables ****************************************************************************/

static tty_color_pair_t tty_color_defaults = {
    .fg = NULL, .bg = NULL, .attrs = NULL, .pair_index = 0
};

/* Set if we are actually using colors */
gboolean use_colors = FALSE;

gboolean need_convert_256color = FALSE;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static GHashTable *mc_tty_color__hashtable = NULL;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mc_color__deinit (tty_color_pair_t *color)
{
    g_free (color->fg);
    g_free (color->bg);
    g_free (color->attrs);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tty_color_free_temp_cb (gpointer key, gpointer value, gpointer user_data)
{
    (void) key;
    (void) user_data;

    return ((tty_color_lib_pair_t *) value)->is_temp;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tty_color_release_temp_cb (gpointer key, gpointer value, gpointer user_data)
{
    const tty_color_lib_pair_t *mc_color_pair = (const tty_color_lib_pair_t *) value;

    (void) key;

    return mc_color_pair->is_temp && mc_color_pair->pair_index == GPOINTER_TO_SIZE (user_data);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
tty_color_get_next_cpn_cb (gpointer key, gpointer value, gpointer user_data)
{
    tty_color_lib_pair_t *mc_color_pair = (tty_color_lib_pair_t *) value;
    size_t cp = GPOINTER_TO_SIZE (user_data);

    (void) key;

    return (cp == mc_color_pair->pair_index);
}

/* --------------------------------------------------------------------------------------------- */

static size_t
tty_color_get_next__color_pair_number (void)
{
    size_t cp_count, cp;

    cp_count = g_hash_table_size (mc_tty_color__hashtable);
    for (cp = 0; cp < cp_count; cp++)
        if (g_hash_table_find (mc_tty_color__hashtable, tty_color_get_next_cpn_cb,
                               GSIZE_TO_POINTER (cp))
            == NULL)
            break;

    return cp;
}

/* --------------------------------------------------------------------------------------------- */

static tty_color_lib_pair_t *
tty_color_pair_by_index (int pair_index)
{
    if (mc_tty_color__hashtable == NULL || pair_index < 0)
        return NULL;

    return (tty_color_lib_pair_t *) g_hash_table_find (
        mc_tty_color__hashtable, tty_color_get_next_cpn_cb, GSIZE_TO_POINTER ((size_t) pair_index));
}

/* --------------------------------------------------------------------------------------------- */
/** Allocate (or find) a pair by already resolved color indices. */

static int
tty_alloc_color_pair_ints (int ifg, int ibg, int attr, gboolean is_temp)
{
    gchar *color_pair;
    tty_color_lib_pair_t *mc_color_pair;

    color_pair = g_strdup_printf ("%d.%d.%d", ifg, ibg, attr);
    if (color_pair == NULL)
        return 0;

    mc_color_pair = (tty_color_lib_pair_t *) g_hash_table_lookup (mc_tty_color__hashtable,
                                                                  (gpointer) color_pair);

    if (mc_color_pair != NULL)
    {
        g_free (color_pair);
        if (is_temp && mc_color_pair->is_temp)
            mc_color_pair->refs++;
        return mc_color_pair->pair_index;
    }

    if (need_convert_256color)
    {
        if ((ifg & FLAG_TRUECOLOR) == 0)
            ifg = convert_256color_to_truecolor (ifg);

        if ((ibg & FLAG_TRUECOLOR) == 0)
            ibg = convert_256color_to_truecolor (ibg);
    }

    mc_color_pair = g_try_new0 (tty_color_lib_pair_t, 1);
    if (mc_color_pair == NULL)
    {
        g_free (color_pair);
        return 0;
    }

    mc_color_pair->is_temp = is_temp;
    mc_color_pair->refs = is_temp ? 1 : 0;
    mc_color_pair->fg = ifg;
    mc_color_pair->bg = ibg;
    mc_color_pair->attr = attr;
    mc_color_pair->pair_index = tty_color_get_next__color_pair_number ();

    tty_color_try_alloc_lib_pair (mc_color_pair);

    g_hash_table_insert (mc_tty_color__hashtable, (gpointer) color_pair, (gpointer) mc_color_pair);

    return mc_color_pair->pair_index;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
tty_init_colors (gboolean disable, gboolean force)
{
    tty_color_init_lib (disable, force);
    mc_tty_color__hashtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_colors_done (void)
{
    tty_color_deinit_lib ();
    mc_color__deinit (&tty_color_defaults);
    g_hash_table_destroy (mc_tty_color__hashtable);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
tty_use_colors (void)
{
    return use_colors;
}

/* --------------------------------------------------------------------------------------------- */

/* The background of an allocated pair, by its name; NULL for an unknown pair.
   A color specified without a background is drawn on it. */
const char *
tty_color_pair_background (int pair_index)
{
    const tty_color_lib_pair_t *mc_color_pair;

    mc_color_pair = tty_color_pair_by_index (pair_index);
    if (mc_color_pair == NULL)
        return NULL;
    return tty_color_get_name_by_index (mc_color_pair->bg);
}

/* --------------------------------------------------------------------------------------------- */

/* The foreground of an allocated pair, by its name; NULL for an unknown pair. */
const char *
tty_color_pair_foreground (int pair_index)
{
    const tty_color_lib_pair_t *mc_color_pair;

    mc_color_pair = tty_color_pair_by_index (pair_index);
    if (mc_color_pair == NULL)
        return NULL;
    return tty_color_get_name_by_index (mc_color_pair->fg);
}

/* --------------------------------------------------------------------------------------------- */

static int
tty_color_to_rgb (int color)
{
    if (color < 0)
        return -1;  // the terminal's own
    if ((color & FLAG_TRUECOLOR) != 0)
        return color & 0xFFFFFF;

    return convert_256color_to_truecolor (color) & 0xFFFFFF;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * The two halves of an allocated pair as 24-bit colors; -1 for a half left to
 * the terminal.  For a caller that has to compute with a color rather than
 * merely name it.
 */

gboolean
tty_color_pair_rgb (int pair_index, int *fg, int *bg)
{
    const tty_color_lib_pair_t *p;

    p = tty_color_pair_by_index (pair_index);
    if (p == NULL)
        return FALSE;

    if (fg != NULL)
        *fg = tty_color_to_rgb (p->fg);
    if (bg != NULL)
        *bg = tty_color_to_rgb (p->bg);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Pair drawing one pair's text on another pair's background.
 *
 * Composing indices rather than color names keeps the attributes, which have no
 * name to be mapped back to.
 *
 * @param fg_pair_index pair the text color and the attributes come from
 * @param bg_pair_index pair the background comes from
 * @param extra_attrs attributes added on top of those, NULL for none
 * @param is_temp whether the combined pair is temporary
 * @return index of the combined pair, or -1 if either pair is unknown
 */

int
tty_color_pair_compose (int fg_pair_index, int bg_pair_index, const char *extra_attrs,
                        gboolean is_temp)
{
    const tty_color_lib_pair_t *f, *b;
    int attr;

    f = tty_color_pair_by_index (fg_pair_index);
    b = tty_color_pair_by_index (bg_pair_index);
    if (f == NULL || b == NULL)
        return -1;

    attr = f->attr | tty_attr_get_bits (extra_attrs);

    return tty_alloc_color_pair_ints (f->fg, b->bg, attr, is_temp);
}

/* --------------------------------------------------------------------------------------------- */

int
tty_try_alloc_color_pair (const tty_color_pair_t *color, gboolean is_temp)
{
    gboolean is_base;
    int ifg, ibg, attr;

    is_base = (color->fg == NULL || strcmp (color->fg, "base") == 0);
    ifg = tty_color_get_index_by_name (is_base ? tty_color_defaults.fg : color->fg);
    is_base = (color->bg == NULL || strcmp (color->bg, "base") == 0);
    ibg = tty_color_get_index_by_name (is_base ? tty_color_defaults.bg : color->bg);
    is_base = (color->attrs == NULL || strcmp (color->attrs, "base") == 0);
    attr = tty_attr_get_bits (is_base ? tty_color_defaults.attrs : color->attrs);

    return tty_alloc_color_pair_ints (ifg, ibg, attr, is_temp);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Drop one reference to a temporary pair; the pair is released when the last owner
 * lets go.  Permanent (skin) pairs and unknown indices are ignored, so a caller may
 * pass any color it stored without checking where it came from.
 */

void
tty_color_release_temp (int pair_index)
{
    tty_color_lib_pair_t *mc_color_pair;

    mc_color_pair = tty_color_pair_by_index (pair_index);
    if (mc_color_pair == NULL || !mc_color_pair->is_temp)
        return;

    if (mc_color_pair->refs > 1)
    {
        mc_color_pair->refs--;
        return;
    }

    /* The hash key is built from the color indices before the 256->truecolor
       conversion, so it cannot be reconstructed from the stored pair; drop the
       entry by its pair index instead. */
    g_hash_table_foreach_remove (mc_tty_color__hashtable, tty_color_release_temp_cb,
                                 GSIZE_TO_POINTER (mc_color_pair->pair_index));
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_free_temp (void)
{
    g_hash_table_foreach_remove (mc_tty_color__hashtable, tty_color_free_temp_cb, NULL);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_free_all (void)
{
    g_hash_table_remove_all (mc_tty_color__hashtable);
}

/* --------------------------------------------------------------------------------------------- */

void
tty_color_set_defaults (const tty_color_pair_t *color)
{
    mc_color__deinit (&tty_color_defaults);

    tty_color_defaults.fg = g_strdup (color->fg);
    tty_color_defaults.bg = g_strdup (color->bg);
    tty_color_defaults.attrs = g_strdup (color->attrs);
    tty_color_defaults.pair_index = 0;
}

/* --------------------------------------------------------------------------------------------- */
