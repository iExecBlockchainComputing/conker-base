#include <arpa/inet.h>
#include <curl/curl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <rats-tls/api.h>
#include <rats-tls/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <jansson.h>
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

#define DEFAULT_PORT 1234
#define DEFAULT_IP "127.0.0.1"

const char* command_get_key = "getKey";
char* wrap_key = "";
char* get_key_from_kbs_through_rats_tls(rats_tls_log_level_t log_level,
                                        char* attester_type,
                                        char* verifier_type,
                                        char* tls_type,
                                        char* crypto_type,
                                        bool mutual,
                                        char* ip,
                                        int port,
                                        char* app_id) {
    rats_tls_conf_t conf;

    memset(&conf, 0, sizeof(conf));
    claim_t custom_claims[1];
    if (NULL != app_id) {
        custom_claims[0].name = "appId";
        custom_claims[0].value = (uint8_t *) app_id;
        custom_claims[0].value_size = strlen(app_id);
        conf.custom_claims = (claim_t *)custom_claims;
        conf.custom_claims_length = 1;
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
    printf("app2 id is %s\n", app_id);
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

    msg = command_get_key;

    size_t len = strlen(msg);
    ret = rats_tls_transmit(handle, (void*)msg, &len);
    if (ret != RATS_TLS_ERR_NONE || len != strlen(msg)) {
        LOG_ERROR("Failed to transmit %#x\n", ret);
        goto err;
    }
    int buff_size = 255;
    char* buf = malloc(buff_size);
    len = buff_size;
    ret = rats_tls_receive(handle, buf, &len);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to receive %#x\n", ret);
        goto err;
    }

    if (len != 32) {
        LOG_ERROR("get key failed, error: the key size is not 16byte\n");
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

int push_wrapkey_to_secret_box(const char* wrapkey) {
    CURL* curl;
    CURLcode res;
    char request_buffer[1024 * 64];
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

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    json_t *root;
    json_error_t error;
    root = json_loads(contents, 0, &error);
    if (NULL == root) {
        LOG_ERROR("get sealing key failed, please check if attest-helper server is ready");
    }

    json_t *j_code = json_object_get(root,"code");
    int code = json_integer_value(j_code); 
    if (200 == code) {
        json_t *j_sealingkey = json_object_get(root, "sealingKey");
        wrap_key = json_string_value(j_sealingkey);
    } else {
        LOG_ERROR("get sealing key failed, please retry, error code: %d", code);
    }

    return realsize;
}

int get_sealing_key() {
    CURL* curl;
    CURLcode res;
    long http_code = 0;

    curl = curl_easy_init();
    if (curl) {
        // get token
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:8080/v1/attest/sealingkey");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            LOG_ERROR("curl_easy_perform() failed: %s please check if attest-helper server is ready\n", curl_easy_strerror(res));
            return -1;
        }
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200) {
            LOG_ERROR("get sealing key failed, please check if attest-helper server is ready; %ld\n", http_code);
            return -1;
        curl_easy_cleanup(curl);
        return 0;
        }
    } else {
        LOG_ERROR("init curl failed\n");
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char* mode;
    mode = getenv("mode");
    if (NULL == mode) {
        LOG_ERROR("key provider mode doest not config, support mode: 'local' or 'kbs'\n");
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
    } else if (!strcmp(mode, "sealing")) {
        int ret = get_sealing_key();
        if (ret) {
            LOG_ERROR("get sealing key faield\n");
            return -1;
        }
    }else if (!strcmp(mode, "kbs")) {
        LOG_INFO("try to get key from kbs\n");
        char* kbs_endpoint = getenv("kbsEndpoint");
        if (NULL == kbs_endpoint) {
            LOG_ERROR("kbs mode must config kbsEndpoint\n");
            return -1;
        }

        LOG_DEBUG("config of kbsEndpoint is %s", kbs_endpoint);

        char* srv_ip;
        char* str_port;
        int port;

        srv_ip = strtok(kbs_endpoint, ":");
        str_port = strtok(NULL, ":");
        if (NULL == str_port) {
            LOG_ERROR("kbsEndpoint format error, eg: 127.0.0.1:5443\n");
            return -1;
        }
        port = atoi(str_port);

        char* app_id = getenv("appId");

        char* const short_options = "a:v:t:c:ml:DEh";
        struct option long_options[] = {
            {"attester", required_argument, NULL, 'a'},
            {"verifier", required_argument, NULL, 'v'},
            {"tls", required_argument, NULL, 't'},
            {"crypto", required_argument, NULL, 'c'},
            {"mutual", no_argument, NULL, 'm'},
            {"log-level", required_argument, NULL, 'l'},
            {"help", no_argument, NULL, 'h'},
            {0, 0, 0, 0}};

        char* attester_type = "";
        char* verifier_type = "";
        char* tls_type = "";
        char* crypto_type = "";
        bool mutual = true;
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
                        "        --help/-h             show the usage\n");
                    exit(-1);
                default:
                    exit(-1);
            }
        } while (opt != -1);

        global_log_level = log_level;
        app_log_level = log_level;

        wrap_key = get_key_from_kbs_through_rats_tls(log_level, attester_type, verifier_type,
                                                     tls_type, crypto_type, mutual, srv_ip,
                                                     port, app_id);
        if (NULL == wrap_key) {
            LOG_ERROR("get key from kbs failed\n");
            return -1;
        }
    } else {
        LOG_ERROR("key provider mode only support 'local' or 'kbs' or 'sealing'");
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
