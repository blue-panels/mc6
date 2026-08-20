/*
   Midnight Commander - the shell integration mcterm writes into each shell.

   The strings here teach bash, zsh, fish and the POSIX shells to report their
   working directory (OSC 7) and mark their prompts and commands (OSC 133), each
   carrying the session token so the host tells its own shell from any other.

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

/** \file mcterm_setup.c
 *  \brief Source: shell integration setup strings written into the pty
 */

#include <config.h>

#include "lib/global.h"
#include "lib/shell.h"  // shell_type_t, SHELL_*

#include "mcterm.h"        // MCTERM_OSC7_TOKEN_PREFIX
#include "mcterm_proto.h"  // MCTERM_MARK_TOKEN_KEY
#include "mcterm_setup.h"

/*** file scope macros ***************************************************************************/

/* The session token tails every OSC the strings below make the shell send:
   mc=<token> on a semantic mark, ?mc=<token> on an OSC 7. */
#define MC_TOK MCTERM_MARK_TOKEN_KEY MCTERM_TOKEN_PLACEHOLDER
#define MC_CWD MCTERM_OSC7_TOKEN_PREFIX MCTERM_TOKEN_PLACEHOLDER

/*** public functions ****************************************************************************/

/* --------------------------------------------------------------------------------------------- */

/**
 * The shell integration to write into a shell of type @shell_type, with MCTERM_TOKEN_PLACEHOLDER
 * standing where the per-session token goes; NULL for a shell the terminal cannot drive.
 */

