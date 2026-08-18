/*
   Midnight Commander - mcterm shell protocol marks.

   Copyright (C) 2026
   Free Software Foundation, Inc.

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

#ifndef MC__MCTERM_PROTO_H
#define MC__MCTERM_PROTO_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* The key our session token is passed under, in every mark our own shell sends. */
#define MCTERM_MARK_TOKEN_KEY "mc="

/*** enums ***************************************************************************************/

/* The semantic prompt marks of OSC 133, as the shells that speak it emit them. */
typedef enum
{
    MCTERM_MARK_NONE = 0,
    MCTERM_MARK_PROMPT_START,   // 133;A - the prompt begins here
    MCTERM_MARK_PROMPT_END,     // 133;B - the prompt ends, what follows is typed by the user
    MCTERM_MARK_COMMAND_START,  // 133;C - the command is under way
    MCTERM_MARK_COMMAND_DONE,   // 133;D - it finished, with an exit code
} mcterm_mark_t;

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    mcterm_mark_t mark;
    /* Meaningful for MCTERM_MARK_COMMAND_DONE, -1 when the shell sent no code. */
    int exit_code;
} mcterm_osc133_t;

/*** declarations of public functions ************************************************************/

gboolean mcterm_osc133_parse (const char *raw, const char *token, mcterm_osc133_t *out);

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_PROTO_H */
