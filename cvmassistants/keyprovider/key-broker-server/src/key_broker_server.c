#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rats-tls/api.h>
#include <rats-tls/log.h>

#define DEFAULT_PORT 1234
#define DEFAULT_IP   "127.0.0.1"

const char *command_get_key = "getKey";

char *wrap_key = "00112233445566778899aabbccddeeff";
char *white_measure = "";

void hexdump_mem(const void* data, size_t size) {
    uint8_t* ptr = (uint8_t*)data;
    for (size_t i = 0; i < size; i++)
        printf("%02x", ptr[i]);
    printf("\n");
}

const char *format_hex_buffer (char *buffer, uint maxSize, uint8_t *data, size_t size) {
	if (size * 2 >= maxSize)
		return "DEADBEEF";

	for (size_t i=0; i < size; i++){
	    sprintf(&buffer[i*2], "%02x", data[i]);
	}
	buffer[size*2+1] = '\0';
	return buffer;
}

int call_back(void *args) {
    rtls_evidence_t *ev = (rtls_evidence_t *)args;
	printf("verify_callback called, claims %p, claims_size %zu, args %p\n", ev->custom_claims,
	       ev->custom_claims_length, args);
	for (size_t i = 0; i < ev->custom_claims_length; ++i) {
		printf("custom_claims[%zu] -> name: '%s' value_size: %zu value: '%.*s'\n", i,
		       ev->custom_claims[i].name, ev->custom_claims[i].value_size,
		       (int)ev->custom_claims[i].value_size, ev->custom_claims[i].value);
	}

	const int hex_buffer_size = 1024*1;
    char hex_buffer[hex_buffer_size];
	printf("csv_vm_measure is %s\n", format_hex_buffer(hex_buffer,hex_buffer_size,ev->csv.measure,ev->csv.measure_sz));
	printf("csv_vm_id is %s\n", ev->csv.vm_id);
	printf("csv_policy is %s\n", ev->csv.policy);
	printf("csv_vm_version is %s\n", ev->csv.vm_version);
    int white_measure_sz = strlen(white_measure);
    if (white_measure_sz == 0) {
            RTLS_ERR("white measure unset\n");
    		return 0 ;
    }
	if (!strncmp(hex_buffer, white_measure, white_measure_sz)) {
		// match the measure
        RTLS_INFO("csv_vm_measure match the white_list\n");
		return -1 ;
	} else {
		//unmach
		RTLS_ERR("unmatch csv_vm_measure white_list\n");
		return 0;
	}
}

