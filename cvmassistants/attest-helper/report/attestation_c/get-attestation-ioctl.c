#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <unistd.h>

#include "attestation.h"

#include "openssl/sm3.h"

struct csv_attestation_user_data {
    uint8_t data[GUEST_ATTESTATION_DATA_SIZE];
    uint8_t mnonce[GUEST_ATTESTATION_NONCE_SIZE];
    hash_block_u hash;
};

static void gen_random_bytes(void* buf, uint32_t len) {
    uint32_t i;
    uint8_t* buf_byte = (uint8_t*)buf;

    for (i = 0; i < len; i++) {
        buf_byte[i] = rand() & 0xFF;
    }
}

static void csv_data_dump(const char* name, uint8_t* data, uint32_t len) {
    printf("%s:", name);
    int i;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)data[i];
        printf("%02hhx", c);
    }
    printf("\n");
}

struct csv_guest_mem {
    unsigned long va;
    int size;
};

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)

#define csv_guest_IOC_TYPE 'D'
#define GET_ATTESTATION_REPORT _IOWR(csv_guest_IOC_TYPE, 1, struct csv_guest_mem)

int get_attestation_report_use_ioctl(Csv_attestation_report* report, const char* custom_data) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!report) {
        LOG_ERROR("NULL pointer for report\n");
        return -1;
    }

    if (!custom_data) {
        LOG_ERROR("NULL pointer for custom data");
        return -1;
    }

    if (strlen(custom_data) > GUEST_ATTESTATION_DATA_SIZE) {
        LOG_ERROR("custom size is too large, limit to %d \n", GUEST_ATTESTATION_DATA_SIZE);
        return -1;
    }
    struct csv_attestation_user_data* user_data;
    int user_data_len = PAGE_SIZE;
    long ret;
    int fd = 0;
    struct csv_guest_mem mem = {0};

    /* prepare user data */
    user_data = (struct csv_attestation_user_data*)malloc(user_data_len);
    if (user_data == NULL) {
        LOG_ERROR("NULL pointer for user_data\n");
        return -1;
    }
    memset((void*)user_data, 0x0, user_data_len);

    strncpy((char*)user_data->data, custom_data, GUEST_ATTESTATION_DATA_SIZE);
    gen_random_bytes(user_data->mnonce, GUEST_ATTESTATION_NONCE_SIZE);
    // compute hash and save to the private page
    sm3((const unsigned char*)user_data,
        GUEST_ATTESTATION_DATA_SIZE + GUEST_ATTESTATION_NONCE_SIZE,
        (unsigned char*)&user_data->hash);

    fd = open("/dev/csv-guest", O_RDWR);
    if (fd < 0) {
        LOG_ERROR("open /dev/csv-guest failed\n");
        free(user_data);
        return -1;
    }
    mem.va = (uint64_t)user_data;
    LOG_DEBUG("mem.va: %lx\n", mem.va);
    mem.size = user_data_len;
    /*  get attestation report */
    ret = ioctl(fd, GET_ATTESTATION_REPORT, &mem);
    if (ret < 0) {
        LOG_ERROR("ioctl GET_ATTESTATION_REPORT fail: %ld\n", ret);
        goto error;
    }
    memcpy(report, user_data, sizeof(*report));

    LOG_DEBUG("the import info of the report is as follow: \n");
    // retrieve mnonce, PEK cert and ChipId by report->anonce
    static uint8_t g_user_data[USER_DATA_SIZE];
    int i,j;
    j = sizeof(report->user_data) / sizeof(uint32_t);
    for (i = 0; i < j; i++)
        ((uint32_t *)g_user_data)[i] = ((uint32_t *)report->user_data)[i] ^ report->anonce;
    printf("user data: %-64.64s \n", g_user_data);

    static uint8_t g_mnonce[GUEST_ATTESTATION_NONCE_SIZE];
    j = sizeof(report->mnonce) / sizeof(uint32_t);
    for (i = 0; i < j; i++)
         ((uint32_t *)g_mnonce)[i] = ((uint32_t *)report->mnonce)[i] ^ report->anonce;
    csv_data_dump("monce", g_mnonce, GUEST_ATTESTATION_NONCE_SIZE);

    static uint8_t g_measure[HASH_BLOCK_LEN];
    j = sizeof(report->measure) / sizeof(uint32_t);
    for (i = 0; i < j; i++)
        ((uint32_t *)g_measure)[i] = ((uint32_t *)report->measure.block)[i] ^ report->anonce;
    csv_data_dump("measure", g_measure, HASH_BLOCK_LEN);

    static uint8_t g_chip_id[SN_LEN];
    j = ((uint8_t *)&report->reserved2 - (uint8_t *)report->sn) / sizeof(uint32_t);
    for (i = 0; i < j; i++)
        ((uint32_t *)g_chip_id)[i] = ((uint32_t *)report->sn)[i] ^ report->anonce;
    printf("chip_id: %-64.64s \n", g_chip_id);
    LOG_INFO("get attestation report successful\n");
error:
    close(fd);
    free(user_data);
    return ret;
}
