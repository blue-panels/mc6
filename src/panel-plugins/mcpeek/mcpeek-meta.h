/** \file mcpeek-meta.h
 *  \brief Header: ECMA-335 metadata tables and heaps for mcpeek
 */

#ifndef MC__MCPEEK_META_H
#define MC__MCPEEK_META_H

#include "lib/global.h"

#include "mcpeek-pe.h"

/*** typedefs(not structures) and defined constants **********************************************/

#define MCPEEK_TABLE_MAX 64

/* Table ids, as in ECMA-335 II.22.  Only the ones the browser reads are named. */
#define MCPEEK_T_MODULE           0x00
#define MCPEEK_T_TYPEREF          0x01
#define MCPEEK_T_TYPEDEF          0x02
#define MCPEEK_T_FIELD            0x04
#define MCPEEK_T_METHODDEF        0x06
#define MCPEEK_T_PARAM            0x08
#define MCPEEK_T_INTERFACEIMPL    0x09
#define MCPEEK_T_MEMBERREF        0x0A
#define MCPEEK_T_CONSTANT         0x0B
#define MCPEEK_T_CUSTOMATTRIBUTE  0x0C
#define MCPEEK_T_EVENTMAP         0x12
#define MCPEEK_T_EVENT            0x14
#define MCPEEK_T_PROPERTYMAP      0x15
#define MCPEEK_T_PROPERTY         0x17
#define MCPEEK_T_METHODSEMANTICS  0x18
#define MCPEEK_T_MODULEREF        0x1A
#define MCPEEK_T_TYPESPEC         0x1B
#define MCPEEK_T_ASSEMBLY         0x20
#define MCPEEK_T_ASSEMBLYREF      0x23
#define MCPEEK_T_FILE             0x26
#define MCPEEK_T_EXPORTEDTYPE     0x27
#define MCPEEK_T_MANIFESTRESOURCE 0x28
#define MCPEEK_T_NESTEDCLASS      0x29
#define MCPEEK_T_GENERICPARAM     0x2A

/* Column numbers of the tables the browser walks.  Keeping them named here
   means the layout tables in the .c file stay the only place that knows the
   physical order. */
#define MCPEEK_TYPEDEF_FLAGS      0
#define MCPEEK_TYPEDEF_NAME       1
#define MCPEEK_TYPEDEF_NAMESPACE  2
#define MCPEEK_TYPEDEF_EXTENDS    3
#define MCPEEK_TYPEDEF_FIELDLIST  4
#define MCPEEK_TYPEDEF_METHODLIST 5

#define MCPEEK_FIELD_FLAGS        0
#define MCPEEK_FIELD_NAME         1
#define MCPEEK_FIELD_SIGNATURE    2

#define MCPEEK_METHOD_RVA         0
#define MCPEEK_METHOD_IMPLFLAGS   1
#define MCPEEK_METHOD_FLAGS       2
#define MCPEEK_METHOD_NAME        3
#define MCPEEK_METHOD_SIGNATURE   4
#define MCPEEK_METHOD_PARAMLIST   5

#define MCPEEK_TYPEREF_SCOPE      0
#define MCPEEK_TYPEREF_NAME       1
#define MCPEEK_TYPEREF_NAMESPACE  2

#define MCPEEK_ASMREF_MAJOR       0
#define MCPEEK_ASMREF_MINOR       1
#define MCPEEK_ASMREF_BUILD       2
#define MCPEEK_ASMREF_REVISION    3
#define MCPEEK_ASMREF_FLAGS       4
#define MCPEEK_ASMREF_PUBKEY      5
#define MCPEEK_ASMREF_NAME        6
#define MCPEEK_ASMREF_CULTURE     7

#define MCPEEK_ASM_NAME           7
#define MCPEEK_ASM_MAJOR          1
#define MCPEEK_ASM_MINOR          2
#define MCPEEK_ASM_BUILD          3
#define MCPEEK_ASM_REVISION       4

