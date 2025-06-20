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

#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "../quakedef.h"
#include "qcurses.h"

int comment_rows = 0;

/*
 * parse the text data into a grid
 */
qcurses_char_t * qcurses_parse_txt(char * txt){
    qcurses_char_t * qstr = Q_calloc(strlen(txt) + 1, sizeof(qcurses_char_t));
    int i = 0, j = 0, rows = 0;
    for (i = 0; i < strlen(txt); i++){
        qstr[j++].symbol = txt[i];
        if (txt[i] == '\n')
            rows++;
    }
    qstr[j].symbol = 0;

    free(txt);
    comment_rows = rows;
    return qstr;
}

/*
 * parse the news html into a nice, readable form
 * TODO: add a way to display a menu with demos
 */
qcurses_char_t * qcurses_parse_news(char * html){
    qboolean tag = false;
    int color = 0;
    int i = 0, j = 0;
    char * src = Q_strcasestr(html, "<p class=\"d\">");
    char * end = Q_strcasestr(src + 1, "<p class=\"d\">");
    qcurses_char_t * qstr = Q_calloc(strlen(html), sizeof(qcurses_char_t));
    for (i = 0; i < end - src; i++){
        if( src[i] == '<'){
            tag = true;
            if(!Q_strncasecmp(src + i, "<p class=\"d\"", 12) || !Q_strncasecmp(src + i, "<p class=\"s\"", 12) || !Q_strncasecmp(src + i, "<span class=\"y\">", 16)) {
                color = 128;
            } else if (!Q_strncasecmp(src + i, "<a href=\"/quake/mkt.pl?level", 28)) {
                color = 128;
            } else if (!Q_strncasecmp(src + i, "<li>", 4)) {
                qstr[j++].symbol = 11 + 128;
                qstr[j++].symbol = ' ';
            } else if (!Q_strncasecmp(src + i, "</p>", 4)) {
                color = 0;
                qstr[j++].symbol = '\n';
            } else if (!Q_strncasecmp(src + i, "</span>", 7) || !Q_strncasecmp(src + i, "</a>", 4)) {
                color = 0;
            }
        }
        if(!tag)
            qstr[j++].symbol = src[i] + color;
        if( src[i] == '>')
            tag = false;
    }
    qstr[j].symbol = 0;

    return qstr;
}

/*
 * initialize a box with no callback
 */
qcurses_box_t * qcurses_init(int cols, int rows) {
    return qcurses_init_callback(cols, rows, NULL);
}

/*
 * initialize a box with callback attached. callbacks can be switched
 * out directly when needed.
 */
qcurses_box_t * qcurses_init_callback(int cols, int rows, void (*callback)(qcurses_char_t * self, const mouse_state_t * ms)) {
    qcurses_box_t * box = Q_malloc(sizeof(qcurses_box_t));

    box->cols = cols;
    box->rows = box->page_rows = rows;
    box->paged = -1;

    box->grid = Q_calloc(rows, sizeof(qcurses_char_t *));
    for (int i = 0; i < rows; i++){
        box->grid[i] = Q_calloc(cols, sizeof(qcurses_char_t));
        for (int j = 0; j < cols; j++) {
            box->grid[i][j].symbol = ' ';
            box->grid[i][j].callback = callback;
            box->grid[i][j].callback_data.row = i;
            box->grid[i][j].callback_data.col = j;
        }
    }

    return box;
}

/*
 * initialize a paged box, with no callback.
 */
qcurses_box_t * qcurses_init_paged(int cols, int rows) {
    qcurses_box_t * box = Q_malloc(sizeof(qcurses_box_t));

    box->cols = cols;
    box->rows = rows;
    box->page_rows = 16 * rows;
    box->paged = 0;

    box->grid = Q_calloc(16 * rows, sizeof(qcurses_char_t *));
    for (int i = 0; i < 16 * rows; i++){
        box->grid[i] = Q_calloc(cols, sizeof(qcurses_char_t));
        for (int j = 0; j < cols; j++)
            box->grid[i][j].symbol = ' ';
    }

    return box;
}

