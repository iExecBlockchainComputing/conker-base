#include <arpa/inet.h>
#include <curl/curl.h>
#include <getopt.h>
#include <jansson.h>
#include <netinet/in.h>
#include <rats-tls/api.h>
#include <rats-tls/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
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

#define LOG_DEBUG(...)                              \
    do {                                            \
        if (app_log_level == 0) {                   \
            TIMEPRINT                               \
            printf("[DEBUG]");                      \
            printf("[%s:%d] ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                    \
        }                                           \
    } while (0);

#define LOG_WARNING(...)                        \
    do {                                        \
        TIMEPRINT                               \
        printf("[WARNING]");                    \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                    \
    } while (0);

#define LOG_ERROR(...)                          \
    do {                                        \
        TIMEPRINT                               \
        printf("[ERROR]");                      \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                    \
    } while (0);

#define LOG_INFO(...)                           \
    do {                                        \
        TIMEPRINT                               \
        printf("[INFO]");                       \
        printf("[%s:%d] ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                    \
    } while (0);

#define DEFAULT_PORT 1234
#define DEFAULT_IP "127.0.0.1"

const char* command_get_secret = "getSecret";

char* get_secret_from_kbs_through_rats_tls(rats_tls_log_level_t log_level,
                                           char* attester_type,
                                           char* verifier_type,
                                           char* tls_type,
                                           char* crypto_type,
                                           bool mutual,
                                           char* ip,
                                           int port,
                                           bool appid_flag) {
    rats_tls_conf_t conf;

    memset(&conf, 0, sizeof(conf));

    char* app_id;
    claim_t custom_claims[1];
    if (appid_flag) {
        app_id = getenv("appId");
        if (NULL != app_id) {
            custom_claims[0].name = "appId";
            custom_claims[0].value = (uint8_t*)app_id;
            custom_claims[0].value_size = strlen(app_id);
            conf.custom_claims = (claim_t*)custom_claims;
            conf.custom_claims_length = 1;
        } else {
            LOG_ERROR("could not read the app_id from env\n");
            return NULL;
        }
    }

    conf.log_level = log_level;
    strcpy(conf.attester_type, attester_type);
    strcpy(conf.verifier_type, verifier_type);
    strcpy(conf.tls_type, tls_type);
    strcpy(conf.crypto_type, crypto_type);
    conf.cert_algo = RATS_TLS_CERT_ALGO_DEFAULT;
    if (mutual)
        conf.flags |= RATS_TLS_CONF_FLAGS_MUTUAL;

    /* Create a socket that uses an internet IPv4 address,
     * Sets the socket to be stream based (TCP),
     * 0 means choose the default protocol.
     */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("failed to call socket()\n");
        return NULL;
    }
    struct sockaddr_in s_addr;
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);

    /* Get the server IPv4 address from the command line call */
    if (inet_pton(AF_INET, ip, &s_addr.sin_addr) != 1) {
        LOG_ERROR("invalid server address\n");
        return NULL;
    }

    /* Connect to the server */
    if (connect(sockfd, (struct sockaddr*)&s_addr, sizeof(s_addr)) == -1) {
        LOG_ERROR("failed to call connect()\n");
        return NULL;
    }
    rats_tls_handle handle;
    rats_tls_err_t ret = rats_tls_init(&conf, &handle);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to initialize rats tls %#x\n", ret);
        return NULL;
    }
    ret = rats_tls_set_verification_callback(&handle, NULL);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to set verification callback %#x\n", ret);
        return NULL;
    }
    ret = rats_tls_negotiate(handle, sockfd);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to negotiate %#x\n", ret);
        goto err;
    }
    const char* msg;
    if (appid_flag) {
        msg = app_id;
    } else {
        msg = command_get_secret;
    }

    size_t len = strlen(msg);
    ret = rats_tls_transmit(handle, (void*)msg, &len);
    if (ret != RATS_TLS_ERR_NONE || len != strlen(msg)) {
        LOG_ERROR("Failed to transmit %#x\n", ret);
        goto err;
    }
    int buff_size = 4096;
    char* buf = malloc(buff_size);
    len = buff_size;
    ret = rats_tls_receive(handle, buf, &len);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to receive %#x\n", ret);
        goto err;
    }

    if (len >= buff_size)
        len = buff_size - 1;
    buf[len] = '\0';

    ret = rats_tls_cleanup(handle);
    if (ret != RATS_TLS_ERR_NONE)
        LOG_ERROR("Failed to cleanup %#x\n", ret);

    return buf;

err:
    /* Ignore the error code of cleanup in order to return the prepositional
     * error */
    rats_tls_cleanup(handle);
    return NULL;
}