#define MCPEEK_NESTED_NESTED      0
#define MCPEEK_NESTED_ENCLOSING   1

#define MCPEEK_RESOURCE_OFFSET    0
#define MCPEEK_RESOURCE_FLAGS     1
#define MCPEEK_RESOURCE_NAME      2

#define MCPEEK_PROPERTY_FLAGS     0
#define MCPEEK_PROPERTY_NAME      1
#define MCPEEK_PROPERTY_TYPE      2

#define MCPEEK_EVENT_FLAGS        0
#define MCPEEK_EVENT_NAME         1

#define MCPEEK_MAP_PARENT         0
#define MCPEEK_MAP_LIST           1

/* Coded index kinds, ECMA-335 II.24.2.6 */
typedef enum
{
    MCPEEK_CI_TYPEDEFORREF = 0,
    MCPEEK_CI_HASCONSTANT,
    MCPEEK_CI_HASCUSTOMATTRIBUTE,
    MCPEEK_CI_HASFIELDMARSHAL,
    MCPEEK_CI_HASDECLSECURITY,
    MCPEEK_CI_MEMBERREFPARENT,
    MCPEEK_CI_HASSEMANTICS,
    MCPEEK_CI_METHODDEFORREF,
    MCPEEK_CI_MEMBERFORWARDED,
    MCPEEK_CI_IMPLEMENTATION,
    MCPEEK_CI_CUSTOMATTRIBUTETYPE,
    MCPEEK_CI_RESOLUTIONSCOPE,
    MCPEEK_CI_TYPEORMETHODDEF,
    MCPEEK_CI_COUNT
} mcpeek_coded_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    mcpeek_image_t *img;

    guint32 rows[MCPEEK_TABLE_MAX];
    const guint8 *base[MCPEEK_TABLE_MAX];
    guint32 row_size[MCPEEK_TABLE_MAX];
    guint8 col_off[MCPEEK_TABLE_MAX][16];
    guint8 col_width[MCPEEK_TABLE_MAX][16];
    guint8 col_count[MCPEEK_TABLE_MAX];

    guint8 str_w;
    guint8 guid_w;
    guint8 blob_w;
} mcpeek_meta_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean mcpeek_meta_init (mcpeek_meta_t *meta, mcpeek_image_t *img, GError **error);

/* Row count of @table; 0 when the table is absent. */
guint32 mcpeek_meta_rows (const mcpeek_meta_t *meta, int table);

/* Value of column @col in row @rid (1-based) of @table, zero-extended.
   Returns 0 for anything out of range, which callers read as "absent". */
guint32 mcpeek_meta_col (const mcpeek_meta_t *meta, int table, guint32 rid, int col);

/* A #Strings entry; never NULL, empty string for a bad index. */
const char *mcpeek_meta_string (const mcpeek_meta_t *meta, guint32 idx);

/* A #Blob entry with its length; NULL for a bad index. */
const guint8 *mcpeek_meta_blob (const mcpeek_meta_t *meta, guint32 idx, guint32 *len);

/* Split a coded index into the table it points at and the row within it.
   FALSE when the tag has no table assigned. */
gboolean mcpeek_meta_decode (mcpeek_coded_t kind, guint32 value, int *table, guint32 *rid);

/* Where the run of child rows owned by row @rid of @table ends: the list
   column of the next row, or the child table's row count for the last row.
   @list_col is the column holding the run start. */
guint32 mcpeek_meta_list_end (const mcpeek_meta_t *meta, int table, guint32 rid, int list_col,
                              int child_table);

/* Which TypeDef declares row @rid of @child_table.  The runs of children a
   type owns are the only link, and they run the other way, so this is a scan.
   0 when nothing owns it. */
guint32 mcpeek_meta_owner_type (const mcpeek_meta_t *meta, int child_table, int list_col,
                                guint32 rid);

#endif
