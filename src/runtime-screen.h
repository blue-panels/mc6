/** \file runtime-screen.h
 *  \brief Header: full-screen widget groups filled and driven by a runtime package
 */

#ifndef MC_RUNTIME_SCREEN_H
#define MC_RUNTIME_SCREEN_H

#include "lib/extension-runtime.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

gboolean runtime_screen_run (mc_runtime_plugin_context_t *context,
                             const mc_runtime_screen_descriptor_t *descriptor, const char **error);
gboolean runtime_screen_update (mc_runtime_plugin_context_t *context, guint64 screen_id,
                                const mc_runtime_screen_patch_t *patch, const char **error);
gboolean runtime_screen_close (mc_runtime_plugin_context_t *context, guint64 screen_id,
                               const char **error);

/*** inline functions ****************************************************************************/

#endif /* MC_RUNTIME_SCREEN_H */
