/*
   Skin editor plugin - the main dialog.

   Copyright (C) 2026
   Free Software Foundation, Inc.

   Written by:
   Ilia Maslakov <il.smind@gmail.com>, 2026

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "lib/global.h"
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/tty/tty.h"
#include "lib/widget.h"

#include "skineditor_ui.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef struct
{
    WDialog *dlg;
    WLabel *name_label;
    WLabel *hint_label;
    WButton *cancel_button;
} skineditor_ui_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
ui_layout (skineditor_ui_t *ui)
{
    const WRect *r = &WIDGET (ui->dlg)->rect;

    widget_set_size (WIDGET (ui->name_label), 2, 3, 1, r->cols - 6);
    widget_set_size (WIDGET (ui->hint_label), r->lines - 4, 3, 1, r->cols - 6);
    widget_set_size (WIDGET (ui->cancel_button), r->lines - 3,
                     (r->cols - button_get_width (ui->cancel_button)) / 2, 1,
                     button_get_width (ui->cancel_button));
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
ui_dialog_callback (Widget *w, Widget *sender, widget_msg_t msg, int parm, void *data)
{
    skineditor_ui_t *ui = (skineditor_ui_t *) DIALOG (w)->data.p;

    switch (msg)
    {
    case MSG_RESIZE:
        dlg_default_callback (w, sender, msg, parm, data);
        ui_layout (ui);
        return MSG_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
skineditor_run (void)
{
    skineditor_ui_t ui;
    WGroup *g;
    char *title, *name;

    title = g_strdup_printf (_ ("Skin: %s"), mc_skin__default.name);
    ui.dlg = dlg_create (TRUE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, dialog_colors,
                         ui_dialog_callback, NULL, NULL, title);
    g_free (title);
    ui.dlg->data.p = &ui;
    g = GROUP (ui.dlg);

    name = g_strdup_printf (_ ("%s (%s)"), mc_skin__default.name, mc_skin__default.description);
    ui.name_label = label_new (2, 3, name);
    g_free (name);
    group_add_widget (g, ui.name_label);

    ui.hint_label = label_new (3, 3, _ ("Esc Close"));
    group_add_widget (g, ui.hint_label);

    ui.cancel_button = button_new (4, 3, B_CANCEL, NORMAL_BUTTON, _ ("&Cancel"), NULL);
    group_add_widget (g, ui.cancel_button);

    ui_layout (&ui);
    dlg_run (ui.dlg);
    widget_destroy (WIDGET (ui.dlg));
}

/* --------------------------------------------------------------------------------------------- */
