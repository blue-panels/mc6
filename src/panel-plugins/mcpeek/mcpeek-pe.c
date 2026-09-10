/*
   Panel plugin mcpeek for the M-Commander
   PE image and CLI metadata root

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
 */

#include <config.h>

#include <string.h>

#include "mcpeek-pe.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define DOS_MAGIC       0x5A4D     /* "MZ" */
#define PE_MAGIC        0x00004550 /* "PE\0\0" */
#define OPT_MAGIC_PE32  0x010B
#define OPT_MAGIC_PE32P 0x020B
#define META_MAGIC      0x424A5342 /* "BSJB" */

#define COFF_SIZE       20
#define SECTION_SIZE    40
#define DATA_DIR_CLI    14

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* All reads go through these: the file is untrusted input, and nothing in it
   is guaranteed to be aligned. */

static gboolean
rd_u16 (const guint8 *base, gsize size, gsize off, guint16 *out)
{
    guint16 v;

    if (off + 2 > size)
        return FALSE;
    memcpy (&v, base + off, 2);
    *out = GUINT16_FROM_LE (v);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
rd_u32 (const guint8 *base, gsize size, gsize off, guint32 *out)
{
    guint32 v;

    if (off + 4 > size)
        return FALSE;
    memcpy (&v, base + off, 4);
    *out = GUINT32_FROM_LE (v);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
mcpeek_parse_sections (mcpeek_image_t *img, gsize pe_off, guint16 opt_size, GError **error)
{
    gsize off;
    guint32 i;

    off = pe_off + 4 + COFF_SIZE + opt_size;

    img->sections = g_new0 (mcpeek_section_t, img->n_sections);

    for (i = 0; i < img->n_sections; i++)
    {
        gsize s = off + (gsize) i * SECTION_SIZE;

        if (!rd_u32 (img->data, img->size, s + 8, &img->sections[i].vsize)
            || !rd_u32 (img->data, img->size, s + 12, &img->sections[i].va)
            || !rd_u32 (img->data, img->size, s + 16, &img->sections[i].rawsize)
            || !rd_u32 (img->data, img->size, s + 20, &img->sections[i].raw))
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("truncated section table"));
            return FALSE;
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Walk the stream headers of the metadata root and record the ones we use. */
static gboolean
mcpeek_parse_streams (mcpeek_image_t *img, GError **error)
{
    guint32 ver_len;
    guint16 n_streams;
    gsize off;
    guint16 i;

    if (!rd_u32 (img->meta, img->meta_size, 12, &ver_len) || img->meta_size < 16
        || ver_len > img->meta_size - 16)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("bad metadata version"));
        return FALSE;
    }

    /* the field is padded with NULs but need not hold one: copy by length */
    {
        gsize n = MIN ((gsize) ver_len, sizeof (img->version) - 1);

        memcpy (img->version, img->meta + 16, n);
        img->version[n] = '\0';
    }

    /* the version string is padded to a 4-byte boundary */
    off = 16 + ((ver_len + 3) & ~3u);

    if (!rd_u16 (img->meta, img->meta_size, off + 2, &n_streams))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("no stream headers"));
        return FALSE;
    }
    off += 4;

    for (i = 0; i < n_streams; i++)
    {
        guint32 s_off, s_size;
        const char *name;
        gsize name_len;
        mcpeek_stream_t *target = NULL;

        if (!rd_u32 (img->meta, img->meta_size, off, &s_off)
            || !rd_u32 (img->meta, img->meta_size, off + 4, &s_size))
            break;

        name = (const char *) img->meta + off + 8;
        name_len = strnlen (name, img->meta_size - MIN (img->meta_size, off + 8));
        if (off + 8 + name_len >= img->meta_size)
            break;

        if (s_off > img->meta_size || s_size > img->meta_size - s_off)
            break;

        if (strcmp (name, "#~") == 0 || strcmp (name, "#-") == 0)
            target = &img->tables;
        else if (strcmp (name, "#Strings") == 0)
            target = &img->strings;
        else if (strcmp (name, "#US") == 0)
            target = &img->us;
        else if (strcmp (name, "#GUID") == 0)
            target = &img->guids;
        else if (strcmp (name, "#Blob") == 0)
            target = &img->blob;

        if (target != NULL)
        {
            target->data = img->meta + s_off;
            target->size = s_size;
        }

        off += 8 + ((name_len + 1 + 3) & ~(gsize) 3);
    }

    if (img->tables.data == NULL)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("no metadata table stream"));
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const guint8 *
mcpeek_rva_ptr (const mcpeek_image_t *img, guint32 rva, gsize need)
{
    guint32 i;

    for (i = 0; i < img->n_sections; i++)
    {
        const mcpeek_section_t *s = &img->sections[i];
        guint32 span;

        if (rva < s->va)
            continue;

        /* the mapped span is the smaller of the virtual and the raw size:
           beyond the raw size there is nothing in the file to point at */
        span = MIN (s->vsize, s->rawsize);
        if (rva - s->va >= span || span - (rva - s->va) < need)
            continue;

        if ((gsize) s->raw + (rva - s->va) + need > img->size)
            return NULL;

        return img->data + s->raw + (rva - s->va);
    }

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

mcpeek_image_t *
mcpeek_image_open (const char *path, GError **error)
{
    mcpeek_image_t *img;
    guint16 dos_magic, opt_magic, opt_size, n_sections;
    guint32 pe_sig, pe_off32, meta_rva, meta_size, n_dirs;
    gsize pe_off, opt_off, dir_off;
    const guint8 *cli;
    guint32 magic;

    img = g_new0 (mcpeek_image_t, 1);

    img->map = g_mapped_file_new (path, FALSE, error);
    if (img->map == NULL)
    {
        g_free (img);
        return NULL;
    }

    img->data = (const guint8 *) g_mapped_file_get_contents (img->map);
    img->size = g_mapped_file_get_length (img->map);

    if (img->data == NULL || !rd_u16 (img->data, img->size, 0, &dos_magic) || dos_magic != DOS_MAGIC
        || !rd_u32 (img->data, img->size, 0x3C, &pe_off32))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("not a PE file"));
        mcpeek_image_free (img);
        return NULL;
    }

    pe_off = pe_off32;
    if (!rd_u32 (img->data, img->size, pe_off, &pe_sig) || pe_sig != PE_MAGIC)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("not a PE file"));
        mcpeek_image_free (img);
        return NULL;
    }

    if (!rd_u16 (img->data, img->size, pe_off + 6, &n_sections)
        || !rd_u16 (img->data, img->size, pe_off + 20, &opt_size))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("truncated COFF header"));
        mcpeek_image_free (img);
        return NULL;
    }
    img->n_sections = n_sections;

    opt_off = pe_off + 4 + COFF_SIZE;
    if (!rd_u16 (img->data, img->size, opt_off, &opt_magic))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("truncated optional header"));
        mcpeek_image_free (img);
        return NULL;
    }
    img->pe32plus = opt_magic == OPT_MAGIC_PE32P;

    if (!img->pe32plus && opt_magic != OPT_MAGIC_PE32)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("unknown optional header"));
        mcpeek_image_free (img);
        return NULL;
    }

    if (!mcpeek_parse_sections (img, pe_off, opt_size, error))
    {
        mcpeek_image_free (img);
        return NULL;
    }

    /* the data directory follows the optional header proper; its size and the
       header layout differ between PE32 and PE32+ */
    dir_off = opt_off + (img->pe32plus ? 112 : 96);
    if (!rd_u32 (img->data, img->size, dir_off - 4, &n_dirs) || n_dirs <= DATA_DIR_CLI)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("no CLI data directory"));
        mcpeek_image_free (img);
        return NULL;
    }

    {
        guint32 cli_rva, cli_size;

        if (!rd_u32 (img->data, img->size, dir_off + DATA_DIR_CLI * 8, &cli_rva)
            || !rd_u32 (img->data, img->size, dir_off + DATA_DIR_CLI * 8 + 4, &cli_size)
            || cli_rva == 0)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("not a managed assembly"));
            mcpeek_image_free (img);
            return NULL;
        }

        cli = mcpeek_rva_ptr (img, cli_rva, 72);
        if (cli == NULL)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("bad CLI header"));
            mcpeek_image_free (img);
            return NULL;
        }
    }

    (void) rd_u32 (cli, 72, 24, &img->resources_rva);
    (void) rd_u32 (cli, 72, 28, &img->resources_size);

    if (!rd_u32 (cli, 72, 8, &meta_rva) || !rd_u32 (cli, 72, 12, &meta_size)
        || !rd_u32 (cli, 72, 16, &img->cli_flags) || !rd_u32 (cli, 72, 20, &img->entry_point_token))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("bad CLI header"));
        mcpeek_image_free (img);
        return NULL;
    }

    img->meta = mcpeek_rva_ptr (img, meta_rva, meta_size);
    img->meta_size = meta_size;
    if (img->meta == NULL || !rd_u32 (img->meta, img->meta_size, 0, &magic) || magic != META_MAGIC)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _ ("no metadata root"));
        mcpeek_image_free (img);
        return NULL;
    }

    if (!mcpeek_parse_streams (img, error))
    {
        mcpeek_image_free (img);
        return NULL;
    }

    return img;
}

/* --------------------------------------------------------------------------------------------- */

void
mcpeek_image_free (mcpeek_image_t *img)
{
    if (img == NULL)
        return;

    g_free (img->sections);
    if (img->map != NULL)
        g_mapped_file_unref (img->map);
    g_free (img);
}

/* --------------------------------------------------------------------------------------------- */

gboolean
mcpeek_image_is_managed (const char *path)
{
    mcpeek_image_t *img;

    img = mcpeek_image_open (path, NULL);
    if (img == NULL)
        return FALSE;

    mcpeek_image_free (img);
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
