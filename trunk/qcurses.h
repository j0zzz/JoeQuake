/*
 * Module to simplify drawing an interactive menu a la curses and 
 * ncurses. The module detects the amount of space available in the menu,
 * creates a grid of characters, and supports writing to this grid.
 *
 * Functions for auto-wrapping text are included.
 *
 * Also supported are background boxes, to mark certain selections.
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
#ifndef _QCURSES_H_
#define _QCURSES_H_

typedef struct qcurses_char_s {
    unsigned char symbol;
    long background;
    char decoration;
} qcurses_char_t;

typedef struct qcurses_box_s {
    qcurses_char_t ** grid;
    int cols;
    int rows;
} qcurses_box_t;

typedef struct qcurses_list_s {
    int cursor;
    int len;
    int places;
    int window_start;
    char (*array)[80];
} qcurses_list_t;

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

void M_Demos_Display(int width, int height);
void M_Demos_KeyHandle(int key);
qcurses_box_t * qcurses_init(int cols, int rows);
void qcurses_free(qcurses_box_t * box);
void qcurses_insert(qcurses_box_t * dest, int col, int row, qcurses_box_t * src);
void qcurses_print(qcurses_box_t * dest, int col, int row, char * src, qboolean bold);
void qcurses_display(qcurses_box_t * src);

#endif /* _QCURSES_H_ */
