/*
   src/panel-plugins/shell-link - print a built-in helper script for the script tests

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

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

#include "src/panel-plugins/shell-link/shfs.h"

int
main (int argc, char **argv)
{
    const char *text;

    if (argc != 2)
    {
        fprintf (stderr, "usage: %s HELPER\n", argv[0]);
        return 2;
    }

    text = shfs_helper_builtin (argv[1]);
    if (text == NULL)
    {
        fprintf (stderr, "no built-in helper: %s\n", argv[1]);
        return 2;
    }

    fputs (text, stdout);

    return 0;
}
