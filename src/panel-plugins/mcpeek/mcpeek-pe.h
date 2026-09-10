/** \file mcpeek-pe.h
 *  \brief Header: PE image and CLI metadata root for mcpeek
 */

#ifndef MC__MCPEEK_PE_H
#define MC__MCPEEK_PE_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    guint32 va;
    guint32 vsize;
    guint32 raw;
    guint32 rawsize;
} mcpeek_section_t;

/* One stream of the metadata root, already bounds-checked against the file. */
typedef struct
{
    const guint8 *data;
    gsize size;
} mcpeek_stream_t;

typedef struct
{
    GMappedFile *map;
    const guint8 *data;
    gsize size;

    gboolean pe32plus;
    guint32 n_sections;
    mcpeek_section_t *sections;

    /* CLI header (data directory 14) */
    guint32 cli_flags;
    guint32 entry_point_token;
    guint32 resources_rva;
    guint32 resources_size;

    /* metadata root and its streams */
    const guint8 *meta;
    gsize meta_size;
    char version[64]; /* the runtime version string, e.g. "v4.0.30319" */

    mcpeek_stream_t tables;  /* #~ or #- */
    mcpeek_stream_t strings; /* #Strings */
    mcpeek_stream_t us;      /* #US */
    mcpeek_stream_t guids;   /* #GUID */
    mcpeek_stream_t blob;    /* #Blob */
} mcpeek_image_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/* Open a PE file and locate its CLI metadata.  Returns NULL and sets @error
   when the file is not a managed assembly or is malformed. */
mcpeek_image_t *mcpeek_image_open (const char *path, GError **error);
void mcpeek_image_free (mcpeek_image_t *img);

/* Map a relative virtual address to a pointer into the file, or NULL when the
   range [rva, rva + need) is not covered by any section. */
const guint8 *mcpeek_rva_ptr (const mcpeek_image_t *img, guint32 rva, gsize need);

/* Does this look like a managed assembly?  Cheap enough for a directory
   listing: reads only the headers. */
gboolean mcpeek_image_is_managed (const char *path);

#endif
