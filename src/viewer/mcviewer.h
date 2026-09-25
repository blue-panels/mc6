/** \file mcviewer.h
 *  \brief Header: internal file viewer
 */

#ifndef MC__VIEWER_H
#define MC__VIEWER_H

#include "lib/global.h"
#include "lib/widget.h"  // WRect

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* The help of the viewer, which its dialogs name when they ask for a node */
#define MCVIEW_HELP_FILE "mview.md"

struct WView;
typedef struct WView WView;
typedef struct mcview_generator mcview_generator_t;

mcview_generator_t *mcview_generator_new (const char *initial, gsize length,
                                          gboolean (*next) (void *, GString *, gboolean *),
                                          void *data, GDestroyNotify destroy);
mcview_generator_t *mcview_generator_ref (mcview_generator_t *generator);
void mcview_generator_unref (mcview_generator_t *generator);

typedef struct
{
    gboolean wrap;        // Wrap text lines to fit them on the screen
    gboolean hex;         // Plainview or hexview
    gboolean magic;       // Preprocess the file using external programs
    gboolean nroff;       // Nroff-style highlighting
    gboolean ansi;        // interpret SGR escape sequences found in the text
    gboolean highlight;   // color the text by syntax, as the editor does
    gboolean terminal;    // ANSI terminal replay mode (virtual screen buffer)
    gboolean structured;  // Structured (tree) view of JSON/YAML/XML content
} mcview_mode_flags_t;

/* Exactly one of command, argv, file or generator identifies the source. */
typedef struct
{
    char *command; /* shell pipeline -> mc_popen + stream */
    char **argv;   /* direct process argv; never interpreted by a shell */
    guint argc;
    char *cwd;
    gboolean separate_stderr;
    char *file; /* local path -> mc_open + file load */
    gboolean auto_scroll_bottom;
    /* Terminal display: the row of the output to open at, from 0. */
    guint top_row;
    char *title;
    char *help_file;
    char *help_node;
    gboolean initial_terminal;
    gboolean initial_nroff;
    /* the source asks the viewer to open with wrapping on or off */
    gboolean has_wrap;
    gboolean wrap;
    char *raw_file;
    mcview_generator_t *generator;
} mcview_source_spec_t;

typedef enum
{
    MCV_KEY_PASS = 0,
    MCV_KEY_HANDLED,
    MCV_KEY_OPEN_OPTIONS
} mcv_key_result_t;

typedef enum
{
    MCVIEW_SOURCE_STARTED,
    MCVIEW_SOURCE_FINISHED,
    MCVIEW_SOURCE_FAILED,
    MCVIEW_SOURCE_CANCELLED
} mcview_source_state_t;

typedef struct
{
    guint64 generation;
    mcview_source_state_t state;
    int exit_code;
    int term_signal;
    /* Bytes received from the process on stdout. */
    guint64 output_size;
} mcview_source_state_event_t;

typedef struct
{
    /* Open plugin options and update plugin-side pending state. */
    gboolean (*open_options) (void *ctx, mcview_source_spec_t *draft);

    /* Validate pending state and fill draft. */
    gboolean (*prepare) (void *ctx, mcview_source_spec_t *draft, char **err_out);

    /* New source opened successfully; promote pending state. */
    void (*commit) (void *ctx);

    /* Draft abandoned; drop pending state. */
    void (*rollback) (void *ctx);

    void (*free) (void *ctx);

    mcv_key_result_t (*handle_key) (void *ctx, int key);

    /* TRUE for the keys the source declared as its own: the viewer offers
       those to handle_key() before it looks them up in its own keymap. */
    gboolean (*owns_key) (void *ctx, int key);

    gboolean (*prepare_viewport) (void *ctx, mcview_source_spec_t *draft, guint columns,
                                  guint lines, char **err_out);
    gboolean rebuild_on_resize;
    void (*source_state) (void *ctx, const mcview_source_state_event_t *event);
} mcview_source_controller_t;

/* Spec helpers. clone() deep-copies all string fields. */
extern mcview_source_spec_t *mcview_source_spec_clone (const mcview_source_spec_t *src);
extern void mcview_source_spec_free (mcview_source_spec_t *s);

/*** global variables defined in .c file *********************************************************/

extern mcview_mode_flags_t mcview_global_flags;
extern mcview_mode_flags_t mcview_altered_flags;

extern gboolean mcview_remember_file_position;
extern int mcview_max_dirt_limit;

extern gboolean mcview_mouse_move_pages;
extern char *mcview_show_eof;
extern gboolean mcview_structured_auto;
extern int mcview_structured_max_size;
extern int mcview_structured_max_nodes;
/* One-shot request to open the next viewed file in structured mode
 * (set by the mctree symlink and %view{structured}; consumed by mcview_load) */
extern gboolean mcview_open_structured_once;

/*** declarations of public functions ************************************************************/

/* Creates a new WView object with the given properties. */
extern WView *mcview_new (const WRect *r, gboolean is_panel);

/* Shows {file} or the output of {command} in the internal viewer,
 * starting in line {start_line}.
 */
extern gboolean mcview_viewer (const char *command, const vfs_path_t *file_vpath, int start_line,
                               off_t search_start, off_t search_end);

extern gboolean mcview_load (WView *view, const char *command, const char *file, int start_line,
                             off_t search_start, off_t search_end);

/* View data from a pipe fd. */
extern gboolean mcview_viewer_fd (int fd);

/* View streaming command output (non-blocking, select-driven). */
extern gboolean mcview_viewer_stream (const char *command);

/* View a source driven by a plugin controller. Takes ownership of
 * initial_spec and ctx. */
extern gboolean mcview_viewer_with_controller (mcview_source_spec_t *initial_spec,
                                               const mcview_source_controller_t *controller,
                                               void *ctx, int start_line);

/* initial_spec and ctx are consumed on both success and failure. */
extern gboolean mcview_source_controller_attach (WView *view, mcview_source_spec_t *initial_spec,
                                                 const mcview_source_controller_t *controller,
                                                 void *ctx, char **err_out);
extern void mcview_source_controller_detach (WView *view);

extern void mcview_clear_mode_flags (mcview_mode_flags_t *flags);

// the viewer settings, read from and written to the [Viewer] section of the ini file
extern void mcview_load_options (void);
extern void mcview_save_options (void);
extern void mcview_done_options (void);

/* the %view{...} flags of one file: saved before they are read, put back after */
extern void mcview_global_flags_save (mcview_mode_flags_t *saved);
extern void mcview_global_flags_restore (const mcview_mode_flags_t *saved);

/* Show @text in place of content; in a panel it replaces the Quick View body. */
extern void mcview_load_text (WView *view, const char *text);
/* A viewer embedded as a cell of a screen: no frame, no status line. */
extern void mcview_set_embedded (WView *view, gboolean embedded);

/* Give the view a temp file to own; unlinked on the next one or on destroy. */
extern void mcview_set_tmp_preview (WView *view, const char *path);
extern void mcview_remove_tmp_preview (WView *view);
extern void mcview_source_rebuild_viewport (WView *view);

/*** inline functions ****************************************************************************/
#endif
