/*
 * Module to draw the SDA news.
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
#include <curl/curl.h>
#include "browser_curl.h"

qcurses_char_t * news = NULL;
extern qcurses_box_t * main_box;

static int news_page = 0;

static browser_curl_t * curl = NULL;

/*
 * keyboard input handler for the news tab
 */
void M_Demos_KeyHandle_News(int k) {
    int distance = 1;
    switch (k) {
    case 'b':
        if (!keydown[K_CTRL])
            break;
    case K_PGUP:
    case K_HOME:
    case K_MWHEELUP:
        distance = keydown[K_HOME] ? 10000 : 10;
    case K_UPARROW:
    case 'k':
        if (news)
            news_page = max(0, news_page - distance);
        break;
    case 'd':
        if (!keydown[K_CTRL])
            break;
    case K_PGDN:
    case K_END:
    case K_MWHEELDOWN:
        distance = keydown[K_END] ? 10000 : 10;
    case K_DOWNARROW:
    case 'j':
        if (news)
            news_page = max(0, news_page + distance);
        break;
    case K_ENTER:
    case K_RIGHTARROW:
    case 'l':
        break;
    case K_BACKSPACE:
    case K_LEFTARROW:
    case 'h':
        break;
    case 'r':
        if (!keydown[K_CTRL])
            break;
    case K_F5:
        free(news);
        news = NULL;
        break;
    default:
        break;
    }
}

/*
 * display the news tab
 */
void M_Demos_DisplayNews (int cols, int rows, int start_col, int start_row) {
    qcurses_box_t * local_box = qcurses_init_paged(cols, rows - 10);
    qcurses_box_t * help_box  = qcurses_init(cols, rows - local_box->rows);
    if (!news) {
        if (!curl || !curl->running) { /* start downloading the news */
            qcurses_print_centered(local_box, local_box->rows / 2, "Downloading...", false);
            curl = browser_curl_start(NULL, "https://speeddemosarchive.com/quake/news.html");
            if (!curl)
                news = qcurses_parse_txt("Couldn't properly start download of news.html.");
        } else if (curl->running == CURL_DOWNLOADING) { /* perform download of the news */
            float progress = browser_curl_step(curl);
            qcurses_print_centered(local_box, local_box->rows / 2, "Downloading...", false);
            qcurses_print_centered(local_box, local_box->rows / 2 + 1, "\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82", false);
            qcurses_print(local_box, local_box->cols / 2 - 5 + (int)(10.0 * progress), local_box->rows / 2 + 1, "\x83", false);
        } else if (curl->running == CURL_FINISHED) { /* download finished, time to parse */
            news = qcurses_parse_news(curl->mem.buf);
            browser_curl_clean(curl);
            curl = NULL;
        }
    }

    M_Demos_HelpBox (help_box, TAB_SDA_NEWS, "", false);

    /* print the news */
    if (news) {
        int total_rows = qcurses_boxprint_wrapped(local_box, news, local_box->cols * local_box->rows, 0);
        local_box->paged = news_page = max(min(news_page, total_rows - local_box->rows), 0);

        if (total_rows > local_box->rows){
            for (int i = 0; i < local_box->rows; i++)
                qcurses_print(local_box, local_box->cols - 1, i + news_page, "\x06", false);

            qcurses_print(local_box, local_box->cols - 1, min(news_page + (local_box->rows - 1) * news_page / (total_rows - local_box->rows), local_box->page_rows - 1), "\x83", false);
        }
    }

    qcurses_insert(main_box, start_col, start_row, local_box);
    qcurses_insert(main_box, start_col, start_row + local_box->rows, help_box);
    qcurses_free(local_box);
    qcurses_free(help_box);
}
