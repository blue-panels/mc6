/** \file slv.h
 *  \brief Header: STL5 def-file parser, evaluator and formatter.
 *  STL5 is the Struct Look 4.00 def-file format; files tagged "STL 5.00" get the extensions.
 */

#ifndef MC__MCSTRUCT_SLV_H
#define MC__MCSTRUCT_SLV_H

#include <sys/types.h>

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define SLV_MAX_IF_DEPTH      8
#define SLV_MAX_NEST_DEPTH    64
#define SLV_MAX_DEF_SIZE      (1024 * 1024)
#define SLV_MAX_DEF_LINES     65536
#define SLV_MAX_STRUCTS       4096
#define SLV_DEFAULT_LAZY_ROWS 64

/*** enums ***************************************************************************************/

typedef enum
{
    SLV_TYPE_NONE = 0,
    SLV_TYPE_CHAR,      /* c */
    SLV_TYPE_HEX,       /* b w d q */
    SLV_TYPE_INT,       /* i8 i16 i32 i64 */
    SLV_TYPE_UINT,      /* u8 u16 u32 u64 */
    SLV_TYPE_BITS,      /* t8 t16 t32 t64 */
    SLV_TYPE_BE_HEX,    /* m8 m16 m32 m64 */
    SLV_TYPE_FLOAT,     /* f32 f64 f80 */
    SLV_TYPE_MSBIN,     /* e32 e64 */
    SLV_TYPE_PTR,       /* p32 p48 */
    SLV_TYPE_PSTRING,   /* sp */
    SLV_TYPE_CSTRING,   /* sc */
    SLV_TYPE_TIME_DOS,  /* td */
    SLV_TYPE_TIME_UNIX, /* tu, tu64 (5.00) */
    SLV_TYPE_TIME_FILE, /* tf: Windows FILETIME, 100 ns since 1601 (5.00) */
    SLV_TYPE_JUMP,      /* j8 j16 j32 j64 */
    SLV_TYPE_STR8,      /* s8: fixed size, NUL padded UTF-8 (5.00) */
    SLV_TYPE_STR16,     /* s16: fixed size, NUL padded UTF-16LE (5.00) */
    SLV_TYPE_CHECK,     /* crc32 sum8 sum16 over a range (5.00) */
    SLV_TYPE_VARINT,    /* v: SQLite varint, big-endian 7-bit groups, up to 9 bytes (5.00) */
    SLV_TYPE_LEB128     /* vl: unsigned LEB128, little-endian 7-bit groups (5.00) */
} slv_type_t;

typedef enum
{
    SLV_ITEM_FIELD,  /* a typed field, possibly repeated */
    SLV_ITEM_REMARK, /* ':' text */
    SLV_ITEM_SKIP,   /* '+' / '-' expr */
    SLV_ITEM_SEEK,   /* '.' expr, local offset */
    SLV_ITEM_NESTED, /* '*' count name */
    SLV_ITEM_JUMP,   /* jNN target=expr */
    SLV_ITEM_IF,     /* #if block */
    SLV_ITEM_REPEAT, /* #repeat block (5.00) */
    SLV_ITEM_ENDIAN, /* #endian (5.00) */
    SLV_ITEM_SET,    /* #set label = expr (5.00) */
    SLV_ITEM_CHECK,  /* crc32 / sum8 / sum16 (5.00) */
    SLV_ITEM_VALUE   /* '=' expr: a computed value shown as a row, no bytes (5.00) */
} slv_item_kind_t;

typedef enum
{
    SLV_FOLLOWER_NONE = 0,
    SLV_FOLLOWER_EXPR,   /* count / length / offset */
    SLV_FOLLOWER_LEGEND, /* 'name or `name */
    SLV_FOLLOWER_BITS    /* digit string for tNN split */
} slv_follower_kind_t;

