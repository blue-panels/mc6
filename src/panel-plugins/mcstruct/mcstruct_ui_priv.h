/** \file mcstruct_ui_priv.h
 *  \brief Header: the mcstruct screen, shared by its source files
 */

#ifndef MC__MCSTRUCT_UI_PRIV_H
#define MC__MCSTRUCT_UI_PRIV_H

#include <stdlib.h>
#include <string.h>
#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/event.h"
#include "lib/lock.h"
#include "lib/vfs/vfs.h"
#include "src/editor/edit.h"
#include "slv.h"
#include "slv_load.h"
#include "slv_reader.h"
#include "slv_edit.h"
#include "mcstruct_ui.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define TREE_COLS 5
#define DEF_COLS  2

/*** structures declarations (and typedefs of structures)*****************************************/

/* a one-line status bar in the skin's statusbar color, as mcedit has */
typedef struct
{
    Widget widget;
    char *text;
    int color; /* -1 = the statusbar color of the skin */
} WStatus;

/* skin sections [mcstruct-tree], [mcstruct-hex], [mcstruct-def] */
typedef struct
{
    int tree_normal, tree_frame, tree_frame_active, tree_head, tree_selected;
    int tree_offset, tree_name, tree_type, tree_value;
    int tree_struct, tree_jump, tree_remark, tree_error;
    int hex_head;
    hexstrip_colors_t hex;
    int def_normal, def_frame, def_frame_active, def_head, def_selected;
    int def_lineno, def_directive, def_comment, def_label;
} ui_colors_t;

enum
{
    CMD_HELP = 1,
    CMD_SAVE,
    CMD_PUT_BYTES, /* Shift-F2: the bytes of the current node to a file */
    CMD_GET_BYTES, /* Ctrl-F2: the bytes of the current node from a file */
    CMD_STRUCT,
    CMD_EDIT,
    CMD_GOTO,
    CMD_DEF,
    CMD_SEARCH,
    CMD_HEX,
    CMD_ERRORS,
    CMD_QUIT,
    CMD_BACK, /* Shift-F4: back to the viewer */
    CMD_EDIT_DEF,
    CMD_SEARCH_NEXT,
    CMD_CALC
};

enum
{
    COL_OFFSET,
    COL_KEY,
    COL_HINT,
    COL_VALUE,
    COL_COMMENT
};

typedef struct
{
    slv_node_t *node;
    int depth;
    int index; /* position in the parent's children */
} row_t;

typedef struct
{
    slv_node_t *root;
    int current;
    /* a buffer entered: the reader and hex source to come back to */
    const slv_reader_t *outer_reader;
    slv_reader_t *mem_reader;
    hexstrip_source_t outer_src;
} jump_t;

typedef struct
{
    WDialog *dlg;
    WStatus *title;
    WStatus *tree_head;
    WTable *tree;
    WStatus *hex_head;
    WHexStrip *hex;
    WStatus *def_head;
    WTable *def;
    WButtonBar *bb;
    ui_colors_t colors;

    slv_settings_t settings;
    slv_file_reader_t *fr;
    slv_file_t *file;
    char *def_path;
    slv_eval_t ev;
    slv_node_t *root;
    GArray *rows;     /* row_t */
    GPtrArray *jumps; /* jump_t * */
    int buffer_depth; /* inside a buffer: read-only */
    GString *cell;    /* scratch for the datasource */
    gboolean syncing;
    gboolean hex_hidden;
    char *search;
    unsigned char *hex_pat; /* the last byte search in the hex zone */
    gsize hex_pat_len;
    gboolean hex_pat_text;
    int grid_lead; /* 1 when the grid has a row number / offset column */
    char *display_name;
    gboolean quit;
    gboolean back; /* left with Shift-F4 */
    vfs_path_t *vpath;
    gboolean locked;

    /* table screen: replaces the tree zone while grid != NULL */
    WTable *grid;
    slv_node_t *grid_node;
    char *grid_path;      /* index path of grid_node, to find it again after a refresh */
    GPtrArray *grid_cols; /* slv_node_t *: the leaf fields of the first row, one per column */
    int grid_cache_row;   /* leaves of this row are in grid_cache */
    GPtrArray *grid_cache;
    int grid_first_col; /* horizontal scroll */
    int grid_ncols;     /* columns shown */
    int tree_current;   /* tree row to return to */
    WRect box[3];       /* frames around tree, def-file and hex zones; lines 0 = none */
    int zoom;           /* 0 none, 1 tree, 2 hex, 3 def-file */
    int zoom_key;
} ui_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WStatus *status_new (int y, int x, int cols);
char *node_path (const slv_node_t *node);
gboolean node_is_gridable (const slv_node_t *n);
gboolean node_is_text (const slv_node_t *n);
gboolean ui_cmd_save (ui_t *ui);
gboolean ui_input_offset (ui_t *ui, const char *title, off_t *offset);
gboolean ui_bytes_range (ui_t *ui, const char *what, off_t *offset, off_t *size, char **name,
                         char **sname);
gboolean ui_load_def (ui_t *ui, const char *path);
row_t *ui_current_row (ui_t *ui);
slv_node_t *ui_grid_cell_node (ui_t *ui, int row, int col);
void status_set_text (WStatus *st, const char *text);
void ui_apply_colors (ui_t *ui);
void ui_build (ui_t *ui, const slv_def_t *def, off_t offset);
void ui_build_rows (ui_t *ui, const slv_def_t *def, off_t offset, gint64 rows);
void ui_step_struct (ui_t *ui, int dir);
void ui_def_go_reference (ui_t *ui);
void ui_close_grid (ui_t *ui);
void ui_cmd_calc (ui_t *ui);
void ui_cmd_edit_def (ui_t *ui, int line);
void ui_cmd_edit_field (ui_t *ui);
void ui_cmd_errors (ui_t *ui);
void ui_cmd_help (void);
gboolean ui_hex_focused (const ui_t *ui);
void ui_cmd_put_bytes (ui_t *ui);
void ui_cmd_get_bytes (ui_t *ui);
void ui_cmd_quit (ui_t *ui);
void ui_cmd_search (ui_t *ui, gboolean next);
void ui_cmd_select_def (ui_t *ui);
void ui_cmd_select_struct (ui_t *ui);
void ui_grid_build (ui_t *ui);
void ui_grid_heads (ui_t *ui);
void ui_grid_scroll (ui_t *ui, int dir);
void ui_layout (ui_t *ui);
void ui_note_change (ui_t *ui);
void ui_open_grid (ui_t *ui, slv_node_t *node);
void ui_refresh (ui_t *ui);
void ui_sync_from_grid (ui_t *ui);
void ui_sync_from_tree (ui_t *ui);
void ui_sync_from_hex (ui_t *ui);
void ui_update_heads (ui_t *ui);
void ui_view_text (ui_t *ui, const slv_node_t *n);

#endif /* MC__MCSTRUCT_UI_PRIV_H */
