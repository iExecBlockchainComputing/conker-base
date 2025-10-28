#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
// Log levels
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3

int app_log_level = LOG_LEVEL_INFO;  // Default to INFO level

#define LOG_WITH_TIMESTAMP(fmt, level, associated_level, ...) \
    do { \
        if (app_log_level <= associated_level) { \
            time_t now = time(NULL); \
            struct tm *t = gmtime(&now); \
            char ts[24]; \
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S UTC", t); \
            printf("%-29s [%-5s] [%s:%d] " fmt "\n", ts, level, __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
    } while (0)

#define LOG_DEBUG(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "DEBUG", LOG_LEVEL_DEBUG, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "INFO", LOG_LEVEL_INFO, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "WARN", LOG_LEVEL_WARN, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "ERROR", LOG_LEVEL_ERROR, ##__VA_ARGS__)


char* wrap_key = "";

int push_wrapkey_to_secret_box(const char* wrapkey) {
    CURL* curl;
    CURLcode res;
    char request_buffer[64];
    long http_code = 0;

    curl = curl_easy_init();
    if (curl) {
        // get token
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:9090/secret");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "http");

        strcpy(request_buffer, "key=wrapkey&value=");
        strcat(request_buffer, wrapkey);
        LOG_DEBUG("Request body is %s", request_buffer);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_buffer);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            LOG_ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));
            return -1;
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200) {
            LOG_ERROR("Verify quteo from azure failed, http code is %ld", http_code);
            return -1;
        }
        curl_easy_cleanup(curl);
        return 0;
    } else {
        LOG_ERROR("Init curl failed");
        return -1;
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    
    LOG_INFO("Try to get key from local");
    wrap_key = getenv("localKey");
    if (NULL == wrap_key) {
        LOG_ERROR("local-key does not config");
        return -1;
    }
    if (strlen(wrap_key) != 32) {
        LOG_ERROR("Key size is not 32 bytes, please check");
        return -1;
    }

    LOG_INFO("Get wrap_key successful from local");
    LOG_DEBUG("Wrapkey is %s", wrap_key);
    int ret = push_wrapkey_to_secret_box(wrap_key);
    if (ret != 0) {
        LOG_ERROR("Push wrapkey to secret box failed");
        return -1;
    }
    return 0;
}