int push_wrapkey_to_secret_box(const char* secret) {
    json_error_t error;
    json_t* json_secret = json_loads(secret, 0, &error);
    if (!json_secret) {
        LOG_ERROR("secret json format error\n");
        return -1;
    }

    void* iter = json_object_iter(json_secret);
    while (iter != NULL) {
        const char* key = json_object_iter_key(iter);       //
        json_t* json_value = json_object_iter_value(iter);  //
        if (json_value == NULL) {
            printf("json_object_iter_value fail\n");
            return -1;
        }
        const char* value;
        switch (json_typeof(json_value)) {
            case JSON_STRING: {
                value = json_string_value(json_value);
                break;
            }
            default: {
                LOG_WARNING("the value of %s is not string, not support yet\n", key);
            }
        }
        LOG_DEBUG("key is %s, value is %s\n", key, value);

        iter = json_object_iter_next(json_secret, iter);

        CURL* curl;
        CURLcode res;
        char request_buffer[1024 * 64] = {0};
        long http_code = 0;
        curl = curl_easy_init();
        if (curl) {
            // get token
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
            curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:9090/secret");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "http");

            strcat(request_buffer, "key=");
            strcat(request_buffer, key);
            strcat(request_buffer, "&value=");
            strcat(request_buffer, value);
            LOG_DEBUG("request body is %s\n", request_buffer);

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
        } else {
            LOG_ERROR("init curl failed\n");
            return -1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char* secret = "";
    LOG_INFO("try to get key from kbs\n");
    char* kbs_endpoint = getenv("kbsEndpoint");
    if (NULL == kbs_endpoint) {
        LOG_ERROR("kbs mode must config kbsEndpoint\n");
        return -1;
    }

    LOG_DEBUG("config of kbsEndpoint is %s\n", kbs_endpoint);

    char* secret_save_path = NULL;
    char* srv_ip = NULL;
    char* str_port = NULL;
    int port;

    srv_ip = strtok(kbs_endpoint, ":");
    str_port = strtok(NULL, ":");
    if (NULL == str_port) {
        LOG_ERROR("kbsEndpoint format error, eg: 127.0.0.1:5443\n");
        return -1;
    }
    port = atoi(str_port);
    char* const short_options = "a:v:t:c:ml:DEhfs:";
    struct option long_options[] = {
        {"attester", required_argument, NULL, 'a'},
        {"verifier", required_argument, NULL, 'v'},
        {"tls", required_argument, NULL, 't'},
        {"crypto", required_argument, NULL, 'c'},
        {"mutual", no_argument, NULL, 'm'},
        {"log-level", required_argument, NULL, 'l'},
        {"appId", required_argument, NULL, 'f'},
        {"savePath", no_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}};

    char* attester_type = "";
    char* verifier_type = "";
    char* tls_type = "";
    char* crypto_type = "";
    bool mutual = true;
    bool appid_flag = false;
    int opt;
    rats_tls_log_level_t log_level = RATS_TLS_LOG_LEVEL_INFO;
    do {
        opt = getopt_long(argc, argv, short_options, long_options, NULL);
        switch (opt) {
            case 'a':
                attester_type = optarg;
                break;
            case 'v':
                verifier_type = optarg;
                break;
            case 't':
                tls_type = optarg;
                break;
            case 'c':
                crypto_type = optarg;
                break;
            case 'l':
                if (!strcasecmp(optarg, "debug"))
                    log_level = RATS_TLS_LOG_LEVEL_DEBUG;
                else if (!strcasecmp(optarg, "info"))
                    log_level = RATS_TLS_LOG_LEVEL_INFO;
                else if (!strcasecmp(optarg, "warn"))
                    log_level = RATS_TLS_LOG_LEVEL_WARN;
                else if (!strcasecmp(optarg, "error"))
                    log_level = RATS_TLS_LOG_LEVEL_ERROR;
                else if (!strcasecmp(optarg, "fatal"))
                    log_level = RATS_TLS_LOG_LEVEL_FATAL;
                else if (!strcasecmp(optarg, "off"))
                    log_level = RATS_TLS_LOG_LEVEL_NONE;
                break;
            case 'f':
                appid_flag = true;
                break;
            case 's':
                secret_save_path = optarg;
                break;
            case -1:
                break;
            case 'h':
                puts(
                    "    Usage:\n\n"
                    "        rats-tls-client <options> [arguments]\n\n"
                    "    Options:\n\n"
                    "        --attester/-a value   set the type of quote attester\n"
                    "        --verifier/-v value   set the type of quote verifier\n"
                    "        --tls/-t value        set the type of tls wrapper\n"
                    "        --crypto/-c value     set the type of crypto wrapper\n"
                    "        --mutual/-m           set to enable mutual attestation\n"
                    "        --log-level/-l        set the log level\n"
                    "        --ip/-i               set the listening ip address\n"
                    "        --port/-p             set the listening tcp port\n"
                    "        --debug-enclave/-D    set to enable enclave debugging\n"
                    "        --verdictd/-E         set to connect verdictd based on EAA protocol\n"
                    "        --appId/-f            need to add appid to claims\n"
                    "        --savePath/-s         save secret to local path"
                    "        --help/-h             show the usage\n");
                exit(-1);
            default:
                exit(-1);
        }
    } while (opt != -1);

    global_log_level = log_level;
    app_log_level = log_level;

    secret = get_secret_from_kbs_through_rats_tls(log_level, attester_type, verifier_type,
                                                  tls_type, crypto_type, mutual, srv_ip,
                                                  port, appid_flag);
    if (NULL == secret) {
        LOG_ERROR("get secret from kbs failed\n");
        return -1;
    }

    LOG_INFO("get secret successful \n");
    LOG_DEBUG("secret is %s\n", secret);

    if (secret_save_path != NULL) {
        FILE* file = fopen(secret_save_path, "w");
        if (file == NULL) {
            LOG_ERROR("Failed to open the file %s\n", secret_save_path);
            return -1;
        }
        fputs(secret, file);
        fclose(file);
    } else {
        int ret = push_wrapkey_to_secret_box(secret);
        if (ret != 0) {
            LOG_ERROR("push wrapkey to secret box failed\n")
            return -1;
        }
    }

    return 0;
}
