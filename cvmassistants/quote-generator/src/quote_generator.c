#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <rats-tls/api.h>

#include <rats-tls/core.h>
#include <rats-tls/tdx-ecdsa.h>

// We need to access internal structures, so we'll include the internal headers
//#include "../include/core.h"
//#include "../include/tdx-ecdsa.h"

#define TDX_NUM_RTMRS 4
#define SHA384_HASH_SIZE 48
#define REPORT_DATA_SIZE 64

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

#define LOG_INFO(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "INFO", RATS_TLS_LOG_LEVEL_INFO, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "WARN", RATS_TLS_LOG_LEVEL_WARN, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    LOG_WITH_TIMESTAMP(fmt, "ERROR", RATS_TLS_LOG_LEVEL_ERROR, ##__VA_ARGS__)

// Configuration variables
rats_tls_log_level_t log_level = RATS_TLS_LOG_LEVEL_ERROR;

// Function to extract and display RTMR measurements from TDX quote
void display_rtmr_from_quote(attestation_evidence_t *evidence) {
    if (!evidence || strcmp(evidence->type, "tdx_ecdsa") != 0) {
        LOG_ERROR("Invalid evidence type or NULL evidence");
        return;
    }
    
    // Display the entire quote in hex first
    LOG_INFO("TDX Quote (hex):");
    printf("Quote length: %u bytes\n", evidence->tdx.quote_len);
    printf("Quote data: ");
    
    // Display the quote in hex format (2 hex digits per byte)
    for (uint32_t i = 0; i < evidence->tdx.quote_len; i++) {
        printf("%02x", evidence->tdx.quote[i]);
    }
    printf("\n\n");
    
    // Cast the quote to TDX quote structure for structured access
    tdx_quote_t *quote = (tdx_quote_t *)evidence->tdx.quote;
    if (!quote) {
        LOG_ERROR("NULL quote in evidence");
        return;
    }
    
    // Display RTMR measurements
    LOG_INFO("RTMR Measurements:");
    for (int i = 0; i < TDX_NUM_RTMRS; i++) {
        printf("  RTMR[%d]: ", i);
        for (int j = 0; j < SHA384_HASH_SIZE; j++) {
            printf("%02x", quote->report_body.rtmr[i][j]);
        }
        printf("\n");
    }
}

// Function to directly collect evidence using TDX attester with nullverifier
int collect_tdx_evidence_directly(void) {
    rats_tls_conf_t conf;
    memset(&conf, 0, sizeof(conf));
    
    // Configure for TDX attester with nullverifier (no verification required)
    const char* attester_type = "tdx_ecdsa";
    const char* verifier_type = "nullverifier";  // Use nullverifier to bypass verification
    const char* tls_type = "openssl";
    const char* crypto_type = "openssl";
    
    conf.log_level = log_level;
    conf.api_version = RATS_TLS_API_VERSION_DEFAULT;
    
    // Safe string copying with bounds checking
    strncpy(conf.attester_type, attester_type, ENCLAVE_ATTESTER_TYPE_NAME_SIZE - 1);
    conf.attester_type[ENCLAVE_ATTESTER_TYPE_NAME_SIZE - 1] = '\0';
    strncpy(conf.verifier_type, verifier_type, ENCLAVE_VERIFIER_TYPE_NAME_SIZE - 1);
    conf.verifier_type[ENCLAVE_VERIFIER_TYPE_NAME_SIZE - 1] = '\0';
    strncpy(conf.tls_type, tls_type, TLS_TYPE_NAME_SIZE - 1);
    conf.tls_type[TLS_TYPE_NAME_SIZE - 1] = '\0';
    strncpy(conf.crypto_type, crypto_type, CRYPTO_TYPE_NAME_SIZE - 1);
    conf.crypto_type[CRYPTO_TYPE_NAME_SIZE - 1] = '\0';
    
    conf.cert_algo = RATS_TLS_CERT_ALGO_DEFAULT;
    
    // Set the MUTUAL flag to trigger certificate generation (and evidence collection)
    conf.flags |= RATS_TLS_CONF_FLAGS_MUTUAL;
    
    // Initialize rats-tls - this will trigger evidence collection
    rats_tls_handle handle;
    rats_tls_err_t ret = rats_tls_init(&conf, &handle);
    if (ret != RATS_TLS_ERR_NONE) {
        LOG_ERROR("Failed to initialize rats tls %#x", ret);
        return -1;
    }
    
    // Cast handle to internal context to access the evidence
    rtls_core_context_t *ctx = (rtls_core_context_t *)handle;
    if (!ctx || !ctx->attester) {
        LOG_ERROR("Failed to access attester context");
        rats_tls_cleanup(handle);
        return -1;
    }
    
    // The evidence was collected during init, but we need to collect it again
    // to access it directly. Let's call collect_evidence directly.
    
    // Prepare custom user data for evidence collection (will go into TDX REPORTDATA)
    uint8_t hash[REPORT_DATA_SIZE] = "Hello World!";
    //memcpy(hash, custom_data, sizeof(custom_data));

    // Prepare evidence structure
    attestation_evidence_t evidence;
    memset(&evidence, 0, sizeof(evidence));
    
    // Call attester's collect_evidence function directly
    enclave_attester_err_t attester_ret = ctx->attester->opts->collect_evidence(
        ctx->attester, &evidence, ctx->config.cert_algo, hash, sizeof(hash));
    
    if (attester_ret != ENCLAVE_ATTESTER_ERR_NONE) {
        LOG_ERROR("Failed to collect evidence %#x", attester_ret);
        rats_tls_cleanup(handle);
        return -1;
    }
    
    LOG_INFO("Successfully collected TDX evidence");
    
    // Extract and display RTMR measurements
    display_rtmr_from_quote(&evidence);
    
    // Clean up
    rats_tls_cleanup(handle);
    
    return 0;
}

int main(int argc, char** argv) {
    LOG_INFO("Starting direct TDX evidence collection with nullverifier...");
    
    int result = collect_tdx_evidence_directly();
    
    if (result == 0) {
        LOG_INFO("Successfully collected RTMR measurements");
    } else {
        LOG_ERROR("Failed to collect RTMR measurements");
    }
    
    return result;
}