/*
 * free a box
 */
void qcurses_free(qcurses_box_t *box){
    if (!box)
        return;

    for (int i = 0; i < box->page_rows; i++)
        free(box->grid[i]);
    free(box->grid);
    free(box);
    box = NULL;
}

/*
 * display a box
 */
void qcurses_display(qcurses_box_t *src){
    int i, j;
    for (i = 0; i < src->rows; i++) {
        for (j = 0; j < src->cols; j++) {
            Draw_Character(j * 8, i * 8, src->grid[i][j].symbol, false);
        }
    }
}

/*
 * print a standard string into a box, wrapping when necessary.
 */
int qcurses_boxprint_wrapped(qcurses_box_t *dest, qcurses_char_t *src, size_t size, int row_offset){
    int row = row_offset, col = 0;
    for(int i = 0; i < size && (src + i)->symbol; i++){
        if ((src + i)->symbol == '\n' || col >= dest->cols - 1){
            col = 0;
            row++;
        }
        if ((src + i)->symbol != '\n' && (src + i)->symbol != '\r' && row < dest->page_rows)
            memcpy(dest->grid[row] + col++, src + i, sizeof(qcurses_char_t));
    }

    return row;
}

/*
 * print characters to a box, with no callback 
 */
void qcurses_print(qcurses_box_t *dest, int col, int row, char *src, qboolean bold){
    qcurses_print_callback(dest, col, row, src, bold, NULL);
}

/*
 * print characters to a box, with mouse callback 
 */
void qcurses_print_callback(qcurses_box_t * dest, int col, int row, char * src, qboolean bold, void (*callback)(qcurses_char_t * self, const mouse_state_t * ms)) {
    if (!dest)
        return;

    if (row >= dest->page_rows)
        return;

    for(int i = 0; i < strlen(src) && i + col < dest->cols; i++) {
        dest->grid[row][col + i].symbol = src[i] | (bold ? 128 : 0);
        if (callback || !dest->grid[row][col + i].callback)
            dest->grid[row][col + i].callback = callback;
    }
}

/*
 * print characters, centered in a box.
 */
void qcurses_print_centered(qcurses_box_t *dest, int row, char *src, qboolean bold){
    if (!dest)
        return;

    int col = (dest->cols - strlen(src)) / 2;
    if (col >= 0)
        qcurses_print(dest, col, row, src, bold);
    else 
        qcurses_print(dest, 0, row, src, bold);
}

/*
 * composition of boxes by copying one into another
 */
void qcurses_insert(qcurses_box_t *dest, int col, int row, qcurses_box_t *src) {
    if (!src || !dest)
        return;

    int page_offset = max(src->paged, 0);
    for (int i = row; i < row + src->page_rows && i < dest->rows; i++){
        memcpy(dest->grid[i] + col, src->grid[i - row + page_offset], sizeof(qcurses_char_t) * min(src->cols, dest->cols - col));
    }
}

/*
 * helper function to move cursor in a list
 */
void qcurses_list_move_cursor(qcurses_list_t *col, int move) {
    if (col) {
        col->cursor = max(min(col->cursor + move, col->len - 1), 0);
        if (col->cursor >= col->window_start + col->places)
            col->window_start = col->cursor - col->places + 1;
        if (col->cursor < col->window_start)
            col->window_start = col->cursor;
    }
}

/*
 * helper function to draw a horizontal bar
 */
void qcurses_make_bar(qcurses_box_t * box, int row){
    if (!box)
        return;

    for (int i = 0; i < box->cols; i++)
        qcurses_print(box, i, row, "\x1e", true);
    qcurses_print(box, 0, row, "\x1d", true);
    qcurses_print(box, box->cols - 1, row, "\x1f", true);
}
