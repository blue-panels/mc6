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
#include <string.h>

#include "src/panel-plugins/shell-link/shelldef.h"

int
main (int argc, char **argv)
{
    const char *name = argc > 1 ? argv[1] : "";

    if (strcmp (name, "ls") == 0)
        fputs (VFS_SHELL_LS_DEF_CONTENT, stdout);
    else if (strcmp (name, "get") == 0)
        fputs (VFS_SHELL_GET_DEF_CONTENT, stdout);
    else
    {
        fprintf (stderr, "usage: %s ls|get\n", argv[0]);
        return 2;
    }

    return 0;
}
