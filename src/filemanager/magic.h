/** \file magic.h
 *  \brief Plugin file-operation associations from magic.ini
 */

#ifndef MC__MAGIC_H
#define MC__MAGIC_H

#include "lib/panel-plugin.h"

typedef mc_pp_result_t (*mc_magic_get_local_copy_t) (void *data, char **local_path);

/* A file offered to magic.ini matching. @local_path is a borrowed local path;
   @get_local_copy supplies an owned temporary path only when a Type rule needs
   one. The caller owns and removes the returned temporary path. */
typedef struct
{
    const char *display_name;
    const char *local_path;
    mc_magic_get_local_copy_t get_local_copy;
    void *data;
} mc_magic_source_t;

typedef struct
{
    char *plugin_name;
    char *operation_name;
} mc_magic_action_t;

typedef enum
{
    MC_MAGIC_ACTION_NONE,
    MC_MAGIC_ACTION_FOUND,
    MC_MAGIC_ACTION_ERROR
} mc_magic_action_state_t;

/* Find the first magic.ini association for @action (Open or View). @local_copy
   is populated lazily for Type matching and remains the caller's to unlink and
   free, whether an association is found or not. */
mc_magic_action_state_t mc_magic_find_action (const mc_magic_source_t *source, const char *action,
                                              char **local_copy, mc_magic_action_t *result);
void mc_magic_action_clear (mc_magic_action_t *action);
void mc_magic_flush (void);

#endif /* MC__MAGIC_H */
