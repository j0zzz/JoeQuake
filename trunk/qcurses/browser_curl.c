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


#include "../quakedef.h"
#include <curl/curl.h>
#include "browser_curl.h"

#ifdef _WIN32
/*
 * Callback for writing files in windows. Needed for compat issues.
 */
static size_t win_cb(char *data, size_t size, size_t nmemb, void *clientp) {
    FILE *fh = (FILE *)clientp;

    return fwrite(data, size, nmemb, fh);
}
#endif

/*
 * Callback to download file directly to memory.
 */
static size_t mem_cb(char *data, size_t size, size_t nmemb, void *clientp) {
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)clientp;

    char *ptr = Q_realloc(mem->buf, mem->size + realsize + 1);
    if(!ptr)
        return CURL_WRITEFUNC_ERROR; /* out of memory */

    mem->buf = ptr;
    memcpy(&(mem->buf[mem->size]), data, realsize);
    mem->size += realsize;
    mem->buf[mem->size] = 0;

    return realsize;
}

/* 
 * Start the download using curl.
 */
browser_curl_t *browser_curl_start(char *path, char *href) {
    browser_curl_t *curl = Q_calloc(1, sizeof(browser_curl_t));
    int err;
    if (!curl) {
        Con_Printf("curl allocation error!\n");
        return NULL;
    }

    if ( !(curl->http_handle = curl_easy_init()) ) {
        Con_Printf("curl_easy_init error!\n");
        return NULL;
    }

    curl_easy_setopt(curl->http_handle, CURLOPT_URL, href);
    curl->running = CURL_DOWNLOADING;
    if (path) {
        if ( !(curl->fp = fopen(path, "wb")) ) {
            Con_Printf("curl file opening error!\n");
            return NULL;
        }
#ifdef _WIN32 /* needed to make sure code is portable */
        curl_easy_setopt(curl->http_handle, CURLOPT_WRITEFUNCTION, win_cb); 
#endif
        curl_easy_setopt(curl->http_handle, CURLOPT_WRITEDATA, curl->fp);
    } else {
        curl_easy_setopt(curl->http_handle, CURLOPT_WRITEFUNCTION, mem_cb);
        curl_easy_setopt(curl->http_handle, CURLOPT_WRITEDATA, (void *)(&(curl->mem)));
    }

    if ( !(curl->multi_handle = curl_multi_init()) ) {
        Con_Printf("curl_multi_init error!\n");
        return NULL;
    }
    if ((err = curl_multi_add_handle(curl->multi_handle, curl->http_handle)))
        Con_Printf("curl_multi_add_handle error %d!\n", err);

    return curl;
}

/*
 * Clean up after download.
 */
void browser_curl_clean(browser_curl_t *curl) {
    CURLMcode err;
    if ((err = curl_multi_remove_handle(curl->multi_handle, curl->http_handle)))
        Con_Printf("curl_multi_remove_handle error %d!\n", err);

    if ((err = curl_multi_cleanup(curl->multi_handle)))
        Con_Printf("curl_multi_cleanup error %d!\n", err);

    curl_easy_cleanup(curl->http_handle);
    if (curl->mem.buf) 
        free(curl->mem.buf);
    if (curl->fp)
        fclose(curl->fp);
    curl->fp = NULL;
    free(curl);
    curl = NULL;
}

/*
 * Progress a running download asynchronously, without the use of threads.
 */
float browser_curl_step(browser_curl_t *curl) {
    int run = 0;
    CURLMcode mc = curl_multi_perform(curl->multi_handle, &run);

    if (mc) {
        browser_curl_clean(curl);
        return -1.0;
    } else if (!run) {
        curl->running = CURL_FINISHED;
        return 1.0;
    }

    curl_off_t total, downloaded;
    if (curl_easy_getinfo(curl->http_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total))
        Con_Printf("curl_easy_getinfo error!\n");

    if (curl_easy_getinfo(curl->http_handle, CURLINFO_SIZE_DOWNLOAD_T, &downloaded))
        Con_Printf("curl_easy_getinfo error!\n");

    return (float) downloaded / (float) total;
}
