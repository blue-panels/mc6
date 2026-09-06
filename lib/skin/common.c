/*
   Skins engine.
   Interface functions

   Copyright (C) 2009-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009
   Egmont Koblinger <egmont@gmail.com>, 2010

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
#include <stdlib.h>

#include "internal.h"
#include "lib/util.h"

#include "lib/tty/color.h"  // tty_use_256colors();

/*** global variables ****************************************************************************/

mc_skin_t mc_skin__default;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static gboolean mc_skin_is_init = FALSE;

/* The frames of the progress spinner, one string per frame, split off the skin once and kept
   until the skin is dropped. */
static char **mc_skin_spinner_frames = NULL;
static guint mc_skin_spinner_nframes = 0;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_hash_destroy_value (gpointer data)
{
    tty_color_pair_t *mc_skin_color = (tty_color_pair_t *) data;

    g_free (mc_skin_color->fg);
    g_free (mc_skin_color->bg);
    g_free (mc_skin_color->attrs);
    g_free (mc_skin_color);
}

/* --------------------------------------------------------------------------------------------- */

static char *
mc_skin_get_default_name (void)
{
    char *tmp_str;

    // from command line
    if (mc_global.tty.skin != NULL)
        return g_strdup (mc_global.tty.skin);

    // from environment variable
    tmp_str = getenv ("MC_SKIN");
    if (tmp_str != NULL)
        return g_strdup (tmp_str);

    //  from config. Or 'default' if no present in config
    return mc_config_get_string (mc_global.main_config, CONFIG_APP_SECTION, "skin", "default");
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_reinit (void)
{
    mc_skin_deinit ();
    mc_skin__default.name = mc_skin_get_default_name ();
    mc_skin__default.colors =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, mc_skin_hash_destroy_value);
}

/* --------------------------------------------------------------------------------------------- */

static void
mc_skin_try_to_load_default (void)
{
    mc_skin_reinit ();
    g_free (mc_skin__default.name);
    mc_skin__default.name = g_strdup ("default");
    if (!mc_skin_ini_file_load (&mc_skin__default))
    {
        mc_skin_reinit ();
        mc_skin_set_hardcoded_skin (&mc_skin__default);
    }
}

/* --------------------------------------------------------------------------------------------- */

/* parse the loaded config and check the terminal can show it; else the default skin, FALSE */