typedef enum
{
    SLV_DEF_STRUCT,
    SLV_DEF_TABLE,
    SLV_DEF_VALUE_LEGEND,
    SLV_DEF_BIT_LEGEND
} slv_def_kind_t;

typedef enum
{
    SLV_NODE_STRUCT, /* children are the items of one structure instance */
    SLV_NODE_FIELD,
    SLV_NODE_REMARK,
    SLV_NODE_NESTED, /* '*' N Struct: children are N STRUCT nodes */
    SLV_NODE_TABLE,  /* table: children are STRUCT rows */
    SLV_NODE_JUMP,
    SLV_NODE_REPEAT, /* #repeat: children are one STRUCT node per iteration */
    SLV_NODE_ERROR
} slv_node_kind_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct slv_expr_t slv_expr_t;
typedef struct slv_item_t slv_item_t;
typedef struct slv_def_t slv_def_t;
typedef struct slv_file_t slv_file_t;
typedef struct slv_node_t slv_node_t;

typedef struct
{
    int line;
    char *message;
} slv_error_t;

typedef struct
{
    gint64 min;
    gint64 max; /* value legend: range; bit legend: expected value */
    char *text;
} slv_legend_entry_t;

typedef struct
{
    slv_expr_t *cond; /* NULL for #else */
    GPtrArray *items; /* slv_item_t * */
} slv_branch_t;

struct slv_item_t
{
    slv_item_kind_t kind;
    int line;
    char *label;   /* lower-cased, may be NULL */
    char *name;    /* field title, target struct name, remark text */
    char *comment; /* after ';' */

    slv_type_t type;
    int size; /* bytes per element */
    gboolean big_endian;
    gboolean endian_set; /* .le / .be suffix or a fixed-order type: ignore #endian */
    gboolean hidden;     /* follower == 0 */
    int check_kind;      /* SLV_ITEM_CHECK: 0 crc32, 1 sum8, 2 sum16, 3 check expr (5.00) */

    slv_follower_kind_t follower_kind;
    slv_expr_t *follower; /* SLV_FOLLOWER_EXPR */
    char *legend;         /* SLV_FOLLOWER_LEGEND: lower-cased name */
    char *bits;           /* SLV_FOLLOWER_BITS */

    /* table columns: view width, 0 = auto */
    int view_width;

    /* SLV_ITEM_SKIP: +1 / -1; SLV_ITEM_NESTED / SLV_ITEM_JUMP: target;
       SLV_ITEM_VALUE: 0 decimal, 1 hex, 2 unix time */
    int direction; /* SKIP: +1 / -1; SEEK: 1 local, 2 absolute ('..') */
    char *target;  /* lower-cased key of the nested / jump target */
    gboolean target_is_table;
    slv_expr_t *rows; /* jump to a table: row count */
    slv_expr_t *jump; /* jump target offset */

    GPtrArray *branches; /* SLV_ITEM_IF: slv_branch_t * */

    /* SLV_ITEM_REPEAT: count (repeat_while FALSE) or condition; the block is branches[0] */
    gboolean repeat_while;
    /* SLV_ITEM_CHECK: range [range_from, range_to) and the expected value, may be NULL */
    slv_expr_t *range_from;
    slv_expr_t *range_to;
    slv_expr_t *expected;
    /* 5.00: title "=expr": the name is the C string at that file offset */
    slv_expr_t *title_at;
    /* 5.00: sc.XX ends the string at byte XX instead of NUL */
    int terminator;
    /* 5.00: #encoding in force for text fields, NULL = as is (UTF-8 / ASCII) */
    char *encoding;
};

struct slv_def_t
{
    slv_def_kind_t kind;
    char *name; /* as written */
    char *key;  /* lower-cased lookup key */
    gboolean hidden;
    int line;
    GPtrArray *items; /* struct / table: slv_item_t * */
    GArray *entries;  /* legends: slv_legend_entry_t */
};

