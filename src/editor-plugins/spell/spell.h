#ifndef MC__EDIT_ASPELL_H
#define MC__EDIT_ASPELL_H

#include "lib/global.h"  // include <glib.h>
#include "lib/editor-plugin.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void spell_debug_log (const char *fmt, ...) G_GNUC_PRINTF (1, 2);

void spell_runtime_init (void);
void spell_runtime_shutdown (void);
mc_ep_result_t spell_query_state (mc_ep_state_t *state);

int edit_suggest_current_word (WEdit *edit);
void edit_spellcheck_file (WEdit *edit);
void edit_set_spell_lang (void);
void edit_spell_plugin_settings (void);

const char *spell_dialog_lang_list_show (const GPtrArray *languages);

/*** inline functions ****************************************************************************/

#endif
