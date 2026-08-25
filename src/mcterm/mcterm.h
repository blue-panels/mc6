/** \file mcterm.h
 *  \brief Header: the terminal widget that runs the shell
 */

#ifndef MC__MCTERM_H
#define MC__MCTERM_H

#include "lib/global.h"
#include "lib/widget.h"
#include "lib/keybind.h"
#include "lib/tty/tty.h"

/*** typedefs(not structures) and defined constants **********************************************/

/* What the session token is introduced by, in every OSC 7 our own shell sends. */
#define MCTERM_OSC7_TOKEN_PREFIX "?mc="

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WMcTerm WMcTerm;

/*** declarations of public functions ************************************************************/

#ifdef ENABLE_MCTERM

WMcTerm *mcterm_new (const WRect *r, const char *start_dir);
void mcterm_free (WMcTerm *t);
gboolean mcterm_is_alive (const WMcTerm *t);
gboolean mcterm_in_alt_screen (const WMcTerm *t);
void mcterm_scroll_to_end (WMcTerm *t);
void mcterm_set_scroll_allowed (WMcTerm *t, gboolean allowed);
Widget *mcterm_widget (WMcTerm *t);
gboolean mcterm_send_line (WMcTerm *t, const char *line);
gboolean mcterm_send_internal_line (WMcTerm *t, const char *line);
/* Remember a command submitted by the host and describe the process currently running it. */
void mcterm_set_command_hint (WMcTerm *t, const char *command);
char *mcterm_running_command (const WMcTerm *t, int max_width);
gboolean mcterm_shell_at_prompt (const WMcTerm *t);
/* Whether the shell's own line editor holds nothing typed yet: TRUE at a fresh prompt, FALSE once
   a command has been entered on it. Also TRUE when there is no prompt to speak of. */
gboolean mcterm_shell_line_is_empty (WMcTerm *t);
/* What the shell's line editor holds, read off the screen; NULL when nothing. Caller frees. */
char *mcterm_shell_line_text (WMcTerm *t);
/* How many characters of the line stand before the cursor; 0 when there is no line. */
int mcterm_shell_line_point (WMcTerm *t);
/* Tell the shell to drop its line; it reads as empty until the shell says something. */
void mcterm_clear_shell_line (WMcTerm *t);
gboolean mcterm_wait_for_prompt (WMcTerm *t, int timeout_msec);
const char *mcterm_osc7_raw (const WMcTerm *t);
/* The session token our own shell puts in every OSC 7, NULL when there is none. */
const char *mcterm_osc7_token (const WMcTerm *t);
/* The exit code the shell reported for the last command, -1 when it reported none. */
int mcterm_last_exit_code (const WMcTerm *t);
/* Advances while a command runs, so that the host can show that something is going on. */
guint mcterm_busy_phase (const WMcTerm *t);
void mcterm_set_busy_tick_callback (WMcTerm *t, void (*cb) (void *), void *data);
void mcterm_set_prompt_callback (WMcTerm *t, void (*cb) (void *), void *data);
void mcterm_set_after_redraw_callback (WMcTerm *t, void (*cb) (void *), void *data);
gboolean mcterm_osc7_capable (const WMcTerm *t);
int mcterm_cursor_col (const WMcTerm *t);
/* At a prompt the widget leaves the shell's row to the host: draw it with this. */
void mcterm_draw_prompt_row (const WMcTerm *t, int screen_y, const char *skin_section, int color);
gboolean mcterm_send_tab_complete (WMcTerm *t, const char *text);
/* Hand one key to the shell, for its own line editor to act on. */
gboolean mcterm_send_key (WMcTerm *t, int key);
long mcterm_key_command (const WMcTerm *t, int key);
/* Whether the host types on a command line of its own. Without one the plain
   arrows are left to the shell, there being nowhere else for typing to go. */
void mcterm_set_typing_elsewhere (WMcTerm *t, gboolean elsewhere);

#else /* !ENABLE_MCTERM */

static inline WMcTerm *
mcterm_new (const WRect *r, const char *start_dir)
{
    (void) r;
    (void) start_dir;
    return NULL;
}
static inline void
mcterm_free (WMcTerm *t)
{
    (void) t;
}
static inline gboolean
mcterm_is_alive (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline gboolean
mcterm_in_alt_screen (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline Widget *
mcterm_widget (WMcTerm *t)
{
    (void) t;
    return NULL;
}
static inline gboolean
mcterm_send_line (WMcTerm *t, const char *line)
{
    (void) t;
    (void) line;
    return FALSE;
}
static inline gboolean
mcterm_send_internal_line (WMcTerm *t, const char *line)
{
    (void) t;
    (void) line;
    return FALSE;
}
static inline void
mcterm_set_command_hint (WMcTerm *t, const char *command)
{
    (void) t;
    (void) command;
}
static inline char *
mcterm_running_command (const WMcTerm *t, int max_width)
{
    (void) t;
    (void) max_width;
    return NULL;
}
static inline gboolean
mcterm_shell_at_prompt (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline gboolean
mcterm_shell_line_is_empty (WMcTerm *t)
{
    (void) t;
    return TRUE;
}
static inline char *
mcterm_shell_line_text (WMcTerm *t)
{
    (void) t;
    return NULL;
}
static inline int
mcterm_shell_line_point (WMcTerm *t)
{
    (void) t;
    return 0;
}
static inline void
mcterm_clear_shell_line (WMcTerm *t)
{
    (void) t;
}
static inline gboolean
mcterm_wait_for_prompt (WMcTerm *t, int timeout_msec)
{
    (void) t;
    (void) timeout_msec;
    return FALSE;
}
static inline const char *
mcterm_osc7_raw (const WMcTerm *t)
{
    (void) t;
    return NULL;
}
static inline const char *
mcterm_osc7_token (const WMcTerm *t)
{
    (void) t;
    return NULL;
}
static inline int
mcterm_last_exit_code (const WMcTerm *t)
{
    (void) t;
    return -1;
}
static inline guint
mcterm_busy_phase (const WMcTerm *t)
{
    (void) t;
    return 0;
}
static inline void
mcterm_set_busy_tick_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    (void) t;
    (void) cb;
    (void) data;
}
static inline void
mcterm_set_prompt_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    (void) t;
    (void) cb;
    (void) data;
}
static inline void
mcterm_set_after_redraw_callback (WMcTerm *t, void (*cb) (void *), void *data)
{
    (void) t;
    (void) cb;
    (void) data;
}
static inline gboolean
mcterm_osc7_capable (const WMcTerm *t)
{
    (void) t;
    return FALSE;
}
static inline int
mcterm_cursor_col (const WMcTerm *t)
{
    (void) t;
    return -1;
}
static inline void
mcterm_draw_prompt_row (const WMcTerm *t, int screen_y, const char *skin_section, int color)
{
    (void) t;
    (void) screen_y;
    (void) skin_section;
    (void) color;
}
static inline gboolean
mcterm_send_tab_complete (WMcTerm *t, const char *text)
{
    (void) t;
    (void) text;
    return FALSE;
}
static inline gboolean
mcterm_send_key (WMcTerm *t, int key)
{
    (void) t;
    (void) key;
    return FALSE;
}
static inline long
mcterm_key_command (const WMcTerm *t, int key)
{
    (void) t;
    (void) key;
    return CK_IgnoreKey;
}
static inline void
mcterm_set_typing_elsewhere (WMcTerm *t, gboolean elsewhere)
{
    (void) t;
    (void) elsewhere;
}

#endif /* ENABLE_MCTERM */

/*** inline functions ****************************************************************************/

#endif /* MC__MCTERM_H */
