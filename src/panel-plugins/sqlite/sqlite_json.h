/** \file sqlite_json.h
 *  \brief Header: JSON pretty-printer used by the SQLite panel plugin
 */

#ifndef MC__PANEL_PLUGIN_SQLITE_JSON_H
#define MC__PANEL_PLUGIN_SQLITE_JSON_H

#include <glib.h>

/* Return a newly allocated, formatted JSON value, or NULL if @json is not
   valid JSON.  Lines within the returned value are indented by @indent. */
char *sqlite_json_pretty (const char *json, gsize length, int indent);

#endif /* MC__PANEL_PLUGIN_SQLITE_JSON_H */
