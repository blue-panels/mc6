/** \file textarea.h
 *  \brief Header: WTextArea widget, a field of several lines
 */

#ifndef MC__WIDGET_TEXTAREA_H
#define MC__WIDGET_TEXTAREA_H

/*** typedefs(not structures) and defined constants **********************************************/

#define TEXTAREA(x) ((WTextArea *) (x))

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WTextArea
{
    Widget widget;
    GPtrArray *lines;  // the text, one entry per line, without the newline
    int line;          // the line the cursor is on, from 0
    int point;         // the character of that line the cursor is on, from 0
    int top;           // the first line drawn
    int left;          // the first character drawn of every line
    int mark_line;     // where the marked text begins; -1 when nothing is marked
    int mark_point;
    char charbuf[MB_LEN_MAX];  // the bytes of a character still coming in
    size_t charpoint;          // how many of them there are
} WTextArea;

/*** declarations of public functions ************************************************************/

WTextArea *textarea_new (int y, int x, int lines, int cols, const char *text);
void textarea_set_text (WTextArea *area, const char *text);
char *textarea_get_text (const WTextArea *area);

/* The marked text, and what is done to it. */
void textarea_mark (WTextArea *area, gboolean on);
char *textarea_get_marked (const WTextArea *area);
void textarea_delete_marked (WTextArea *area);

#endif