struct slv_file_t
{
    char *path;
    int version_major;
    int version_minor;
    GPtrArray *defs;    /* slv_def_t *, in file order */
    GHashTable *by_key; /* key -> slv_def_t * */
    GPtrArray *errors;  /* slv_error_t * */
    GPtrArray *lines;   /* char *, the source lines for the def-file zone */
};

typedef struct
{
    gssize (*read) (void *ctx, off_t offset, void *buf, gsize len);
    off_t (*size) (void *ctx);
    void *ctx;
} slv_reader_t;

struct slv_node_t
{
    slv_node_kind_t kind;
    const slv_item_t *item; /* NULL for STRUCT rows and errors */
    const slv_def_t *def;   /* STRUCT / NESTED / TABLE / JUMP target */
    int line;               /* def-file line that produced the node */

    off_t offset; /* file offset */
    off_t size;   /* bytes covered, 0 for remarks and errors */

    gint64 value;  /* first element as integer, jump value */
    double dvalue; /* floats */
    char *key;     /* label or name column */
    char *hint;    /* type column: "u16", "c[12]", "-> Name" */
    char *text;    /* formatted value(s) */
    char *legend;  /* legend text, may be NULL */
    char *comment;

    gboolean lazy; /* children not built yet */
    gboolean expanded;
    gboolean big_endian; /* byte order the field was read with */
    off_t jump_target;   /* SLV_NODE_JUMP */
    gint64 rows;         /* NESTED / TABLE: element count */

    slv_node_t *parent;
    GPtrArray *children; /* slv_node_t * */
    GHashTable *labels;  /* STRUCT rows: label -> value/offset, for the calculator */
    off_t outer_base;    /* STRUCT rows: outermost structure start */
};

typedef struct
{
    const slv_file_t *file;
    const slv_reader_t *reader;
    gint64 lazy_rows; /* build children up to this many eagerly */
    const char *float_format;
} slv_eval_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* parser */
slv_file_t *slv_file_parse (const char *text, gsize len, const char *path);
slv_file_t *slv_file_load (const char *path, GError **error);
void slv_file_free (slv_file_t *file);
const slv_def_t *slv_file_lookup (const slv_file_t *file, const char *name);
slv_def_t *slv_file_first_struct (const slv_file_t *file);

/* expressions */
slv_expr_t *slv_expr_parse (const char *text, char **error);
void slv_expr_free (slv_expr_t *expr);
char *slv_expr_to_string (const slv_expr_t *expr);
/* TRUE and the value when the expression is a plain number */
gboolean slv_expr_constant (const slv_expr_t *expr, gint64 *value);

/* evaluation */
slv_node_t *slv_eval_struct (const slv_eval_t *ev, const slv_def_t *def, off_t offset);
slv_node_t *slv_eval_table (const slv_eval_t *ev, const slv_def_t *def, off_t offset, gint64 rows);
gboolean slv_node_expand (const slv_eval_t *ev, slv_node_t *node);
/* size of a definition whose items all have constant sizes, -1 otherwise */
gssize slv_def_fixed_size (const slv_def_t *def);
void slv_node_free (slv_node_t *node);
gboolean slv_eval_calc (const slv_eval_t *ev, const slv_node_t *scope, const char *text,
                        gint64 *result, char **error);

/* reader over a memory block */
slv_reader_t *slv_reader_new_memory (const void *data, gsize len);
void slv_reader_free (slv_reader_t *reader);

/* formatting helpers */
const char *slv_type_name (slv_type_t type, int size);
/* zlib polynomial; start with 0xFFFFFFFF and xor the result with 0xFFFFFFFF */
guint32 slv_crc32 (guint32 crc, const unsigned char *buf, gsize len);
gint64 slv_varint_value (gboolean leb128, const unsigned char *buf, gsize len);
gsize slv_varint_size (gboolean leb128, const unsigned char *buf, gsize avail);
char *slv_node_dump (const slv_node_t *node, int indent);

#endif
