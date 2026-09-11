/*
   Lexical scanner for the M-Commander
   Header: terminal color backend

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

/** \file tty-backend.h
 *  \brief Header: terminal color backend for the lexical scanner
 */

#ifndef MC__SYNTAX_TTY_BACKEND_H
#define MC__SYNTAX_TTY_BACKEND_H

#include "syntax.h"

/*** declarations of public functions ************************************************************/

/**
 * syntax_color_alloc_fn drawing on the terminal.
 *
 * @param backend_data skin section a half-specified color falls back to, e.g.
 *        "editor"; NULL means the terminal's own default
 */
int syntax_tty_alloc_color (void *backend_data, const char *fg, const char *bg, const char *attrs);
void syntax_tty_release_color (int color);

#endif
