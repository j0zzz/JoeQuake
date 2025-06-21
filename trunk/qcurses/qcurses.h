/*
 * qcurses: module for handling a complex, composited menu.
 *
 * Works in a manner similar to curses/ncurses, where geometric ASCII
 * displays that are easy to make like lists are composited together
 * into a window.
 *
 * The module subdivides the screen into a grid of characters. Each of
 * these characters is stored in a box. Each of the characters can have
 * mouse handlers attached.
 *
 * The boxes are then composited together into bigger boxes by a straight
 * copy. This means we can define behavior locally, per window "pane",
 * but display in one fell swoop and handle everything we need regarding
 * display and input at once.
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

#include "../ghost/demosummary.h"

#define blink(ch) (((int)(realtime * 4) & 1) ? ch : ' ')
#define blinkstr(ch) (((int)(realtime * 4) & 1) ? va("%c", ch) : " ")

/* data for callbacks */
typedef struct qcurses_callback_data_s {
    int row;
    int col;
} qcurses_callback_data_t;

/* single char, with callback support */
typedef struct qcurses_char_s {
    unsigned char symbol;
    long background;
    char decoration;
    qcurses_callback_data_t callback_data;
    void (*callback)(struct qcurses_char_s * self, const mouse_state_t * ms);
} qcurses_char_t;

/* a grid of characters */
typedef struct qcurses_box_s {
    qcurses_char_t ** grid;
    int cols;
    int rows;
    int paged;
    int page_rows;
} qcurses_box_t;

/* a list helper structure */
typedef struct qcurses_list_s {
    int cursor;
    int len;
    int places;
    int window_start;
} qcurses_list_t;

/* extending previous list, a record list */
typedef struct qcurses_recordlist_s {
    qcurses_list_t list;
    char (*array)[80];
    char (*sda_name)[50];
} qcurses_recordlist_t;

/* extending list, a demo list */
typedef struct qcurses_demlist_s {
    qcurses_list_t list;
    direntry_t * entries;
    demo_summary_t ** summaries;
} qcurses_demlist_t;

qcurses_char_t * qcurses_parse_txt(char * txt);
qcurses_char_t * qcurses_parse_news(char * html);
qcurses_box_t * qcurses_init(int cols, int rows);
qcurses_box_t * qcurses_init_callback(int cols, int rows, void (*callback)(qcurses_char_t * self, const mouse_state_t * ms));
qcurses_box_t * qcurses_init_paged(int cols, int rows);
void qcurses_free(qcurses_box_t * box);
void qcurses_insert(qcurses_box_t * dest, int col, int row, qcurses_box_t * src);
void qcurses_print(qcurses_box_t * dest, int col, int row, char * src, qboolean bold);
void qcurses_print_callback(qcurses_box_t * dest, int col, int row, char * src, qboolean bold, void (*callback)(qcurses_char_t * self, const mouse_state_t * ms));
void qcurses_print_centered(qcurses_box_t * dest, int row, char * src, qboolean bold);
void qcurses_display(qcurses_box_t * src);
int qcurses_boxprint_wrapped(qcurses_box_t *dest, qcurses_char_t *src, size_t size, int row_offset);
void qcurses_list_move_cursor(qcurses_list_t *col, int move);
void qcurses_make_bar(qcurses_box_t * box, int row);

#endif /* _QCURSES_H_ */
