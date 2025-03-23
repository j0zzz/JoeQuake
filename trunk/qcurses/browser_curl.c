#include "../quakedef.h"
#include <curl/curl.h>
#include "browser_curl.h"

browser_curl_t *browser_curl_start(char *path, char *href) {
    browser_curl_t *curl = calloc(1, sizeof(browser_curl_t));
    curl->running = CURL_DOWNLOADING;
    curl->fp = fopen(path, "wb");
    curl->http_handle = curl_easy_init();
    curl_easy_setopt(curl->http_handle, CURLOPT_URL, href);
    curl_easy_setopt(curl->http_handle, CURLOPT_WRITEDATA, curl->fp);
    curl->multi_handle = curl_multi_init();
    curl_multi_add_handle(curl->multi_handle, curl->http_handle);
    return curl;
}

void browser_curl_clean(browser_curl_t *curl) {
    curl_multi_remove_handle(curl->multi_handle, curl->http_handle);
    curl_multi_cleanup(curl->multi_handle);
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
    curl_easy_getinfo(curl->http_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
    curl_easy_getinfo(curl->http_handle, CURLINFO_SIZE_DOWNLOAD_T, &downloaded);

    //if ((float)total < 0.0)
    //    return 0.0;
    //else
        return (float) downloaded / (float) total;
}
