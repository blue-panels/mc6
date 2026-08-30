/*
   mc_slv_dump - print a binary file through an STL def-file

   Usage: mc_slv_dump FILE DEF.STL [STRUCT] [OFFSET]

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026.

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

#include <stdio.h>
#include <stdlib.h>

#include "lib/global.h"

#include "src/panel-plugins/mcstruct/slv.h"

/* --------------------------------------------------------------------------------------------- */

int
main (int argc, char **argv)
{
    char *data = NULL;
    gsize len = 0;
    GError *error = NULL;
    slv_file_t *file;
    slv_reader_t *reader;
    slv_eval_t ev;
    const slv_def_t *def;
    slv_node_t *root;
    off_t offset = 0;
    guint i;
    char *dump;

    if (argc < 3)
    {
        fprintf (stderr, "usage: %s FILE DEF.STL [STRUCT] [OFFSET]\n", argv[0]);
        return 2;
    }

    if (!g_file_get_contents (argv[1], &data, &len, &error))
    {
        fprintf (stderr, "%s: %s\n", argv[1], error->message);
        return 1;
    }

    file = slv_file_load (argv[2], &error);
    if (file == NULL)
    {
        fprintf (stderr, "%s: %s\n", argv[2], error->message);
        return 1;
    }
    for (i = 0; i < file->errors->len; i++)
    {
        const slv_error_t *err = g_ptr_array_index (file->errors, i);

        fprintf (stderr, "%s:%d: %s\n", argv[2], err->line, err->message);
    }

    def = argc > 3 ? slv_file_lookup (file, argv[3]) : slv_file_first_struct (file);
    if (def == NULL)
    {
        fprintf (stderr, "structure not found\n");
        return 1;
    }
    if (argc > 4)
        offset = (off_t) strtoll (argv[4], NULL, 0);

    reader = slv_reader_new_memory (data, len);
    ev.file = file;
    ev.reader = reader;
    ev.lazy_rows = SLV_DEFAULT_LAZY_ROWS;
    ev.float_format = "%g";

    root = slv_eval_struct (&ev, def, offset);
    dump = slv_node_dump (root, 0);
    fputs (dump, stdout);
    g_free (dump);

    slv_node_free (root);
    slv_reader_free (reader);
    slv_file_free (file);
    g_free (data);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
