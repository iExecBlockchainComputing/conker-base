#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "attestation.h"

#include "openssl/sm3.h"

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGEMAP_LEN 8

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
    printf("%s:\n", name);
    int i;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)data[i];
        printf("%02hhx", c);
    }
    printf("\n");
}

static uint64_t va_to_pa(uint64_t va) {
    FILE* pagemap;
    uint64_t offset, pfn;

    pagemap = fopen("/proc/self/pagemap", "rb");
    if (!pagemap) {
        LOG_ERROR("open pagemap fail\n");
        return 0;
    }

    offset = va / PAGE_SIZE * PAGEMAP_LEN;
    if (fseek(pagemap, offset, SEEK_SET) != 0) {
        LOG_ERROR("seek pagemap fail\n");
        fclose(pagemap);
        return 0;
    }

    if (fread(&pfn, 1, PAGEMAP_LEN - 1, pagemap) != PAGEMAP_LEN - 1) {
        LOG_ERROR("read pagemap fail\n");
        fclose(pagemap);
        return 0;
    }

    pfn &= 0x7FFFFFFFFFFFFF;

    return pfn << PAGE_SHIFT;
}

static long hypercall(unsigned int nr, unsigned long p1, unsigned int len) {
    long ret = 0;

    asm volatile("vmmcall"
                 : "=a"(ret)
                 : "a"(nr), "b"(p1), "c"(len)
                 : "memory");
    return ret;
}

int compute_session_mac_and_verify(struct csv_attestation_report* report) {
    hash_block_u hmac = {0};

    int i, j = 0;
    uint8_t mnonce[GUEST_ATTESTATION_NONCE_SIZE];
    j = sizeof(report->mnonce) / sizeof(uint32_t);
    for (i = 0; i < j; i++)
        ((uint32_t*)mnonce)[i] = ((uint32_t*)report->mnonce)[i] ^ report->anonce;


    sm3_hmac((const unsigned char*)(&report->pek_cert),
             sizeof(report->pek_cert) + SN_LEN + sizeof(report->reserved2),
             mnonce, GUEST_ATTESTATION_NONCE_SIZE, (unsigned char*)(hmac.block));

    if (memcmp(hmac.block, report->mac.block, sizeof(report->mac.block)) == 0) {
        LOG_INFO("mac verify success\n");
        return 0;
    } else {
        LOG_ERROR("mac verify failed\n");
        return -1;
    }
}

// kvm_hc_vm_attestationhoskerne，>5.10100，5.112
int get_attestation_report_use_vmmcall(Csv_attestation_report* report, const char* custom_data, unsigned int kvm_hc_vm_attestation) {
    setvbuf(stdout, NULL, _IONBF, 0);
    struct csv_attestation_user_data* user_data;
    uint64_t user_data_pa;
    long ret;

    if (!report) {
        LOG_ERROR("NULL pointer for report\n");
        return -1;
    }

    if (custom_data == NULL) {
        LOG_ERROR("NULL pointer for user_data\n");
        return -1;
    }

    /* prepare user data */
    user_data = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (user_data == MAP_FAILED) {
        LOG_ERROR("mmap failed\n");
        return -1;
    }
    LOG_DEBUG("mmap %p\n", user_data);
    strncpy((char*)user_data->data, custom_data, GUEST_ATTESTATION_DATA_SIZE);
    gen_random_bytes(user_data->mnonce, GUEST_ATTESTATION_NONCE_SIZE);

    // compute hash and save to the private page
    sm3((const unsigned char*)user_data,
        GUEST_ATTESTATION_DATA_SIZE + GUEST_ATTESTATION_NONCE_SIZE,
        (unsigned char*)&user_data->hash);

    /* call host to get attestation report */
    user_data_pa = va_to_pa((uint64_t)user_data);
    LOG_DEBUG("user_data_pa: %lx\n", user_data_pa);
    LOG_DEBUG("kvm_hc_vm_attestatio: %d\n", kvm_hc_vm_attestation);

    ret = hypercall(kvm_hc_vm_attestation, user_data_pa, PAGE_SIZE);
    if (ret) {
        LOG_ERROR("hypercall fail: %ld\n", ret);
        munmap(user_data, PAGE_SIZE);
        return -1;
    }
    memcpy(report, user_data, sizeof(*report));

    LOG_DEBUG("the import info of the report is as follow: \n");
    // retrieve mnonce, PEK cert and ChipId by report->anonce
    static uint8_t g_user_data[USER_DATA_SIZE] = {0};
    int i,j;
    j = sizeof(report->user_data) / sizeof(uint32_t);
    for (i = 0; i < j; i++)
        ((uint32_t *)g_user_data)[i] = ((uint32_t *)report->user_data)[i] ^ report->anonce;
    printf("data_string: %-64.64s \n", g_user_data);

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

    ret = compute_session_mac_and_verify(report);
    if (ret) {
        LOG_ERROR("PEK cert and ChipId have been tampered with\n");
        return ret;
    } else {
        LOG_INFO("check PEK cert and ChipId successfully\n");
    }

//    memset(report->reserved2, 0, sizeof(report->reserved2));
    munmap(user_data, PAGE_SIZE);
    return 0;
}

int save_report_to_file(struct csv_attestation_report* report, const char* path) {
    if (!report) {
        LOG_ERROR("no report\n");
        return -1;
    }
    if (!path || !*path) {
        LOG_ERROR("no file\n");
        return -1;
    }

    int fd = open(path, O_CREAT | O_WRONLY);
    if (fd < 0) {
        LOG_ERROR("open file %s fail %d\n", path, fd);
        return fd;
    }

    int len = 0, n;

    while (len < sizeof(*report)) {
        n = write(fd, report + len, sizeof(*report));
        if (n == -1) {
            LOG_ERROR("write file error\n");
            close(fd);
            return n;
        }
        len += n;
    }

    close(fd);
    LOG_INFO("save report to %s successful\n", path);
    return 0;
}