int rats_tls_server_startup(rats_tls_log_level_t log_level, char *attester_type,
			    char *verifier_type, char *tls_type, char *crypto_type, bool mutual,
			    bool debug_enclave, char *ip, int port, const char *white_measure)
{
	rats_tls_conf_t conf;

	memset(&conf, 0, sizeof(conf));
	conf.log_level = log_level;
	strcpy(conf.attester_type, attester_type);
	strcpy(conf.verifier_type, verifier_type);
	strcpy(conf.tls_type, tls_type);
	strcpy(conf.crypto_type, crypto_type);
	conf.cert_algo = RATS_TLS_CERT_ALGO_DEFAULT;
	conf.flags |= RATS_TLS_CONF_FLAGS_SERVER;
	if (mutual)
		conf.flags |= RATS_TLS_CONF_FLAGS_MUTUAL;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		RTLS_ERR("Failed to call socket()");
		return -1;
	}

	int reuse = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, sizeof(int)) < 0) {
		RTLS_ERR("Failed to call setsockopt()");
		return -1;
	}

	struct sockaddr_in s_addr;
	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = inet_addr(ip);
	s_addr.sin_port = htons(port);

	/* Bind the server socket */
	if (bind(sockfd, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1) {
		RTLS_ERR("Failed to call bind()");
		return -1;
	}

	/* Listen for a new connection, allow 5 pending connections */
	if (listen(sockfd, 5) == -1) {
		RTLS_ERR("Failed to call listen()");
		return -1;
	}

	rats_tls_handle handle;
	rats_tls_err_t ret = rats_tls_init(&conf, &handle);
	if (ret != RATS_TLS_ERR_NONE) {
		RTLS_ERR("Failed to initialize rats tls %#x\n", ret);
		return -1;
	}

    rats_tls_callback_t user_callback = call_back;
	ret = rats_tls_set_verification_callback(&handle, user_callback);
	if (ret != RATS_TLS_ERR_NONE) {
		RTLS_ERR("Failed to set verification callback %#x\n", ret);
		return -1;
	}
    RTLS_INFO("Waiting for a connection ...\n");
	while (1) {
		/* Accept client connections */
		struct sockaddr_in c_addr;
		socklen_t size = sizeof(c_addr);

		int connd = accept(sockfd, (struct sockaddr *)&c_addr, &size);
		if (connd < 0) {
			RTLS_ERR("Failed to call accept()");
			goto over;
		}

		ret = rats_tls_negotiate(handle, connd);
		if (ret != RATS_TLS_ERR_NONE) {
			RTLS_ERR("Failed to negotiate %#x\n", ret);
			goto over;
		}

		RTLS_DEBUG("Client connected successfully\n");

		char buf[256];
		size_t len = sizeof(buf);
		ret = rats_tls_receive(handle, buf, &len);
		if (ret != RATS_TLS_ERR_NONE) {
			RTLS_ERR("Failed to receive %#x\n", ret);
			goto over;
		}

		if (len >= sizeof(buf))
			len = sizeof(buf) - 1;
		buf[len] = '\0';

		RTLS_INFO("Client: %s\n", buf);

        if (strcmp(buf, command_get_key)) {
			RTLS_ERR("unknow command");
			goto over;
		}

		/* Reply back to the client */
		len = strlen(wrap_key);
		ret = rats_tls_transmit(handle, wrap_key, &len);
		if (ret != RATS_TLS_ERR_NONE) {
			RTLS_ERR("Failed to transmit %#x\n", ret);
			goto over;
		}
over:
		close(connd);
	}
	return 0;
}

int main(int argc, char **argv)
{
    printf("    - Welcome to RATS-TLS sample server program\n");

	char *const short_options = "a:v:t:c:ml:i:p:Dhw:k:";
	// clang-format off
        struct option long_options[] = {
                { "attester", required_argument, NULL, 'a' },
                { "verifier", required_argument, NULL, 'v' },
                { "tls", required_argument, NULL, 't' },
                { "crypto", required_argument, NULL, 'c' },
                { "mutual", no_argument, NULL, 'm' },
                { "log-level", required_argument, NULL, 'l' },
                { "ip", required_argument, NULL, 'i' },
                { "port", required_argument, NULL, 'p' },
                { "debug-enclave", no_argument, NULL, 'D' },
				{ "white-measure", required_argument,NULL, 'w'},
				{ "wrap-key", no_argument, NULL, 'k' },
                { "help", no_argument, NULL, 'h' },
                { 0, 0, 0, 0 }
        };
	// clang-format on

	char *attester_type = "";
	char *verifier_type = "";
	char *tls_type = "";
	char *crypto_type = "";
	bool mutual = false;
	rats_tls_log_level_t log_level = RATS_TLS_LOG_LEVEL_INFO;
	char *ip = DEFAULT_IP;
	int port = DEFAULT_PORT;
	bool debug_enclave = false;
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
		case 'm':
			mutual = true;
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
		case 'i':
			ip = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'D':
			debug_enclave = true;
			break;
		case 'w':
		    white_measure = optarg;
			break;
		case 'k':
		    wrap_key = optarg;
			break;
		case -1:
			break;
		case 'h':
			puts("    Usage:\n\n"
			     "        rats-tls-server <options> [arguments]\n\n"
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
			     "        --help/-h             show the usage\n"
				 "        --white-measure/-w    set the white measure hash hex\n");
			exit(1);
			/* Avoid compiling warning */
			break;
		default:
			exit(1);
		}
	} while (opt != -1);

	global_log_level = log_level;

	return rats_tls_server_startup(log_level, attester_type, verifier_type, tls_type,
				       crypto_type, mutual, debug_enclave, ip, port, white_measure);
}