const char *
mcterm_shell_setup (shell_type_t shell_type)
{
    switch (shell_type)
    {
    case SHELL_BASH:
    {
        /* Percent-encoder for $PWD, a hook that reports the directory and the exit code
         * after every command, and a prompt wrapped in marks that say where it ends. */
        static const char setup[] =
            "__mc_pe(){"
            " local s=$1 o= i c;"
            " for((i=0;i<${#s};i++)); do"
            " c=${s:i:1};"
            " case $c in"
            " [a-zA-Z0-9/_~.-]) o+=$c;;"
            " *) printf -v o '%s%%%02X' \"$o\" \"'$c\";;"
            " esac; done;"
            " printf '%s' \"$o\";"
            " }; \\\n"
            "__mc_pc(){"
            " local e=$?;"
            " printf '\\033]133;D;%s;" MC_TOK "\\007' \"$e\";"
            " printf '\\033]7;file://%s" MC_CWD "\\007' \"$(__mc_pe \"$PWD\")\";"
            /* Setting PS1 is an everyday thing to do at a prompt, and it would throw the
               marks away. Put them back whenever they are gone. */
            " case \"$PS1\" in *'133;B;" MC_TOK "'*) ;;"
            " *) PS1=\"\\[\\e]133;A;" MC_TOK "\\a\\]${PS1}\\[\\e]133;B;" MC_TOK "\\a\\]\";;"
            " esac;"
            " return $e;"
            " }; \\\n"
            " if test $BASH_VERSINFO -ge 5"
            " && [[ ${PROMPT_COMMAND@a} == *a* ]] 2>/dev/null; then \\\n"
            "  PROMPT_COMMAND+=(__mc_pc); \\\n"
            " else \\\n"
            "  PROMPT_COMMAND=\"${PROMPT_COMMAND:+$PROMPT_COMMAND; }__mc_pc\"; \\\n"
            " fi; \\\n"
            " PS0=\"\\[\\e]133;C;" MC_TOK "\\a\\]${PS0}\"; \\\n"
            " printf "
            "'\\033]7;file://__mc_sync__/" MC_CWD
            /* Drop this setup itself from the shell's history: it is ours, not the user's. */
            "\\007'; history -d $HISTCMD 2>/dev/null\r";

        return setup;
    }

    case SHELL_ZSH:
    {
        static const char setup[] =
            // Enabled first and on its own line, so the rest of this setup - and later mc's
            // own commands - stay out of the history behind the leading space that follows.
            "setopt hist_ignore_space\r"
            " __mc_pe(){local s=$1 o='' c i;for (( i=1; i<=${#s}; i++ )); do c=${s[i]};"
            "case $c in [a-zA-Z0-9/_~.-])o+=$c;;*)printf -v o '%s%%%02X' \"$o\" \"'$c\";"
            ";esac;done;printf %s \"$o\";}; \\\n"
            " __mc_first=1;__mc_precmd(){local e=$?;if (( __mc_first ));then printf "
            "'\\033[2J\\033[H';"
            "__mc_first=0;fi;"
            "printf '\\033]133;D;%s;" MC_TOK "\\007' \"$e\";"
            "printf '\\033]7;file://%s" MC_CWD "\\007' \"$(__mc_pe \"$PWD\")\";"
            /* An assignment to PROMPT throws the marks away: put them back. */
            "[[ $PROMPT == *'133;B;" MC_TOK "'* ]]"
            " || PROMPT=$'%{\\e]133;A;" MC_TOK "\\a%}'$PROMPT$'%{\\e]133;B;" MC_TOK "\\a%}';};"
            "precmd_functions+=(__mc_precmd); \\\n"
            " __mc_preexec(){printf "
            "'\\033]133;C;" MC_TOK "\\007';};"
            "preexec_functions+=(__mc_preexec); \\\n"
            " printf "
            "'\\033]7;file://__mc_sync__/" MC_CWD "\\007'\r";

        return setup;
    }

    case SHELL_FISH:
    {
        /* fish has an event for everything, so nothing here replaces what the user set:
         * the prompt function is copied and called, the rest hangs off events. */
        static const char setup[] =
            "functions -q __mc_orig_prompt; or functions -c fish_prompt __mc_orig_prompt; "
            "function fish_prompt; printf "
            "'\\033]133;A;" MC_TOK "\\a'; __mc_orig_prompt; "
            "printf '\\033]133;B;" MC_TOK "\\a'; end; "
            "function __mc_preexec --on-event fish_preexec; "
            "printf '\\033]133;C;" MC_TOK "\\a'; end; "
            "function __mc_postexec --on-event fish_postexec; "
            "printf '\\033]133;D;%s;" MC_TOK "\\a' $status; end; "
            "function __mc_cwd --on-event fish_prompt; "
            "printf '\\033]7;file://%s" MC_CWD "\\a' (string escape --style=url -- $PWD); end; "
            "printf "
            "'\\033]7;file://__mc_sync__/" MC_CWD "\\a'\r";

        return setup;
    }

    case SHELL_SH:
    case SHELL_DASH:
    case SHELL_ASH_BUSYBOX:
    {
        /* No precmd or preexec hook here, but PS1 is expanded again at every prompt: a
         * command substitution in it reports the directory and the exit code and lays the
         * prompt marks down itself. There is no command-start (C) mark, nothing runs
         * between the prompt and the command for it to hang on. */
        static const char setup[] =
            "__mc_pe(){ s=$1; o=;"
            " while [ -n \"$s\" ]; do c=${s%\"${s#?}\"};"
            " case $c in [a-zA-Z0-9/_~.-]) o=$o$c;;"
            " *) o=$(printf '%s%%%02X' \"$o\" \"'$c\");; esac;"
            " s=${s#?}; done; printf %s \"$o\"; };"
            " __mc_a(){"
            " printf '\\033]133;D;%s;" MC_TOK "\\007' \"$1\";"
            " printf '\\033]7;file://%s" MC_CWD "\\007' \"$(__mc_pe \"$PWD\")\";"
            " printf '\\033]133;A;" MC_TOK "\\007'; };"
            " __mc_b(){ printf '\\033]133;B;" MC_TOK "\\007'; };"
            " PS1='$(__mc_a $?)'\"$PS1\"'$(__mc_b)';"
            " printf "
            "'\\033]7;file://__mc_sync__/" MC_CWD "\\007'\r";

        return setup;
    }

    default:
        /* ksh, mksh, tcsh and the rest: no hook to chain onto and no PS1 the terminal can
         * drive, so they run without the protocol. */
        return NULL;
    }
}

#undef MC_TOK
#undef MC_CWD

/* --------------------------------------------------------------------------------------------- */
