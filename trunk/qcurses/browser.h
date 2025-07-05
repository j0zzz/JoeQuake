/*
 * Module to simplify drawing an interactive menu a la curses and 
 * ncurses. The module detects the amount of space available in the menu,
 * creates a grid of characters, and supports writing to this grid.
 *
 * Functions for auto-wrapping text are included.
 *
 * Copyright (C) 2025 K. Urbański <karol.jakub.urbanski@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef _BROWSER_H_
#define _BROWSER_H_

#include "qcurses.h"

#define mouse_col(x) (int)((x / browserscale) / 8)
#define mouse_row(y) (int)((y / browserscale) / 8)

#define COL_MAP_WIDTH 20
#define COL_SKILL_WIDTH 6
#define COL_RECORD_MIN_WIDTH 15
#define COL_RECORD_HEIGHT 15

#define BROWSER_HIGHLIGHT_COLOR (0xdc << 16) + (0xdc << 8)

#define MW_SCROLL_LINES 3;

extern cvar_t demo_browser_vim;
extern cvar_t demo_browser_filter;

enum demos_tabs {
    TAB_LOCAL_DEMOS = 1,
    TAB_SDA_NEWS,
    TAB_SDA_DATABASE
};

enum browser_columns {
    COL_MAP = 1,
    COL_TYPE,
    COL_RECORD,
    COL_COMMENT_LOADING,
    COL_COMMENT_LOADED
};

enum map_filters {
    FILTER_DOWNLOADED,
    FILTER_ALL,
    FILTER_NEW,
    FILTER_ID,
};

void Browser_UpdateFurtherColumns (enum browser_columns start_column);

void Browser_CurlStart(char *path, char *href);
void Browser_CurlClean();

qboolean M_Browser_Mouse_Event(const mouse_state_t *ms);
void M_Browser_Draw(int width, int height);
void M_Browser_Key(int key);
void M_Demos_HelpBox (qcurses_box_t *help_box, enum demos_tabs tab, char *search_term, qboolean search_input);
char *GetPrintedTimeNoDec(float time, qboolean strip);

#endif /* _BROWSER_H_ */
