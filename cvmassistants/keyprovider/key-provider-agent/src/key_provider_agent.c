#include <curl/curl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
// Log levels
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_NONE 4

// Key length for wrap key
#define WRAP_KEY_LENGTH 32

int app_log_level = LOG_LEVEL_INFO; // Default to INFO level

#define LOG_WITH_TIMESTAMP(fmt, level, associated_level, ...)                  \
  do {                                                                         \
    if (app_log_level <= associated_level) {                                   \
      time_t now = time(NULL);                                                 \
      const struct tm *t = gmtime(&now);                                       \
      char ts[24];                                                             \
      strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S UTC", t);                    \
      printf("%-29s [%-5s] [%s:%d] " fmt "\n", ts, level, __FILE__, __LINE__,  \
             ##__VA_ARGS__);                                                   \
    }                                                                          \
  } while (0)

#define LOG_DEBUG(fmt, ...)                                                    \
  LOG_WITH_TIMESTAMP(fmt, "DEBUG", LOG_LEVEL_DEBUG, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...)                                                     \
  LOG_WITH_TIMESTAMP(fmt, "INFO", LOG_LEVEL_INFO, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...)                                                     \
  LOG_WITH_TIMESTAMP(fmt, "WARN", LOG_LEVEL_WARN, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...)                                                    \
  LOG_WITH_TIMESTAMP(fmt, "ERROR", LOG_LEVEL_ERROR, ##__VA_ARGS__)

// -----------------------------------------------------------------------------
// Generate a random 32-byte key (alphanumeric and special characters)
// -----------------------------------------------------------------------------
char *generate_random_key(void) {
  char *key = malloc(WRAP_KEY_LENGTH + 1);

  if (!key) {
    LOG_ERROR("Memory allocation failed");
    return NULL;
  }

  const char charset[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-.~";
  size_t charset_size = sizeof(charset) - 1;

  // Seed the random number generator with current time to ensure different keys
  // on each run
  srand((unsigned int)time(NULL));

  for (size_t i = 0; i < WRAP_KEY_LENGTH; i++) {
    key[i] = charset[rand() % charset_size];
  }
  key[WRAP_KEY_LENGTH] = '\0';
  return key;
}

int push_wrapkey_to_secret_box(const char *wrapkey) {
  CURL *curl;
  CURLcode res;
  long http_code = 0;

  curl = curl_easy_init();
  if (curl) {
    char request_buffer[64];
    // get token
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:9090/secret");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "http");

    strcpy(request_buffer, "key=WRAP_KEY&value=");
    strcat(request_buffer, wrapkey);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_buffer);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      LOG_ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));
      return -1;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
      LOG_ERROR(
          "Failed to push wrap key to secret box, HTTP response code: %ld",
          http_code);
      return -1;
    }
    curl_easy_cleanup(curl);
    return 0;
  } else {
    LOG_ERROR("Init curl failed");
    return -1;
  }
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);

  // Command line options
  char *const short_options = "l:h";
  struct option long_options[] = {{"log-level", required_argument, NULL, 'l'},
                                  {"help", no_argument, NULL, 'h'},
                                  {0, 0, 0, 0}};

  int opt;
  do {
    opt = getopt_long(argc, argv, short_options, long_options, NULL);
    switch (opt) {
    case 'l':
      if (!strcasecmp(optarg, "debug"))
        app_log_level = LOG_LEVEL_DEBUG;
      else if (!strcasecmp(optarg, "info"))
        app_log_level = LOG_LEVEL_INFO;
      else if (!strcasecmp(optarg, "warn"))
        app_log_level = LOG_LEVEL_WARN;
      else if (!strcasecmp(optarg, "error"))
        app_log_level = LOG_LEVEL_ERROR;
      else if (!strcasecmp(optarg, "off"))
        app_log_level = LOG_LEVEL_NONE;
      break;
    case 'h':
      puts("    Usage:\n\n"
           "        key-provider-agent [options]\n\n"
           "    Options:\n\n"
           "        --log-level/-l value    set the log level (debug, info, "
           "warn, error, off)\n"
           "        --help/-h               show the usage\n");
      exit(0);
    case -1:
      break;
    default:
      puts("Use --help for usage information");
      exit(-1);
    }
  } while (opt != -1);

  const char *wrap_key = generate_random_key();
  if (wrap_key == NULL) {
    LOG_ERROR("Failed to generate random wrap key");
    return -1;
  }
  LOG_INFO("Successfully generated random wrap key");

  int ret = push_wrapkey_to_secret_box(wrap_key);
  if (ret != 0) {
    LOG_ERROR("Push wrapkey to secret box failed");
    return -1;
  }
  return 0;
}
