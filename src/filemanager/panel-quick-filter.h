/*
   Internal panel quick-filter operations.

   Copyright (C) 2026
   Free Software Foundation, Inc.

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

/** \file panel-quick-filter.h
 *  \brief Header: internal quick-filter operations used by the panel and tests
 */

#ifndef MC__PANEL_QUICK_FILTER_H
#define MC__PANEL_QUICK_FILTER_H

#include "panel.h"

void panel_quick_filter_begin (WPanel *panel);
gboolean panel_quick_filter_apply (WPanel *panel);
void panel_quick_filter_restore (WPanel *panel);
void panel_quick_filter_sync_marks (WPanel *panel);
void panel_quick_filter_get_marked_count (const WPanel *panel, int *visible, int *hidden);

#endif
