#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
int app_log_level = -1;
#define TIMEPRINT                                                  \
    do {                                                           \
        struct timeval now;                                        \
        struct tm* ptime = NULL;                                   \
        gettimeofday(&now, NULL);                                  \
        ptime = gmtime(&now.tv_sec);                               \
        printf("[%d/%d/%d %02d:%02d:%02d]", 1900 + ptime->tm_year, \
               1 + ptime->tm_mon, ptime->tm_mday, ptime->tm_hour,  \
               ptime->tm_min, ptime->tm_sec);                      \
    } while (0);

#define LOG_DEBUG(...)                          \
    do {                                        \
        if (app_log_level) {                    \
            break;                              \
        }                                       \
        TIMEPRINT                               \
        printf("[Debug]");                      \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                    \
    } while (0);

#define LOG_WARING(...)                         \
    do {                                        \
        TIMEPRINT                               \
        printf("[Waring]");                     \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                    \
                                                \
    } while (0);

#define LOG_ERROR(...)                          \
    do {                                        \
        TIMEPRINT                               \
        printf("[Error]");                      \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                    \
    } while (0);

#define LOG_INFO(...)                           \
    do {                                        \
        TIMEPRINT                               \
        printf("[Info]");                       \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                    \
    } while (0);


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

        strcat(request_buffer, "key=wrapkey&value=");
        strcat(request_buffer, wrapkey);
        LOG_DEBUG("request body is %s\n", request_buffer)

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_buffer);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            LOG_ERROR("curl_easy_perform() failed: %s \n", curl_easy_strerror(res));
            return -1;
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200) {
            LOG_ERROR("verify quteo from azure failed, http code is ;%ld\n", http_code);
            return -1;
        }
        curl_easy_cleanup(curl);
        return 0;
    } else {
        LOG_ERROR("init curl failed\n");
        return -1;
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char* mode;
    mode = getenv("mode");
    if (NULL == mode) {
        LOG_ERROR("key provider mode doest not config, support mode: 'local'\n");
        return -1;
    }

    if (!strcmp(mode, "local")) {
        LOG_INFO("try to get key from local\n");
        wrap_key = getenv("localKey");
        if (NULL == wrap_key) {
            LOG_ERROR("local-key doest not config\n");
            return -1;
        }
        if (strlen(wrap_key) != 32) {
            LOG_ERROR("key size is not 16byte,please check\n");
            return -1;
        }
    }  else {
        LOG_ERROR("key provider mode only support 'local'");
        return -1;
    }

    LOG_INFO("get wrap_key successful from %s\n", mode);
    LOG_DEBUG("wrapkey is %s\n", wrap_key);
    int ret = push_wrapkey_to_secret_box(wrap_key);
    if (ret != 0) {
        LOG_ERROR("push wrapkey to secret box failed\n")
        return -1;
    }
    return 0;
}
