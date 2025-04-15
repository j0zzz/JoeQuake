#include "../quakedef.h"
#include <curl/curl.h>
#include "browser_curl.h"

browser_curl_t *browser_curl_start(char *path, char *href) {
    browser_curl_t *curl = calloc(1, sizeof(browser_curl_t));
    if (!curl) {
        Con_Printf("curl allocation error!\n");
        return NULL;
    }

    curl->running = CURL_DOWNLOADING;
    ;
    if ( !(curl->fp = fopen(path, "wb")) ) {
        Con_Printf("curl file opening error!\n");
        return NULL;
    }

    if ( !(curl->http_handle = curl_easy_init()) ) {
        Con_Printf("curl_easy_init error!\n");
        return NULL;
    }

    int err;
    if ((err = curl_easy_setopt(curl->http_handle, CURLOPT_URL, href)))
        Con_Printf("curl_easy_setopt %d error!\n", err);
    if ((err = curl_easy_setopt(curl->http_handle, CURLOPT_WRITEDATA, curl->fp)))
        Con_Printf("curl_easy_setopt %d error!\n", err);

    if ( !(curl->multi_handle = curl_multi_init()) ) {
        Con_Printf("curl_multi_init error!\n");
        return NULL;
    }
    if ((err = curl_multi_add_handle(curl->multi_handle, curl->http_handle)))
        Con_Printf("curl_multi_add_handle error %d!\n", err);

    return curl;
}

void browser_curl_clean(browser_curl_t *curl) {
    CURLMcode err;
    if ((err = curl_multi_remove_handle(curl->multi_handle, curl->http_handle)))
        Con_Printf("curl_multi_remove_handle error %d!\n", err);

    if ((err = curl_multi_cleanup(curl->multi_handle)))
        Con_Printf("curl_multi_cleanup error %d!\n", err);

    curl_easy_cleanup(curl->http_handle);
    fclose(curl->fp);
    curl->fp = NULL;
    free(curl);
    curl = NULL;
}

float browser_curl_step(browser_curl_t *curl) {
    int run = 0;
    CURLMcode mc = curl_multi_perform(curl->multi_handle, &run);

    if(mc){
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
