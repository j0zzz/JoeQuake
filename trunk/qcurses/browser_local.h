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
#ifndef _BROWSER_LOCAL_H_
#define _BROWSER_LOCAL_H_

typedef struct thread_data_s {
    char path[MAX_OSPATH];
    demo_summary_t * summary;
} thread_data_t;

int get_summary_thread(void * entry);

void M_Demos_LocalRead(int rows, char * prevdir);
void M_Demos_DisplayLocal (int cols, int rows, int start_col, int start_row);
void M_Demos_KeyHandle_Local_Search (int k, int max_lines);
void M_Demos_KeyHandle_Local (int k, int max_lines);

#endif /* _BROWSER_LOCAL_H_ */
