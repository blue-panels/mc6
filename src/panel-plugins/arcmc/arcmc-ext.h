/** \file arcmc-ext.h
 *  \brief Header: runtime-configurable external archiver registry
 */

#ifndef ARCMC_EXT_H
#define ARCMC_EXT_H

#include "lib/mcconfig.h"

#include "arcmc-types.h"

/* A previously unknown external format is declared without recompilation as:

   [arcmc-ext-params-NAME]
   extension=.suffix
   pack_bin=...
   pack_args=...
   unpack_bin=...
   unpack_args=...
   test_bin=...
   test_args=...
   extfs_helper=...
   list_file_arg=...

   Missing command keys mean that the corresponding operation is unavailable. */

/*** declarations (functions) ******************************************************************/

void arcmc_ext_archivers_load (mc_config_t *cfg);
void arcmc_ext_archivers_save (mc_config_t *cfg);
void arcmc_ext_archivers_free (void);

const arcmc_ext_archiver_t *arcmc_ext_archiver_by_name (const char *name);
const arcmc_ext_archiver_t *arcmc_find_ext_archiver (const char *archive_path);

#endif /* ARCMC_EXT_H */
