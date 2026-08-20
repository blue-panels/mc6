/** \file mcterm_setup.h
 *  \brief Header: shell integration setup strings written into the pty
 */

#ifndef MC__MCTERM_SETUP_H
#define MC__MCTERM_SETUP_H

#include "lib/shell.h"  // shell_type_t

/*** typedefs(not structures) and defined constants **********************************************/

/* Stands in a setup string where the per-session token goes. */
#define MCTERM_TOKEN_PLACEHOLDER "@MCTOKEN@"

/*** declarations of public functions ************************************************************/

const char *mcterm_shell_setup (shell_type_t shell_type);

#endif
