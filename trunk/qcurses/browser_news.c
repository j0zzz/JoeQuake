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

#include "qcurses.h"
#include "browser.h"

qcurses_char_t * news = NULL;
extern qcurses_box_t * main_box;

void M_Demos_KeyHandle_News(int k) {
    return;
}

void M_Demos_DisplayNews (int cols, int rows, int start_col, int start_row) {
    qcurses_box_t * local_box = qcurses_init(cols - , rows);

    if (!news)
        news = qcurses_parse_news(browser_read_file("news.html"));

    qcurses_boxprint_wrapped(local_box, news, local_box->cols * local_box->rows, 0);
    qcurses_insert(main_box, start_col, start_row, local_box);

    qcurses_free(local_box);
}
