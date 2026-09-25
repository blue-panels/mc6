/*
   Hypertext file browser.

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.
   Copyright (C) 2026
   Ilia Maslakov <il.smind@gmail.com>

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

/** \file help.c
 *  \brief Source: hypertext file browser
 *
 *  Implements the hypertext file viewer.  The help file is markdown, and
 *  help_md.c turns it into the text painted here: a node per heading, ended
 *  by a ^D, links as a ^A, the text, a ^B, the name of the node they lead
 *  to and a ^C.
 *
 *  Laziness/widgeting attack: This file does use the dialog manager
 *  and uses mainly the dialog to achieve the help work.  there is only
 *  one specialized widget and it's only used to forward the mouse messages
 *  to the appropriate routine.
 */

#include <config.h>

#include <limits.h>  // MB_LEN_MAX
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/fileloc.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/event-types.h"

#include "keymap.h"
#include "util.h"  // file_error_message()
#include "help.h"
#include "help_md.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define MAXLINKNAME       80
#define HISTORY_SIZE      20
#define HELP_WINDOW_WIDTH MIN (80, COLS - 16)

// the help of the file manager, the file every other one falls back to
#define HELP_MAIN_FILE      "mcommander.md"

#define STRING_LINK_START   "\01"
#define STRING_LINK_POINTER "\02"
#define STRING_LINK_END     "\03"
#define STRING_NODE_END     "\04"

/*** file scope type declarations ****************************************************************/

/* Link areas for the mouse */
typedef struct Link_Area
{
    int x1, y1, x2, y2;
    const char *link_name;
} Link_Area;

/*** forward declarations (file scope functions) *************************************************/

static char *translate_file (const char *filedata);
static char *help_load (const char *filedata);
static void help_link_script_node (char **filedata, const char *node, const char *parent_node);

/*** file scope variables ************************************************************************/

static char *fdata = NULL;        // The help file shown: script_data or main_data
static char *shown_name = NULL;   // the name the file shown was opened by, for its English copy
static char *script_data = NULL;  // A script's own help file, if one was asked for
static char *main_data = NULL;    // the help of the program, for a node a script has not
static GHashTable *linked_files = NULL;  // the files links led to, by name
static int help_lines;                   // Lines in help viewer
static int history_ptr = 0;              // For the history queue
static const char *main_node;            // The main node
static const char *last_shown = NULL;    // Last byte shown in a screen
static gboolean end_of_node = FALSE;     // Flag: the last character of the node shown?
static const char *currentpoint;
static const char *selected_item;

/* The widget variables */
static WDialog *whelp;

static struct
{
    const char *page;  // Pointer to the selected page
    const char *link;  // Pointer to the selected link
    const char *data;  // The help file the page is in
} history[HISTORY_SIZE];

static GSList *link_area = NULL;
static gboolean inside_link_area = FALSE;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** A help file a link leads to, read from where the help of the program lives, in the language
 * of the user where that file has a translation.
 * @param quiet no message when the file is not there, for a file that is only probed
 * @return TRUE when the file is the one shown now
 */

