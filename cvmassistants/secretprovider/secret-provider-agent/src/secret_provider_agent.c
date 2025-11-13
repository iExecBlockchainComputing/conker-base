#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <rats-tls/api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PORT 1234
#define DEFAULT_IP "127.0.0.1"
// size of session
#define MAX_SESSION_SIZE (100 * 1024 * 1024)  // 100 MB
#define CHUNK_SIZE 4096

#define LOG_WITH_TIMESTAMP(fmt, level, rats_level, ...) \
    do { \
        if (log_level <= rats_level) { \
            time_t now = time(NULL); \
            struct tm *t = gmtime(&now); \
            char ts[24]; \
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S UTC", t); \
            printf("%-29s [%-5s] [%s:%d] " fmt "\n", ts, level, __FILE__, __LINE__, ##__VA_ARGS__); \
        } \
    } while (0)

#define LOG_DEBUG(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "DEBUG", RATS_TLS_LOG_LEVEL_DEBUG, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "INFO", RATS_TLS_LOG_LEVEL_INFO, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "WARN", RATS_TLS_LOG_LEVEL_WARN, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "ERROR", RATS_TLS_LOG_LEVEL_ERROR, ##__VA_ARGS__)

rats_tls_log_level_t log_level = RATS_TLS_LOG_LEVEL_INFO;

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

    bool validation_error = false;
    if (attester_type == NULL || strlen(attester_type) >= ENCLAVE_ATTESTER_TYPE_NAME_SIZE) {
        LOG_ERROR("attester_type is NULL or exceeds maximum allowed size (%d)",
            ENCLAVE_ATTESTER_TYPE_NAME_SIZE - 1);
        validation_error = true;
    }

    if (verifier_type == NULL || strlen(verifier_type) >= ENCLAVE_VERIFIER_TYPE_NAME_SIZE) {
        LOG_ERROR("verifier_type is NULL or exceeds maximum allowed size (%d)",
            ENCLAVE_VERIFIER_TYPE_NAME_SIZE - 1);
        validation_error = true;
    }

    if (tls_type == NULL || strlen(tls_type) >= TLS_TYPE_NAME_SIZE) {
        LOG_ERROR("tls_type is NULL or exceeds maximum allowed size (%d)",
            TLS_TYPE_NAME_SIZE - 1);
        validation_error = true;
    }

    if (crypto_type == NULL || strlen(crypto_type) >= CRYPTO_TYPE_NAME_SIZE) {
        LOG_ERROR("crypto_type is NULL or exceeds maximum allowed size (%d)",
            CRYPTO_TYPE_NAME_SIZE - 1);
        validation_error = true;
    }

    if (validation_error) {
        return NULL;
    }
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
            LOG_ERROR("Could not read the app_id from env");
            return NULL;
        }
    }

    conf.log_level = log_level;
    strncpy(conf.attester_type, attester_type, ENCLAVE_ATTESTER_TYPE_NAME_SIZE - 1);
    conf.attester_type[ENCLAVE_ATTESTER_TYPE_NAME_SIZE - 1] = '\0';
    strncpy(conf.verifier_type, verifier_type, ENCLAVE_VERIFIER_TYPE_NAME_SIZE - 1);
    conf.verifier_type[ENCLAVE_VERIFIER_TYPE_NAME_SIZE - 1] = '\0';
    strncpy(conf.tls_type, tls_type, TLS_TYPE_NAME_SIZE - 1);
    conf.tls_type[TLS_TYPE_NAME_SIZE - 1] = '\0';
    strncpy(conf.crypto_type, crypto_type, CRYPTO_TYPE_NAME_SIZE - 1);
    conf.crypto_type[CRYPTO_TYPE_NAME_SIZE - 1] = '\0';
    conf.cert_algo = RATS_TLS_CERT_ALGO_DEFAULT;
    if (mutual)
        conf.flags |= RATS_TLS_CONF_FLAGS_MUTUAL;

    /* Create a socket that uses an internet IPv4 address,
     * Sets the socket to be stream based (TCP),
     * 0 means choose the default protocol.
     */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("Failed to call socket()");
        return NULL;
    }
    struct sockaddr_in s_addr;
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);

    /* Get the server IPv4 address from the command line call */
    if (inet_pton(AF_INET, ip, &s_addr.sin_addr) != 1) {
        LOG_ERROR("Invalid server address");
        close(sockfd);
        return NULL;
    }

    /* Connect to the server */
    if (connect(sockfd, (struct sockaddr*)&s_addr, sizeof(s_addr)) == -1) {
        LOG_ERROR("Failed to call connect()");
        close(sockfd);
        return NULL;
    }
    rats_tls_handle handle;
    rats_tls_err_t ret = rats_tls_init(&conf, &handle);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to initialize rats tls %#x", ret);
        close(sockfd);
        return NULL;
    }
    ret = rats_tls_set_verification_callback(&handle, NULL);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to set verification callback %#x", ret);
        goto err;
    }
    ret = rats_tls_negotiate(handle, sockfd);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to negotiate %#x", ret);
        goto err;
    }
    
    // Receive the length of the upcoming session file as uint32_t in network byte order
    uint32_t session_len_net;
    size_t session_len_size = sizeof(uint32_t);
    LOG_DEBUG("Receiving session length as uint32_t: %zu bytes", session_len_size);
    ret = rats_tls_receive(handle, &session_len_net, &session_len_size);
    if (ret != RATS_TLS_ERR_NONE || session_len_size != sizeof(uint32_t)) {
        LOG_ERROR("Failed to receive session length %#x (received %zu bytes, expected %zu)", ret, session_len_size, sizeof(uint32_t));
        goto err;
    }
    uint32_t session_len = ntohl(session_len_net); // from network byte order to host byte order
    LOG_DEBUG("Received session length: %u", session_len);
    if (session_len > MAX_SESSION_SIZE) {
        LOG_ERROR("Session length exceeds maximum allowed size (%u > %u)", session_len, (uint32_t) MAX_SESSION_SIZE);
        goto err;
    }

    uint32_t buff_size = session_len + 1; // +1 for null terminator
    char* buf = malloc(buff_size);
    if (buf == NULL) {
        LOG_ERROR("Failed to allocate memory for session file");
        goto err;
    }
    
    // Receive session data in chunks
    size_t bytes_received = 0;
    while (bytes_received < session_len) {
        size_t remaining = session_len - bytes_received ;
        size_t len = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        ret = rats_tls_receive(handle, buf + bytes_received, &len);
        if (ret != RATS_TLS_ERR_NONE) {
            LOG_ERROR("Failed to receive chunk %#x", ret);
            free(buf);
            goto err;
        }
        bytes_received += len;
        LOG_DEBUG("Received chunk (%zu bytes), total received: %zu/%u", len, bytes_received, session_len);
    }
    if (bytes_received != session_len) {
        LOG_ERROR("Unexpected session size. Expected %u, got %zu", session_len, bytes_received);
        free(buf);
        goto err;
    }

    buf[bytes_received] = '\0';

    ret = rats_tls_cleanup(handle);
    if (ret != RATS_TLS_ERR_NONE)
        LOG_ERROR("Failed to cleanup %#x", ret);

    close(sockfd);
    return buf;

