/** \file mcpeek-resolve.h
 *  \brief Header: finding the file behind an assembly reference
 */

#ifndef MC__MCPEEK_RESOLVE_H
#define MC__MCPEEK_RESOLVE_H

#include "lib/global.h"

/*** declarations of public functions ************************************************************/

/* The file for assembly @name, looked for beside @base_dir first and then in
   the places a runtime keeps its assemblies.  NULL when nothing matches,
   which is an ordinary outcome: a reference to something not installed here
   is still a reference.  Caller frees. */
char *mcpeek_resolve_assembly (const char *name, const char *base_dir);

/* The directories that will be searched, in order, for a listing that wants
   to show them.  Caller frees with g_strfreev(). */
char **mcpeek_resolve_paths (const char *base_dir);

void mcpeek_resolve_shutdown (void);

#endif
