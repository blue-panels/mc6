/** \file mcpeek-cs.h
 *  \brief Header: C# text through an external decompiler
 */

#ifndef MC__MCPEEK_CS_H
#define MC__MCPEEK_CS_H

#include "lib/global.h"

/*** declarations of public functions ************************************************************/

/* The configured decompiler command, resolved once: the value from
   mcpeek.ini, else ilspycmd on PATH, else the dotnet global tools dir.
   NULL when there is none, which is not an error - the plugin then shows IL
   and nothing else. */
const char *mcpeek_cs_command (void);

/* The version the decompiler reports, or NULL.  Caller frees. */
char *mcpeek_cs_version (void);

/* Decompiled C# of one type of @assembly.  Cached per assembly mtime and
   type, because a run costs a .NET startup.  Caller frees. */
char *mcpeek_cs_type (const char *assembly, const char *type_full_name, GError **error);

/* Write @assembly out as a compilable project under @dir, and, when the
   decompiler can, the portable PDB that maps that source back to the
   assembly.  Returns FALSE with @error set. */
gboolean mcpeek_cs_export (const char *assembly, const char *dir, gboolean align, GError **error);

/* A project written once into the cache and kept for as long as the assembly
   keeps its mtime, for the few files that only a project export produces:
   the .csproj and Properties/AssemblyInfo.cs.  NULL when it cannot be made. */
const char *mcpeek_cs_project_dir (const char *assembly);

/* Line of @text, 1-based, where @member appears to be declared; 0 when it
   cannot be found. */
int mcpeek_cs_find_member (const char *text, const char *member);

void mcpeek_cs_shutdown (void);

#endif
