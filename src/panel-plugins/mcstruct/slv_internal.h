/** \file slv_internal.h
 *  \brief Header: shared declarations of the STL parser and evaluator
 */

#ifndef MC__MCSTRUCT_SLV_INTERNAL_H
#define MC__MCSTRUCT_SLV_INTERNAL_H

#include "slv.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

typedef enum
{
    SLV_EXPR_NUMBER,
    SLV_EXPR_LABEL,
    SLV_EXPR_AT,        /* @ or @id */
    SLV_EXPR_OFFSET,    /* ^label ^^label ^.label ^@ ^ */
    SLV_EXPR_FILE_SIZE, /* $size */
    SLV_EXPR_NEG,
    SLV_EXPR_NOT,
    SLV_EXPR_BITNOT,
    SLV_EXPR_ADD,
    SLV_EXPR_SUB,
    SLV_EXPR_MUL,
    SLV_EXPR_DIV,
    SLV_EXPR_MOD,
    SLV_EXPR_SHL,
    SLV_EXPR_SHR,
    SLV_EXPR_BAND,
    SLV_EXPR_BOR,
    SLV_EXPR_BXOR,
    SLV_EXPR_AND,
    SLV_EXPR_OR,
    SLV_EXPR_EQ,
    SLV_EXPR_NE,
    SLV_EXPR_LT,
    SLV_EXPR_GT,
    SLV_EXPR_LE,
    SLV_EXPR_GE,
    SLV_EXPR_CALL /* call('name', args...) (5.00) */
} slv_expr_op_t;

typedef enum
{
    SLV_OFFSET_FILE,  /* ^label */
    SLV_OFFSET_OUTER, /* ^^label */
    SLV_OFFSET_LOCAL  /* ^.label */
} slv_offset_kind_t;

/*** structures declarations (and typedefs of structures)*****************************************/

struct slv_expr_t
{
    slv_expr_op_t op;
    gint64 value;
    int str_len; /* NUMBER from a string literal: byte count */
    char *name;
    /* AT */
    slv_type_t type;
    int size;
    gboolean big_endian;
    gboolean endian_set;
    /* OFFSET */
    slv_offset_kind_t offset_kind;
    gboolean at_current;
    gboolean struct_start;
    slv_expr_t *left;
    slv_expr_t *right;
    /* CALL: name is the provider, args the arguments */
    GPtrArray *args;
};

typedef struct
{
    gint64 value;
    off_t offset;
    char *text; /* c / sc / sp / s8 / sz16 fields: the text, for comparisons with a literal */
} slv_label_t;

typedef struct slv_eval_ctx_t
{
    const slv_eval_t *ev;
    GHashTable *labels;   /* key -> slv_label_t *, one table per structure instance */
    off_t struct_base;    /* file offset where this structure started */
    off_t outer_base;     /* file offset of the outermost structure */
    off_t current_offset; /* next byte to read */
    int current_size;     /* size of the field being evaluated, for '@' */
    int depth;
    gboolean big_endian;                /* #endian in effect */
    const struct slv_eval_ctx_t *outer; /* labels of the enclosing structure are readable */
} slv_eval_ctx_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean slv_parse_number (const char *s, gsize len, gint64 *out);
gboolean slv_parse_type_id (const char *id, slv_type_t *type, int *size, gboolean *big_endian,
                            gboolean *endian_set);
gboolean slv_expr_eval (const slv_expr_t *e, const slv_eval_ctx_t *ctx, gint64 *out, char **error);
gboolean slv_read_bytes (const slv_reader_t *reader, off_t offset, void *buf, gsize len);
const slv_label_t *slv_ctx_lookup_label (const slv_eval_ctx_t *ctx, const char *key);

/* call providers (slv_call.c): a value, or bytes for a buffer */
gboolean slv_call_value (const slv_eval_ctx_t *ctx, const char *name, const gint64 *args,
                         guint nargs, gint64 *out, char **error);
GBytes *slv_call_bytes (const slv_eval_ctx_t *ctx, const char *name, const gint64 *args,
                        guint nargs, char **error);

/* formatting (slv_format.c) */
char *slv_format_value (const slv_item_t *item, const unsigned char *buf, gsize len,
                        const char *float_format, gint64 *first_value, double *dvalue);
char *slv_format_value_endian (const slv_item_t *item, gboolean big_endian,
                               const unsigned char *buf, gsize len, const char *float_format,
                               gint64 *first_value, double *dvalue);
char *slv_format_legend (const slv_def_t *legend, gint64 value);
char *slv_format_unix_time (gint64 v);
char *slv_format_bits (const unsigned char *buf, int size, gboolean big_endian, const char *split);

#endif