err:
    /* Ignore the error code of cleanup in order to return the prepositional error */
    rats_tls_cleanup(handle);
    close(sockfd);
    return NULL;
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char* secret = "";
    LOG_INFO("Try to get key from kbs");
    char* kbs_endpoint = getenv("kbsEndpoint");
    if (NULL == kbs_endpoint) {
        LOG_ERROR("Kbs mode must config kbsEndpoint");
        return -1;
    }

    LOG_DEBUG("Config of kbsEndpoint is %s", kbs_endpoint);

    char* secret_save_path = NULL;
    char* srv_ip = NULL;
    char* str_port = NULL;
    int port;

    srv_ip = strtok(kbs_endpoint, ":");
    str_port = strtok(NULL, ":");
    if (NULL == str_port) {
        LOG_ERROR("KbsEndpoint format error, eg: 127.0.0.1:5443");
        return -1;
    }
    port = atoi(str_port);
    char* const short_options = "a:v:t:c:ml:fs:h";
    struct option long_options[] = {
        {"attester", required_argument, NULL, 'a'},
        {"verifier", required_argument, NULL, 'v'},
        {"tls", required_argument, NULL, 't'},
        {"crypto", required_argument, NULL, 'c'},
        {"mutual", no_argument, NULL, 'm'},
        {"log-level", required_argument, NULL, 'l'},
        {"appId", no_argument, NULL, 'f'},
        {"savePath", required_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}};

    char* attester_type = "";
    char* verifier_type = "";
    char* tls_type = "";
    char* crypto_type = "";
    bool mutual = true;
    bool appid_flag = false;
    int opt;
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

    LOG_INFO("Selected log level %d", log_level);

    if (secret_save_path == NULL) {
        LOG_ERROR("Path to store secret locally is missing");
        return -1;
    }
    FILE* file = fopen(secret_save_path, "w");
    if (file == NULL) {
        LOG_ERROR("Failed to open the file %s", secret_save_path);
        return -1;
    }

    secret = get_secret_from_kbs_through_rats_tls(log_level, attester_type, verifier_type,
                                                  tls_type, crypto_type, mutual, srv_ip,
                                                  port, appid_flag);
    if (secret == NULL) {
        LOG_ERROR("Get secret from kbs failed");
        return -1;
    }

    LOG_INFO("Get secret successful");
    LOG_DEBUG("Secret is %s", secret);

    fputs(secret, file);
    fclose(file);
    free(secret);
    return 0;
}
