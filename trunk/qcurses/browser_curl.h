/*
 * Module to simplify downloads using curl for the SDA demo browser.
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

#ifndef _BROWSER_CURL_H_
#define _BROWSER_CURL_H_

enum download_state {
    CURL_ERROR = -1,
    CURL_FALSE = 0,
    CURL_DOWNLOADING = 1,
    CURL_FINISHED = 2
};

struct memory {
    char *buf;
    size_t size;
};
 
typedef struct browser_curl_s {
    enum download_state running;
    CURL *http_handle;
    CURLM *multi_handle;
    FILE *fp;
    struct memory mem;
} browser_curl_t;

browser_curl_t *browser_curl_start(char *path, char *href);
void browser_curl_clean(browser_curl_t *curl);
float browser_curl_step(browser_curl_t *curl);

#endif /* _BROWSER_CURL_H_ */
