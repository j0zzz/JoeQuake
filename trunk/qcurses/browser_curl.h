#ifndef _BROWSER_CURL_H_
#define _BROWSER_CURL_H_

enum download_state {
    CURL_ERROR = -1,
    CURL_FALSE = 0,
    CURL_DOWNLOADING = 1,
    CURL_FINISHED = 2
};

typedef struct browser_curl_s {
    enum download_state running;
    CURL *http_handle;
    CURLM *multi_handle;
    FILE *fp;
} browser_curl_t;

browser_curl_t *browser_curl_start(char *path, char *href);
void browser_curl_clean(browser_curl_t *curl);
float browser_curl_step(browser_curl_t *curl);

#endif /* _BROWSER_CURL_H_ */