static gboolean
help_open_file (const char *name, gboolean quiet, gboolean translated)
{
    char *key;
    char *path = NULL;
    char *filedata;
    char *text;

    if (linked_files == NULL)
        linked_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    // the file of the language and the English one are two files, and are kept apart
    key = g_strconcat (translated ? "" : "C/", name, (char *) NULL);

    text = g_hash_table_lookup (linked_files, key);
    if (text != NULL)
    {
        g_free (key);
        fdata = text;
        g_free (shown_name);
        shown_name = g_strdup (name);
        return TRUE;
    }

    if (translated)
    {
        char *rel;

        rel = g_build_filename (MC_HELP_DIR, name, (char *) NULL);
        filedata = load_mc_home_file (mc_global.share_data_dir, rel, &path, NULL);
        g_free (rel);
    }
    else
    {
        path = g_build_filename (mc_global.share_data_dir, MC_HELP_DIR, name, (char *) NULL);
        if (!g_file_get_contents (path, &filedata, NULL, NULL))
            filedata = NULL;
    }

    if (filedata == NULL)
    {
        if (!quiet)
            file_error_message (_ ("Cannot open file\n%s"), path);
        g_free (path);
        g_free (key);
        return FALSE;
    }
    g_free (path);

    text = help_load (filedata);
    g_free (filedata);
    if (text == NULL)
    {
        g_free (key);
        return FALSE;
    }

    g_hash_table_insert (linked_files, key, text);
    fdata = text;
    g_free (shown_name);
    shown_name = g_strdup (name);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/** The node a name stands for.  A markdown heading is known by its anchor, which is the title
 * folded to lower case with the spaces turned into dashes, so the name a dialog carries is
 * compared in that shape and keeps working whatever the heading says.
 */

static const char *
search_node (const char *data, const char *name)
{
    char *want;
    const char *inner = name;
    const char *found = NULL;
    const char *p;
    int pass;

    if (data == NULL || name == NULL || *name == '\0')
        return NULL;

    if (*inner == '[')
    {
        const char *end;

        inner++;
        end = strchr (inner, ']');
        if (end == NULL)
            return NULL;

        {
            char *dup;

            dup = g_strndup (inner, end - inner);
            want = help_md_node_id (dup);
            g_free (dup);
        }
    }
    else
        want = help_md_node_id (inner);

    // a place inside a node, then a node of its own, then a name standing in
    // the text as an old help file has it
    for (p = data; found == NULL && (p = strchr (p, CHAR_ANCHOR)) != NULL; p++)
    {
        const char *end = strchr (p + 1, CHAR_ANCHOR);
        char *dup;
        char *id;

        if (end == NULL)
            break;

        dup = g_strndup (p + 1, end - (p + 1));
        id = help_md_node_id (dup);
        g_free (dup);

        if (strcmp (id, want) == 0)
            found = end + 1;
        g_free (id);

        p = end;
    }

    for (pass = 0; pass < 2 && found == NULL; pass++)
        for (p = data; (p = strchr (p, '[')) != NULL; p++)
        {
            const char *end;
            char *dup;
            char *id;

            if (pass == 0 && (p == data || p[-1] != CHAR_NODE_END))
                continue;

            end = strchr (p, ']');
            if (end == NULL)
                break;
            if (memchr (p, '\n', end - p) != NULL)
                continue;

            dup = g_strndup (p + 1, end - p - 1);
            id = help_md_node_id (dup);
            g_free (dup);

            if (strcmp (id, want) == 0)
                found = end + 1;
            g_free (id);

            if (found != NULL)
                break;
        }

    g_free (want);

    return found;
}

/* --------------------------------------------------------------------------------------------- */
/** Searches text in the buffer pointed by start.  Search ends
 * if the CHAR_NODE_END is found in the text.
 * @return NULL on failure
 */

static const char *
search_string_node (const char *start, const char *text)
{
    if (start != NULL)
    {
        const char *d = text;
        const char *e;

        for (e = start; *e != '\0' && *e != CHAR_NODE_END; e++)
        {
            if (*d == *e)
                d++;
            else
                d = text;
            if (*d == '\0')
                return e + 1;
        }
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** The help of the program, loaded once per help session. */

static const char *
help_main_data (void)
{
    if (main_data == NULL)
    {
        char *filedata;

        filedata = load_mc_home_file (mc_global.share_data_dir, MC_HELP, NULL, NULL);
        if (filedata != NULL)
        {
            main_data = help_load (filedata);
            g_free (filedata);
        }
    }

    return main_data;
}

/* --------------------------------------------------------------------------------------------- */
/** A node no file of the language has, looked for in English: first in the English copy of the
 * file shown, then in the English manual of the file manager.  A translation that stops halfway
 * through then opens the English chapter instead of saying that there is no such node.
 * @return the node, or NULL when the English files have not got it either
 */

static const char *
help_find_english_node (const char *name)
{
    char *shown = fdata;
    char *name_shown;
    const char *node = NULL;

    name_shown = g_strdup (shown_name);

    if (name_shown != NULL && help_open_file (name_shown, TRUE, FALSE))
        node = search_node (fdata, name);

    if (node == NULL && help_open_file (HELP_MAIN_FILE, TRUE, FALSE))
        node = search_node (fdata, name);

    if (node == NULL)
        fdata = shown;

    g_free (name_shown);

    return node;
}

/* --------------------------------------------------------------------------------------------- */
/** Finds a node in the help file shown; a node it has not got is looked for in the help of the
 * program, and then in English, and the file that has it becomes the file shown.
 * @return the node, or NULL when no file has it
 */

static const char *
help_find_node (const char *name)
{
    const char *node;

    node = search_node (fdata, name);
    if (node == NULL && fdata != main_data && help_main_data () != NULL)
    {
        node = search_node (main_data, name);
        if (node != NULL)
            fdata = main_data;
    }
    if (node == NULL)
        node = help_find_english_node (name);

    return node;
}

/* --------------------------------------------------------------------------------------------- */

/** Remembers the page shown, the file it is in, and the link to select when coming back.
 * Call it before help_find_node(), which may switch the file.
 */

static void
help_history_push (const char *link)
{
    history_ptr = (history_ptr + 1) % HISTORY_SIZE;
    history[history_ptr].page = currentpoint;
    history[history_ptr].link = link;
    history[history_ptr].data = fdata;
}

/* --------------------------------------------------------------------------------------------- */

static void
help_history_pop (void)
{
    history_ptr--;
    if (history_ptr < 0)
        history_ptr = HISTORY_SIZE - 1;
}

/* --------------------------------------------------------------------------------------------- */
/** Searches the_char in the buffer pointer by start and searches
 * it can search forward (direction = 1) or backward (direction = -1)
 */

static const char *
search_char_node (const char *start, char the_char, int direction)
{
    const char *e;

    for (e = start; (*e != '\0') && (*e != CHAR_NODE_END); e += direction)
        if (*e == the_char)
            return e;

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** Returns the new current pointer when moved lines lines */

static const char *
help_node_start (const char *c)
{
    const char *p;

    for (p = c; (int) (p - fdata) > 0 && *p != CHAR_NODE_END; p--)
        ;

    if (*p != CHAR_NODE_END)
        return fdata;

    while (*p != '\0' && *p != ']')
        p++;

    return *p == '\0' ? fdata : p + 2;  // skip the newline after the name of the node
}

/* --------------------------------------------------------------------------------------------- */
/** Where the line after this one starts on the screen.  The window breaks a line that does not
 * fit its width, so a paragraph written as one long line takes several lines of the screen, and
 * counting the newlines of the text would not find them.
 * @return NULL at the end of the node
 */

static const char *
help_next_line (const char *start)
{
    const char *p = start;
    const char *word_start = NULL;
    const char *mark_start = NULL;  // the markers that open the word
    int col = 0;
    int word = 0;
    gboolean painting = TRUE;

    while (*p != '\0' && *p != CHAR_NODE_END)
    {
        const char *n = str_cget_next_char (p);

        switch (*p)
        {
        case CHAR_LINK_POINTER:
            painting = FALSE;
            break;
        case CHAR_LINK_END:
            painting = TRUE;
            break;
        case CHAR_ANCHOR:
            for (n = p + 1; *n != '\0' && *n != CHAR_ANCHOR && *n != CHAR_NODE_END; n++)
                ;
            if (*n == CHAR_ANCHOR)
                n++;
            break;
        case CHAR_VERSION:
        {
            int width = 0;

            for (n = p + 1; *n >= '0' && *n <= '9'; n++)
                width = width * 10 + *n - '0';
            if (*n == CHAR_VERSION)
                n++;
            col += width;
            break;
        }
        case CHAR_LINK_START:
        case CHAR_FONT_BOLD:
        case CHAR_FONT_ITALIC:
            if (word == 0 && mark_start == NULL)
                mark_start = p;
            break;
        case CHAR_ALTERNATE:
        case CHAR_NORMAL:
        case CHAR_FONT_NORMAL:
            break;
        default:
            if (!painting)
                break;

            if (*p == '\n')
                return n;

            if (*p != ' ' && *p != '\t')
            {
                if (word == 0)
                    word_start = p;
                word++;
                break;
            }

            // a word goes out whole, and starts the next line when it does not fit
            if (word != 0)
            {
                if (col + word >= HELP_WINDOW_WIDTH && word_start != start)
                    return word_start;
                col += word;
                word = 0;
                word_start = NULL;
            }

            if (*p == ' ')
            {
                if (col >= HELP_WINDOW_WIDTH - 1)
                    return n;
                col++;
            }
            else
            {
                col = (col / 8 + 1) * 8;
                if (col >= HELP_WINDOW_WIDTH)
                    return n;
            }
        }

        p = n;
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
move_forward2 (const char *c, int lines)
{
    const char *p = c;
    int line;

    currentpoint = c;

    for (line = 0; line < lines; line++)
    {
        const char *next;

        next = help_next_line (p);
        if (next == NULL)
            return currentpoint = c;
        p = next;
    }

    return currentpoint = p;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
move_backward2 (const char *c, int lines)
{
    GPtrArray *starts;
    const char *start;
    const char *p;

    currentpoint = c;
    start = help_node_start (c);

    starts = g_ptr_array_new ();
    for (p = start; p != NULL && p <= c; p = help_next_line (p))
        g_ptr_array_add (starts, (gpointer) p);

    if (starts->len > (guint) lines)
        currentpoint = g_ptr_array_index (starts, starts->len - 1 - lines);
    else
        currentpoint = start;

    g_ptr_array_free (starts, TRUE);

    return currentpoint;
}

/* --------------------------------------------------------------------------------------------- */

static void
move_forward (int i)
{
    if (!end_of_node)
        currentpoint = move_forward2 (currentpoint, i);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_backward (int i)
{
    currentpoint = move_backward2 (currentpoint, ++i);
}

/* --------------------------------------------------------------------------------------------- */

static void
move_to_top (void)
{
    while (((int) (currentpoint - fdata) > 0) && (*currentpoint != CHAR_NODE_END))
        currentpoint--;

    while (*currentpoint != ']')
        currentpoint++;
    currentpoint = currentpoint + 2;  // Skip the newline following the start of the node
    selected_item = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
move_to_bottom (void)
{
    while ((*currentpoint != '\0') && (*currentpoint != CHAR_NODE_END))
        currentpoint++;
    currentpoint--;
    move_backward (1);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
help_follow_link (const char *start, const char *lc_selected_item)
{
    const char *p;

    if (lc_selected_item == NULL)
        return start;

    for (p = lc_selected_item; *p != '\0' && *p != CHAR_NODE_END && *p != CHAR_LINK_POINTER; p++)
        ;
    if (*p == CHAR_LINK_POINTER)
    {
        int i;
        char link_name[MAXLINKNAME];

        char *hash;

        link_name[0] = '[';
        for (i = 1;
             *p != CHAR_LINK_END && *p != '\0' && *p != CHAR_NODE_END && i < MAXLINKNAME - 3;)
            link_name[i++] = *++p;
        link_name[i - 1] = ']';
        link_name[i] = '\0';

        // a link names a file of its own when it leads out of this one
        for (hash = strstr (link_name, ".md"); hash != NULL; hash = strstr (hash + 1, ".md"))
            if (hash[3] == '#' || hash[3] == ']')
                break;

        if (hash != NULL)
        {
            char node[MAXLINKNAME];
            gboolean have_node = hash[3] == '#';

            if (have_node)
            {
                char *end;

                node[0] = '[';
                g_strlcpy (node + 1, hash + 4, sizeof (node) - 2);
                end = strrchr (node, ']');
                if (end != NULL)
                    *end = '\0';
                strcat (node, "]");
            }

            hash[3] = '\0';  // what is left is the name of the file
            if (!help_open_file (link_name + 1, FALSE, TRUE))
                return start;

            if (have_node)
                p = help_find_node (node);
            else
            {
                // the file opens on the first node it has
                p = strchr (fdata, ']');
                if (p != NULL)
                    p++;
            }
        }
        else
            p = help_find_node (link_name);
        if (p != NULL)
        {
            p += 1;  // Skip the newline following the start of the node
            return p;
        }
    }

    // Create a replacement page with the error message
    return _ ("Help file format error\n");
}

/* --------------------------------------------------------------------------------------------- */

static const char *
select_next_link (const char *current_link)
{
    const char *p;

    if (current_link == NULL)
        return NULL;

    p = search_string_node (current_link, STRING_LINK_END);
    if (p == NULL)
        return NULL;
    p = search_string_node (p, STRING_LINK_START);
    if (p == NULL)
        return NULL;
    return p - 1;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
select_prev_link (const char *current_link)
{
    return current_link == NULL ? NULL : search_char_node (current_link - 1, CHAR_LINK_START, -1);
}

/* --------------------------------------------------------------------------------------------- */

static void
start_link_area (int x, int y, const char *link_name)
{
    Link_Area *la;

    if (inside_link_area)
        message (D_NORMAL, _ ("Warning"), "%s", _ ("Internal bug: Double start of link area"));

    // Allocate memory for a new link area
    la = g_new (Link_Area, 1);
    // Save the beginning coordinates of the link area
    la->x1 = x;
    la->y1 = y;
    // Save the name of the destination anchor
    la->link_name = link_name;
    link_area = g_slist_prepend (link_area, la);

    inside_link_area = TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
end_link_area (int x, int y)
{
    if (inside_link_area)
    {
        Link_Area *la = (Link_Area *) link_area->data;
        // Save the end coordinates of the link area
        la->x2 = x;
        la->y2 = y;
        inside_link_area = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
clear_link_areas (void)
{
    g_clear_slist (&link_area, g_free);
    inside_link_area = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static void
help_print_word (WDialog *h, GString *word, int *col, int *line, gboolean add_space)
{
    if (*line >= help_lines)
        g_string_set_size (word, 0);
    else
    {
        int w;

        w = str_term_width1 (word->str);
        if (*col + w >= HELP_WINDOW_WIDTH)
        {
            *col = 0;
            (*line)++;
        }

        if (*line >= help_lines)
            g_string_set_size (word, 0);
        else
        {
            widget_gotoyx (h, *line + 2, *col + 2);
            tty_print_string (word->str);
            g_string_set_size (word, 0);
            *col += w;
        }
    }

    if (add_space)
    {
        if (*col < HELP_WINDOW_WIDTH - 1)
        {
            tty_print_char (' ');
            (*col)++;
        }
        else
        {
            *col = 0;
            (*line)++;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static mc_tty_char_t
mc_acs_map (int c)
{
    switch (c)
    {
    case 'q':
        return mc_global.tty.ugly_line_drawing ? '-'
            : mc_global.utf8_display           ? 0x2500
                                               : MC_ACS_HLINE;
    case 'x':
        return mc_global.tty.ugly_line_drawing ? '|'
            : mc_global.utf8_display           ? 0x2502
                                               : MC_ACS_VLINE;
    case 'l':
        return mc_global.tty.ugly_line_drawing ? '+'
            : mc_global.utf8_display           ? 0x250C
                                               : MC_ACS_ULCORNER;
    case 'k':
        return mc_global.tty.ugly_line_drawing ? '+'
            : mc_global.utf8_display           ? 0x2510
                                               : MC_ACS_URCORNER;
    case 'm':
        return mc_global.tty.ugly_line_drawing ? '+'
            : mc_global.utf8_display           ? 0x2514
                                               : MC_ACS_LLCORNER;
    case 'j':
        return mc_global.tty.ugly_line_drawing ? '+'
            : mc_global.utf8_display           ? 0x2518
                                               : MC_ACS_LRCORNER;
    case 't':
        return mc_global.tty.ugly_line_drawing ? '|'
            : mc_global.utf8_display           ? 0x251C
                                               : MC_ACS_LTEE;
    case 'u':
        return mc_global.tty.ugly_line_drawing ? '|'
            : mc_global.utf8_display           ? 0x2524
                                               : MC_ACS_RTEE;
    case 'w':
        return mc_global.tty.ugly_line_drawing ? '-'
            : mc_global.utf8_display           ? 0x252C
                                               : MC_ACS_TTEE;
    case 'v':
        return mc_global.tty.ugly_line_drawing ? '-'
            : mc_global.utf8_display           ? 0x2534
                                               : MC_ACS_BTEE;
    case 'n':
        return mc_global.tty.ugly_line_drawing ? '+'
            : mc_global.utf8_display           ? 0x253C
                                               : MC_ACS_PLUS;

    default:
        return c;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
help_show (WDialog *h, const char *paint_start)
{
    gboolean painting = TRUE;
    gboolean repeat_paint;
    int active_col, active_line;  // Active link position
    char buff[MB_LEN_MAX + 1];
    GString *word;

    word = g_string_sized_new (32);

    tty_setcolor (HELP_NORMAL_COLOR);
    do
    {
        int line = 0;
        int col = 0;
        gboolean acs = FALSE;  // Flag: Is alternate character set active?
        const char *p, *n;

        active_col = 0;
        active_line = 0;

        repeat_paint = FALSE;

        clear_link_areas ();
        if ((int) (selected_item - paint_start) < 0)
            selected_item = NULL;

        p = paint_start;
        n = paint_start;
        while ((n[0] != '\0') && (n[0] != CHAR_NODE_END) && (line < help_lines))
        {
            int c;

            p = n;
            n = str_cget_next_char (p);
            memcpy (buff, p, n - p);
            buff[n - p] = '\0';

            c = (unsigned char) buff[0];
            switch (c)
            {
            case CHAR_LINK_START:
                if (selected_item == NULL)
                    selected_item = p;
                if (p != selected_item)
                    tty_setcolor (HELP_LINK_COLOR);
                else
                {
                    tty_setcolor (HELP_SLINK_COLOR);

                    // Store the coordinates of the link
                    active_col = col + 2;
                    active_line = line + 2;
                }
                start_link_area (col, line, p);
                break;
            case CHAR_LINK_POINTER:
                painting = FALSE;
                break;
            case CHAR_LINK_END:
                painting = TRUE;
                help_print_word (h, word, &col, &line, FALSE);
                end_link_area (col - 1, line);
                tty_setcolor (HELP_NORMAL_COLOR);
                break;
            case CHAR_ANCHOR:
                // a place a link leads to, of no width
                while (n[0] != '\0' && n[0] != CHAR_ANCHOR && n[0] != CHAR_NODE_END)
                    n++;
                if (n[0] == CHAR_ANCHOR)
                    n++;
                break;
            case CHAR_ALTERNATE:
                acs = TRUE;
                break;
            case CHAR_NORMAL:
                acs = FALSE;
                break;
            case CHAR_VERSION:
            {
                // the field says how many columns it takes, so a version
                // longer than that paints over what follows it rather than
                // pushing it aside
                int width = 0;

                while (n[0] >= '0' && n[0] <= '9')
                    width = width * 10 + *n++ - '0';

                if (n[0] == CHAR_VERSION)
                    n++;
                else
                    width = str_term_width1 (mc_global.mc_version);

                widget_gotoyx (h, line + 2, col + 2);
                tty_print_string (mc_global.mc_version);
                col += width;
                break;
            }
            case CHAR_FONT_BOLD:
                tty_setcolor (HELP_BOLD_COLOR);
                break;
            case CHAR_FONT_ITALIC:
                tty_setcolor (HELP_ITALIC_COLOR);
                break;
            case CHAR_FONT_NORMAL:
                help_print_word (h, word, &col, &line, FALSE);
                tty_setcolor (HELP_NORMAL_COLOR);
                break;
            case '\n':
                if (painting)
                    help_print_word (h, word, &col, &line, FALSE);
                line++;
                col = 0;
                break;
            case ' ':
            case '\t':
                // word delimiter
                if (painting)
                {
                    help_print_word (h, word, &col, &line, c == ' ');
                    if (c == '\t')
                    {
                        col = (col / 8 + 1) * 8;
                        if (col >= HELP_WINDOW_WIDTH)
                        {
                            line++;
                            col = 8;
                        }
                    }
                }
                break;
            default:
                if (painting && (line < help_lines))
                {
                    if (!acs)
                        // accumulate symbols in a word
                        g_string_append (word, buff);
                    else if (col < HELP_WINDOW_WIDTH)
                    {
                        widget_gotoyx (h, line + 2, col + 2);
                        tty_print_char (mc_acs_map (c));
                        col++;
                    }
                }
            }
        }

        // print last word
        if (n[0] == CHAR_NODE_END)
            help_print_word (h, word, &col, &line, FALSE);

        last_shown = p;
        end_of_node = line < help_lines;
        tty_setcolor (HELP_NORMAL_COLOR);
        if ((int) (selected_item - last_shown) >= 0)
        {
            if ((link_area == NULL) || (link_area->data == NULL))
                selected_item = NULL;
            else
            {
                selected_item = ((Link_Area *) link_area->data)->link_name;
                repeat_paint = TRUE;
            }
        }
    }
    while (repeat_paint);

    g_string_free (word, TRUE);

    // Position the cursor over a nice link
    if (active_col != 0)
        widget_gotoyx (h, active_line, active_col);
}

/* --------------------------------------------------------------------------------------------- */
/** show help */

static void
help_help (WDialog *h)
{
    const char *p;

    help_history_push (selected_item);

    p = help_find_node ("[How to use help]");
    if (p != NULL)
    {
        currentpoint = p + 1;  // Skip the newline following the start of the node
        selected_item = NULL;
        widget_draw (WIDGET (h));
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
help_index (WDialog *h)
{
    const char *new_item;

    help_history_push (selected_item);
    new_item = help_find_node ("[Contents]");

    if (new_item == NULL)
    {
        help_history_pop ();
        message (D_ERROR, MSG_ERROR, _ ("Cannot find node %s in help file"), "[Contents]");
    }
    else
    {

        currentpoint = new_item + 1;  // Skip the newline following the start of the node
        selected_item = NULL;
        widget_draw (WIDGET (h));
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
help_back (WDialog *h)
{
    currentpoint = history[history_ptr].page;
    selected_item = history[history_ptr].link;
    fdata = (char *) history[history_ptr].data;
    help_history_pop ();

    widget_draw (WIDGET (h));  // FIXME: unneeded?
}

/* --------------------------------------------------------------------------------------------- */

static void
help_next_link (gboolean move_down)
{
    const char *new_item;

    new_item = select_next_link (selected_item);
    if (new_item != NULL)
    {
        selected_item = new_item;
        if ((int) (selected_item - last_shown) >= 0)
        {
            if (move_down)
                move_forward (1);
            else
                selected_item = NULL;
        }
    }
    else if (move_down)
        move_forward (1);
    else
        selected_item = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
help_prev_link (gboolean move_up)
{
    const char *new_item;

    new_item = select_prev_link (selected_item);
    selected_item = new_item;
    if ((selected_item == NULL) || (selected_item < currentpoint))
    {
        if (move_up)
            move_backward (1);
        else if ((link_area != NULL) && (link_area->data != NULL))
            selected_item = ((Link_Area *) link_area->data)->link_name;
        else
            selected_item = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
help_next_node (void)
{
    const char *new_item;

    new_item = currentpoint;
    while ((*new_item != '\0') && (*new_item != CHAR_NODE_END))
        new_item++;

    if (*++new_item == '[')
        while (*++new_item != '\0')
            if ((*new_item == ']') && (*++new_item != '\0') && (*++new_item != '\0'))
            {
                currentpoint = new_item;
                selected_item = NULL;
                break;
            }
}

/* --------------------------------------------------------------------------------------------- */

static void
help_prev_node (void)
{
    const char *new_item;

    new_item = currentpoint;
    while (((int) (new_item - fdata) > 1) && (*new_item != CHAR_NODE_END))
        new_item--;
    new_item--;
    while (((int) (new_item - fdata) > 0) && (*new_item != CHAR_NODE_END))
        new_item--;
    while (*new_item != ']')
        new_item++;
    currentpoint = new_item + 2;
    selected_item = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static void
help_select_link (void)
{
    // follow link
    if (selected_item == NULL)
    {
#ifdef WE_WANT_TO_GO_BACKWARD_ON_KEY_RIGHT
        /* Is there any reason why the right key would take us
         * backward if there are no links selected?, I agree
         * with Torben than doing nothing in this case is better
         */
        // If there are no links, go backward in history
        history_ptr--;
        if (history_ptr < 0)
            history_ptr = HISTORY_SIZE - 1;

        currentpoint = history[history_ptr].page;
        selected_item = history[history_ptr].link;
        fdata = (char *) history[history_ptr].data;
#endif
    }
    else
    {
        help_history_push (selected_item);
        currentpoint = help_follow_link (currentpoint, selected_item);
    }

    selected_item = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
help_execute_cmd (long command)
{
    cb_ret_t ret = MSG_HANDLED;

    switch (command)
    {
    case CK_Help:
        help_help (whelp);
        break;
    case CK_Index:
        help_index (whelp);
        break;
    case CK_Back:
        help_back (whelp);
        break;
    case CK_Up:
        help_prev_link (TRUE);
        break;
    case CK_Down:
        help_next_link (TRUE);
        break;
    case CK_PageDown:
        move_forward (help_lines - 1);
        break;
    case CK_PageUp:
        move_backward (help_lines - 1);
        break;
    case CK_HalfPageDown:
        move_forward (help_lines / 2);
        break;
    case CK_HalfPageUp:
        move_backward (help_lines / 2);
        break;
    case CK_Top:
        move_to_top ();
        break;
    case CK_Bottom:
        move_to_bottom ();
        break;
    case CK_Enter:
        help_select_link ();
        break;
    case CK_LinkNext:
        help_next_link (FALSE);
        break;
    case CK_LinkPrev:
        help_prev_link (FALSE);
        break;
    case CK_NodeNext:
        help_next_node ();
        break;
    case CK_NodePrev:
        help_prev_node ();
        break;
    case CK_Quit:
        dlg_close (whelp);
        break;
    default:
        ret = MSG_NOT_HANDLED;
    }

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
help_handle_key (WDialog *h, int key)
{
    Widget *w = WIDGET (h);
    long command;

    command = widget_lookup_key (w, key);
    if (command == CK_IgnoreKey)
        return MSG_NOT_HANDLED;

    return help_execute_cmd (command);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
help_bg_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_DRAW:
        frame_callback (w, NULL, MSG_DRAW, 0, NULL);
        help_show (DIALOG (w->owner), currentpoint);
        return MSG_HANDLED;

    default:
        return frame_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
help_resize (WDialog *h)
{
    Widget *w = WIDGET (h);
    WButtonBar *bb;
    WRect r = w->rect;

    help_lines = MIN (LINES - 4, MAX (2 * LINES / 3, 18));
    r.lines = help_lines + 4;
    r.cols = HELP_WINDOW_WIDTH + 4;
    dlg_default_callback (w, NULL, MSG_RESIZE, 0, &r);
    bb = buttonbar_find (h);
    widget_set_size (WIDGET (bb), LINES - 1, 0, 1, COLS);

    return MSG_HANDLED;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
help_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_RESIZE:
        return help_resize (h);

    case MSG_KEY:
    {
        cb_ret_t ret;

        ret = help_handle_key (h, parm);
        if (ret == MSG_HANDLED)
            widget_draw (w);

        return ret;
    }

    case MSG_ACTION:
        // Handle shortcuts and buttonbar.
        return help_execute_cmd (parm);

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
interactive_display_finish (void)
{
    clear_link_areas ();
    MC_PTR_FREE (script_data);
    MC_PTR_FREE (main_data);
    if (linked_files != NULL)
    {
        g_hash_table_destroy (linked_files);
        linked_files = NULL;
    }
    MC_PTR_FREE (shown_name);
    fdata = NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** translate help file into terminal encoding */

static char *
translate_file (const char *filedata)
{
    GIConv conv;
    char *translated = NULL;

    conv = str_crt_conv_from ("UTF-8");
    if (conv != INVALID_CONV)
    {
        GString *translated_data;
        gboolean nok;

        // initial allocation for largest whole help file
        translated_data = g_string_sized_new (32 * 1024);
        nok = (str_convert (conv, filedata, translated_data) == ESTR_FAILURE);
        translated = g_string_free (translated_data, nok);

        str_close_conv (conv);
    }

    return translated;
}

/* --------------------------------------------------------------------------------------------- */
/** The text of a help file, ready for the window: the markdown becomes nodes, and the nodes
 * are then put into the charset of the terminal.
 */

static char *
help_load (const char *filedata)
{
    char *nodes;
    char *text;

    nodes = help_md_convert (filedata, NULL);
    text = translate_file (nodes);
    g_free (nodes);

    return text;
}

/* --------------------------------------------------------------------------------------------- */
/** A script's help stands in for a node of the program's help (the viewer's, the editor's); the
 * node shown gets a link to that one under its heading, so the way to the help it replaced is
 * always there.
 */

static void
help_link_script_node (char **filedata, const char *node, const char *parent_node)
{
    const char *heading;
    const char *eol;
    char *name;
    char *link;
    char *linked;

    if (parent_node == NULL || *parent_node == '\0' || strcmp (parent_node, node) == 0)
        return;

    heading = search_node (*filedata, node);
    if (heading == NULL)
        return;
    eol = strchr (heading, '\n');
    if (eol == NULL)
        return;

    name = g_strdup (parent_node);
    if (name[0] == '[' && name[strlen (name) - 1] == ']')
    {
        name[strlen (name) - 1] = '\0';
        memmove (name, name + 1, strlen (name));
    }
    // ^Atext^Bnode^C is a link; the node is named without brackets
    link = g_strdup_printf ("\n%s: \01%s\02%s\03\n", _ ("See also"), name, name);
    linked = g_strdup_printf ("%.*s%s%s", (int) (eol - *filedata + 1), *filedata, link, eol + 1);
    g_free (link);
    g_free (name);
    g_free (*filedata);
    *filedata = linked;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
md_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_RESIZE:
        widget_default_callback (w, NULL, MSG_RESIZE, 0, data);
        w->rect.lines = help_lines;
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
help_mouse_callback (Widget *w, mouse_msg_t msg, mouse_event_t *event)
{
    int x, y;
    GSList *current_area;

    if (msg == MSG_MOUSE_SCROLL_UP || msg == MSG_MOUSE_SCROLL_DOWN)
    {
        if (msg == MSG_MOUSE_SCROLL_UP)
            move_backward (2);
        else
            move_forward (2);

        widget_draw (WIDGET (w->owner));
        return;
    }

    if (msg != MSG_MOUSE_CLICK)
        return;

    if ((event->buttons & GPM_B_RIGHT) != 0)
    {
        // Right button click
        help_back (whelp);
        return;
    }

    // Left bytton click

    // The event is relative to the dialog window, adjust it:
    x = event->x - 1;
    y = event->y - 1;

    // Test whether the mouse click is inside one of the link areas
    for (current_area = link_area; current_area != NULL; current_area = g_slist_next (current_area))
    {
        Link_Area *la = (Link_Area *) current_area->data;

        // Test one line link area
        if (y == la->y1 && x >= la->x1 && y == la->y2 && x <= la->x2)
            break;

        // Test two line link area
        if (la->y1 + 1 == la->y2)
        {
            // The first line || The second line
            if ((y == la->y1 && x >= la->x1) || (y == la->y2 && x <= la->x2))
                break;
        }
        // Mouse will not work with link areas of more than two lines
    }

    // Test whether a link area was found
    if (current_area != NULL)
    {
        Link_Area *la = (Link_Area *) current_area->data;

        // The click was inside a link area -> follow the link
        help_history_push (la->link_name);
        currentpoint = help_follow_link (currentpoint, la->link_name);
        selected_item = NULL;
    }
    else if (y < 0)
        move_backward (help_lines - 1);
    else if (y >= help_lines)
        move_forward (help_lines - 1);
    else if (y < help_lines / 2)
        move_backward (1);
    else
        move_forward (1);

    // Show the new node
    widget_draw (WIDGET (w->owner));
}

/* --------------------------------------------------------------------------------------------- */

static Widget *
mousedispatch_new (const WRect *r)
{
    Widget *w;

    w = g_new0 (Widget, 1);
    widget_init (w, r, md_callback, help_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_WANT_CURSOR;

    return w;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* event callback */
gboolean
help_interactive_display (const gchar *event_group_name, const gchar *event_name,
                          gpointer init_data, gpointer data)
{
    Widget *wh;
    WGroup *g;
    WButtonBar *help_bar;
    Widget *md;
    char *helpfile = NULL;
    char *filedata;
    ev_help_t *event_data = (ev_help_t *) data;
    WRect r = { 1, 1, 1, 1 };
    int i;

    (void) event_group_name;
    (void) event_name;
    (void) init_data;

    if ((event_data->node == NULL) || (*event_data->node == '\0'))
        event_data->node = "[main]";

    /* A path is a script's own help, read as it stands and grafted onto the help of the
       program.  A bare name is the help file of another program of the suite, which the
       dialog that asks for the node names: it is read from where the help lives, in the
       language of the user. */
    if (event_data->filename != NULL && strchr (event_data->filename, PATH_SEP) != NULL)
    {
        g_file_get_contents (event_data->filename, &filedata, NULL, NULL);
        if (filedata != NULL)
        {
            script_data = help_load (filedata);
            g_free (filedata);
            /* after the translation: gettext already speaks the terminal's charset */
            if (script_data != NULL)
                help_link_script_node (&script_data, event_data->node, event_data->parent_node);
        }
        fdata = script_data;
    }
    else
    {
        filedata = load_mc_home_file (mc_global.share_data_dir, MC_HELP, &helpfile, NULL);
        if (filedata != NULL)
        {
            main_data = help_load (filedata);
            g_free (filedata);
        }
        fdata = main_data;

        // the file of the program the dialog belongs to becomes the one shown
        if (event_data->filename != NULL)
            (void) help_open_file (event_data->filename, TRUE, TRUE);
    }

    if (fdata == NULL)
        file_error_message (_ ("Cannot open file\n%s"),
                            event_data->filename ? event_data->filename : helpfile);

    g_free (helpfile);

    if (fdata == NULL)
    {
        interactive_display_finish ();
        return TRUE;
    }

    main_node = help_find_node (event_data->node);

    if (main_node == NULL)
    {
        message (D_ERROR, MSG_ERROR, _ ("Cannot find node %s in help file"), event_data->node);

        // Fallback to [main], return if it also cannot be found
        main_node = help_find_node ("[main]");
        if (main_node == NULL)
        {
            interactive_display_finish ();
            return TRUE;
        }
    }

    help_lines = MIN (LINES - 4, MAX (2 * LINES / 3, 18));

    whelp = dlg_create (TRUE, 0, 0, help_lines + 4, HELP_WINDOW_WIDTH + 4, WPOS_CENTER | WPOS_TRYUP,
                        FALSE, help_colors, help_callback, NULL, "[Help]", _ ("Help"));
    wh = WIDGET (whelp);
    g = GROUP (whelp);
    wh->keymap = help_map;
    widget_want_tab (wh, TRUE);
    // draw background
    whelp->bg->callback = help_bg_callback;

    selected_item = search_string_node (main_node, STRING_LINK_START);
    if (selected_item != NULL)
        selected_item--;
    currentpoint = main_node + 1;  // Skip the newline following the start of the node

    for (i = HISTORY_SIZE - 1; i >= 0; i--)
    {
        history[i].page = currentpoint;
        history[i].link = selected_item;
        history[i].data = fdata;
    }

    help_bar = buttonbar_new ();
    WIDGET (help_bar)->rect.y -= wh->rect.y;
    WIDGET (help_bar)->rect.x -= wh->rect.x;

    r.lines = help_lines;
    r.cols = HELP_WINDOW_WIDTH - 2;
    md = mousedispatch_new (&r);

    group_add_widget (g, md);
    group_add_widget (g, help_bar);  // FIXME

    buttonbar_set_label (help_bar, 1, Q_ ("ButtonBar|Help"), wh->keymap, NULL);
    buttonbar_set_label (help_bar, 2, Q_ ("ButtonBar|Index"), wh->keymap, NULL);
    buttonbar_set_label (help_bar, 3, Q_ ("ButtonBar|Prev"), wh->keymap, NULL);
    buttonbar_set_label (help_bar, 4, "", wh->keymap, NULL);
    buttonbar_set_label (help_bar, 5, "", wh->keymap, NULL);
    buttonbar_set_label (help_bar, 6, "", wh->keymap, NULL);
    buttonbar_set_label (help_bar, 7, "", wh->keymap, NULL);
    buttonbar_set_label (help_bar, 8, "", wh->keymap, NULL);
    buttonbar_set_label (help_bar, 9, "", wh->keymap, NULL);
    buttonbar_set_label (help_bar, 10, Q_ ("ButtonBar|Quit"), wh->keymap, NULL);

    dlg_run (whelp);
    interactive_display_finish ();
    widget_destroy (wh);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