static gboolean
mc_skin_parse_loaded (GError **mcerror)
{
    GError *error = NULL;

    if (!mc_skin_ini_file_parse (&mc_skin__default))
    {
        mc_propagate_error (mcerror, 0,
                            _ ("Unable to parse '%s' skin.\nDefault skin has been loaded"),
                            mc_skin__default.name);

        mc_skin_try_to_load_default ();
        (void) mc_skin_ini_file_parse (&mc_skin__default);
        return FALSE;
    }
    if (mc_skin__default.have_true_colors && !tty_use_truecolors (&error))
    {
        mc_propagate_error (mcerror, 0,
                            _ ("Unable to use '%s' skin with true colors support:\n%s\nDefault "
                               "skin has been loaded"),
                            mc_skin__default.name, error->message);
        g_error_free (error);
        mc_skin_try_to_load_default ();
        (void) mc_skin_ini_file_parse (&mc_skin__default);
        return FALSE;
    }
    if (mc_skin__default.have_256_colors && !tty_use_256colors (&error))
    {
        mc_propagate_error (mcerror, 0,
                            _ ("Unable to use '%s' skin with 256 colors support:\n%s\nDefault "
                               "skin has been loaded"),
                            mc_skin__default.name, error->message);
        g_error_free (error);
        mc_skin_try_to_load_default ();
        (void) mc_skin_ini_file_parse (&mc_skin__default);
        return FALSE;
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

gboolean
mc_skin_init (const gchar *skin_override, GError **mcerror)
{
    gboolean is_good_init = TRUE;

    mc_return_val_if_error (mcerror, FALSE);

    mc_skin__default.have_256_colors = FALSE;
    mc_skin__default.have_true_colors = FALSE;

    mc_skin__default.name =
        skin_override != NULL ? g_strdup (skin_override) : mc_skin_get_default_name ();

    mc_skin__default.colors =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, mc_skin_hash_destroy_value);
    if (!mc_skin_ini_file_load (&mc_skin__default))
    {
        mc_propagate_error (mcerror, 0,
                            _ ("Unable to load '%s' skin.\nDefault skin has been loaded"),
                            mc_skin__default.name);
        mc_skin_try_to_load_default ();
        is_good_init = FALSE;
    }

    if (!mc_skin_parse_loaded (mcerror))
        is_good_init = FALSE;

    mc_skin_is_init = TRUE;
    return is_good_init;
}

/* --------------------------------------------------------------------------------------------- */

/* the engine owns @config from here on; on failure the default loads and the message names @name */

gboolean
mc_skin_load_from_config (mc_config_t *config, const char *name, GError **mcerror)
{
    gboolean is_good_init;
    char *kept;

    if (mcerror != NULL && *mcerror != NULL)
    {
        mc_config_deinit (config);
        return FALSE;
    }

    kept = g_strdup (name != NULL ? name : mc_skin__default.name);
    mc_skin_deinit ();

    mc_skin__default.have_256_colors = FALSE;
    mc_skin__default.have_true_colors = FALSE;
    mc_skin__default.name = kept;
    mc_skin__default.colors =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, mc_skin_hash_destroy_value);
    mc_skin__default.config = config;

    is_good_init = mc_skin_parse_loaded (mcerror);

    mc_skin_is_init = TRUE;
    return is_good_init;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_skin_deinit (void)
{
    tty_color_free_all ();

    MC_PTR_FREE (mc_skin__default.name);
    g_hash_table_destroy (mc_skin__default.colors);
    mc_skin__default.colors = NULL;

    MC_PTR_FREE (mc_skin__default.description);

    mc_config_deinit (mc_skin__default.config);
    mc_skin__default.config = NULL;

    g_strfreev (mc_skin_spinner_frames);
    mc_skin_spinner_frames = NULL;
    mc_skin_spinner_nframes = 0;

    mc_skin_is_init = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

gchar *
mc_skin_get (const gchar *group, const gchar *key, const gchar *default_value)
{
    if (mc_global.tty.ugly_line_drawing)
        return g_strdup (default_value);

    return mc_config_get_string_strict (mc_skin__default.config, group, key, default_value);
}

/* --------------------------------------------------------------------------------------------- */

/* Cut the spinner string into one string per frame - a frame is a single character, so a
   multi-byte glyph stays whole. */
static void
mc_skin_spinner_load (void)
{
    gchar *seq;
    const char *p;
    GPtrArray *frames;

    if (mc_skin_spinner_frames != NULL)
        return;

    seq = mc_skin_get ("core", "spinner_sequence", "|/-\\");
    if (seq == NULL || *seq == '\0' || !g_utf8_validate (seq, -1, NULL))
    {
        g_free (seq);
        seq = g_strdup ("|/-\\");
    }

    frames = g_ptr_array_new ();
    for (p = seq; *p != '\0';)
    {
        const char *next = g_utf8_next_char (p);

        g_ptr_array_add (frames, g_strndup (p, next - p));
        p = next;
    }
    g_free (seq);

    mc_skin_spinner_nframes = frames->len;
    g_ptr_array_add (frames, NULL);
    mc_skin_spinner_frames = (char **) g_ptr_array_free (frames, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * One frame of the progress spinner shown while mc is busy.
 *
 * The frames come from the "spinner_sequence" key of the skin's [core] section, one frame per
 * character, so a skin may set a plain "|/-\" or a run of multi-byte glyphs. The caller keeps
 * a counter and hands it in; the frame is picked by it, wrapping round.
 *
 * @param index the caller's step counter
 * @return the frame string, never NULL
 */

const char *
mc_skin_spinner_frame (unsigned int index)
{
    mc_skin_spinner_load ();
    return mc_skin_spinner_frames[index % mc_skin_spinner_nframes];
}

/* --------------------------------------------------------------------------------------------- */
