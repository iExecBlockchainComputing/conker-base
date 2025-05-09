#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "attestation.h"

#include "openssl/conf.h"
#include "openssl/ec.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/sm2.h"
#include "openssl/sm3.h"

static void csv_report_dump_part(const char* name, uint8_t* section, uint32_t len) {
    printf("report.%s:\n", name);
    int i;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)section[i];
        printf("%02hhx", c);
    }
    printf("\n");
}

static void invert_endian(unsigned char* buf, int len) {
    for (int i = 0; i < len / 2; i++) {
        unsigned int tmp = buf[i];
        buf[i] = buf[len - i - 1];
        buf[len - i - 1] = tmp;
    }
}

static int gmssl_sm2_verify(struct ecc_point_q Q, unsigned char* userid, unsigned int userid_len, const unsigned char* msg, unsigned int msg_len, struct ecdsa_sign* sig_in) {
    int ret;
    EC_KEY* eckey;
    unsigned char dgst[ECC_LEN];
    long unsigned int dgstlen;

    if (!msg || !userid || !sig_in) {
        LOG_ERROR("gmssl_sm2 dsa256_verify invalid input parameter\n")
        return -1;
    }

    invert_endian(sig_in->r, ECC_LEN);
    invert_endian(sig_in->s, ECC_LEN);

    BIGNUM* bn_qx = BN_bin2bn(Q.Qx, 32, NULL);
    BIGNUM* bn_qy = BN_bin2bn(Q.Qy, 32, NULL);

    eckey = EC_KEY_new();
    EC_GROUP* group256 = EC_GROUP_new_by_curve_name(NID_sm2p256v1);
    EC_KEY_set_group(eckey, group256);
    EC_POINT* ecpt_pubkey = EC_POINT_new(group256);
    EC_POINT_set_affine_coordinates_GFp(group256, ecpt_pubkey, bn_qx, bn_qy, NULL);
    EC_KEY_set_public_key(eckey, ecpt_pubkey);

    if (eckey == NULL) {
        /* error */
        LOG_ERROR("EC_KEY_new_by_curve_name");
        EC_POINT_free(ecpt_pubkey);
        return -1;
    }

    dgstlen = sizeof(dgst);
    SM2_compute_message_digest(EVP_sm3(), EVP_sm3(), msg, msg_len, (const char*)userid,
                               userid_len, dgst, &dgstlen, eckey);

    /* verify */
    ECDSA_SIG* s = ECDSA_SIG_new();
    BIGNUM* sig_r = BN_new();
    BIGNUM* sig_s = BN_new();
    BN_bin2bn(sig_in->r, 32, sig_r);
    BN_bin2bn(sig_in->s, 32, sig_s);
    ECDSA_SIG_set0(s, sig_r, sig_s);
    LOG_DEBUG("Signature: r=%s, s=%s\n", BN_bn2hex(sig_r), BN_bn2hex(sig_s));

    ret = SM2_do_verify(dgst, dgstlen, s, eckey);

    EC_POINT_free(ecpt_pubkey);
    ECDSA_SIG_free(s);
    EC_GROUP_free(group256);
    EC_KEY_free(eckey);

    if (1 != ret) {
        LOG_ERROR("SM2_do_verify fail!, ret=%d\n", ret);
        return -1;
    } else
        LOG_INFO("SM2_do_verify success!\n");

    return 0;
}

static int csv_cert_verify(const char* data, uint32_t datalen, ecc_signature_t* signature, ecc_pubkey_t* pubkey) {
    struct ecc_point_q Q;

    Q.curve_id = pubkey->curve_id;
    memcpy(Q.Qx, pubkey->Qx, ECC_LEN);
    memcpy(Q.Qy, pubkey->Qy, ECC_LEN);
    invert_endian(Q.Qx, ECC_LEN);
    invert_endian(Q.Qy, ECC_LEN);

    struct ecdsa_sign sig_in;
    memcpy(sig_in.r, signature->sig_r, ECC_LEN);
    memcpy(sig_in.s, signature->sig_s, ECC_LEN);

    return gmssl_sm2_verify(Q, ((userid_u*)pubkey->user_id)->uid, ((userid_u*)pubkey->user_id)->len, (const unsigned char*)data, datalen, &sig_in);
}

int verify_hrk_cert_signature(CHIP_ROOT_CERT_t* hrk) {
    struct ecc_point_q Q;
    struct ecdsa_sign sig_in;

    uint32_t need_copy_len = 0;
    uint8_t hrk_userid[256] = {0};
    userid_u* sm2_userid = (userid_u*)hrk_userid;

    ecc_pubkey_t* pubkey = &hrk->ecc_pubkey;
    ecc_signature_t* signature = &hrk->ecc_sig;

    Q.curve_id = (curve_id_t)pubkey->curve_id;
    memcpy(Q.Qx, pubkey->Qx, ECC_LEN);
    memcpy(Q.Qy, pubkey->Qy, ECC_LEN);
    invert_endian(Q.Qx, ECC_LEN);
    invert_endian(Q.Qy, ECC_LEN);

    sm2_userid->len = ((userid_u*)pubkey->user_id)->len;
    need_copy_len = sm2_userid->len;
    if (sm2_userid->len > (256 - sizeof(uint16_t))) {
        need_copy_len = 256 - sizeof(uint16_t);
    }
    memcpy(sm2_userid->uid, (uint8_t*)(((userid_u*)pubkey->user_id)->uid), need_copy_len);

    memcpy(sig_in.r, signature->sig_r, ECC_LEN);
    memcpy(sig_in.s, signature->sig_s, ECC_LEN);

    return gmssl_sm2_verify(Q, sm2_userid->uid, sm2_userid->len, (const uint8_t*)hrk, 64 + 512, &sig_in);
}

static int verify_hsk_cert_signature(CHIP_ROOT_CERT_t* hrk, CHIP_ROOT_CERT_t* hsk) {
    struct ecc_point_q Q;
    struct ecdsa_sign sig_in;

    uint32_t need_copy_len = 0;
    uint8_t hrk_userid[256] = {0};
    userid_u* sm2_userid = (userid_u*)hrk_userid;

    ecc_pubkey_t* pubkey = (ecc_pubkey_t*)hrk->pubkey;
    ecc_signature_t* signature = &hsk->ecc_sig;

    Q.curve_id = (curve_id_t)pubkey->curve_id;
    memcpy(Q.Qx, pubkey->Qx, ECC_LEN);
    memcpy(Q.Qy, pubkey->Qy, ECC_LEN);
    invert_endian(Q.Qx, ECC_LEN);
    invert_endian(Q.Qy, ECC_LEN);

    sm2_userid->len = ((userid_u*)pubkey->user_id)->len;
    need_copy_len = sm2_userid->len;
    if (sm2_userid->len > (256 - sizeof(uint16_t))) {
        need_copy_len = 256 - sizeof(uint16_t);
    }
    memcpy(sm2_userid->uid, (uint8_t*)(((userid_u*)pubkey->user_id)->uid), need_copy_len);

    memcpy(sig_in.r, signature->sig_r, ECC_LEN);
    memcpy(sig_in.s, signature->sig_s, ECC_LEN);

    return gmssl_sm2_verify(Q, sm2_userid->uid, sm2_userid->len, (const uint8_t*)hsk, 64 + 512, &sig_in);
}

static int verify_cek_cert_signature(CHIP_ROOT_CERT_t* hsk, CSV_CERT_t* cek) {
    struct ecc_point_q Q;
    struct ecdsa_sign sig_in;

    uint32_t need_copy_len = 0;
    uint8_t hrk_userid[256] = {0};
    userid_u* sm2_userid = (userid_u*)hrk_userid;

    ecc_pubkey_t* pubkey = (ecc_pubkey_t*)hsk->pubkey;
    ecc_signature_t* signature;

    if (KEY_USAGE_TYPE_INVALID == cek->sig1_usage) {
        signature = &cek->ecc_sig2;
    } else {
        signature = &cek->ecc_sig1;
    }

    Q.curve_id = (curve_id_t)pubkey->curve_id;
    memcpy(Q.Qx, pubkey->Qx, ECC_LEN);
    memcpy(Q.Qy, pubkey->Qy, ECC_LEN);
    invert_endian(Q.Qx, ECC_LEN);
    invert_endian(Q.Qy, ECC_LEN);

    sm2_userid->len = ((userid_u*)pubkey->user_id)->len;
    need_copy_len = sm2_userid->len;
    if (sm2_userid->len > (256 - sizeof(uint16_t))) {
        need_copy_len = 256 - sizeof(uint16_t);
    }
    memcpy(sm2_userid->uid, (uint8_t*)(((userid_u*)pubkey->user_id)->uid), need_copy_len);

    memcpy(sig_in.r, signature->sig_r, ECC_LEN);
    memcpy(sig_in.s, signature->sig_s, ECC_LEN);

    return gmssl_sm2_verify(Q, sm2_userid->uid, sm2_userid->len, (const uint8_t*)cek, 16 + 1028, &sig_in);
}

static int verify_pek_cert_with_cek_signature(CSV_CERT_t* cek, CSV_CERT_t* pek) {
    struct ecc_point_q Q;
    struct ecdsa_sign sig_in;

    uint32_t need_copy_len = 0;
    uint8_t hrk_userid[256] = {0};
    userid_u* sm2_userid = (userid_u*)hrk_userid;

    ecc_pubkey_t* pubkey = &cek->ecc_pubkey;
    ecc_signature_t* signature = &pek->ecc_sig1;

    Q.curve_id = (curve_id_t)pubkey->curve_id;
    memcpy(Q.Qx, pubkey->Qx, ECC_LEN);
    memcpy(Q.Qy, pubkey->Qy, ECC_LEN);
    invert_endian(Q.Qx, ECC_LEN);
    invert_endian(Q.Qy, ECC_LEN);

    sm2_userid->len = ((userid_u*)pubkey->user_id)->len;
    need_copy_len = sm2_userid->len;
    if (sm2_userid->len > (256 - sizeof(uint16_t))) {
        need_copy_len = 256 - sizeof(uint16_t);
    }
    memcpy(sm2_userid->uid, (uint8_t*)(((userid_u*)pubkey->user_id)->uid), need_copy_len);

    memcpy(sig_in.r, signature->sig_r, ECC_LEN);
    memcpy(sig_in.s, signature->sig_s, ECC_LEN);

    return gmssl_sm2_verify(Q, sm2_userid->uid, sm2_userid->len, (const uint8_t*)pek, 16 + 1028, &sig_in);
}

static int load_data_from_file(const char* path, void* buff, size_t len) {
    if (!path || !*path) {
        LOG_ERROR("no file\n");
        return -ENOENT;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("open file %s fail %s\n", path, strerror(errno));
        return fd;
    }

    int rlen = 0, n;

    while (rlen < len) {
        n = read(fd, buff + rlen, len);
        if (n == -1) {
            LOG_ERROR("read file error\n");
            close(fd);
            return n;
        }
        if (!n) {
            break;
        }
        rlen += n;
    }

    close(fd);

    return 0;
}

int get_hrk_cert(char* cert_file) {
    int cmd_ret = -1;
    char command_buff[256];

    sprintf(command_buff, "curl -o %s " HRK_CERT_SITE, cert_file);
    cmd_ret = system(command_buff);

    return (int)cmd_ret;
}

static int load_hrk_file(char* filename, void* buff, size_t len) {
    int ret;
    if (access(filename, F_OK) == -1) {
        ret = get_hrk_cert(filename);
        if (ret == -1) {
            LOG_ERROR("Error:Download hrk failed\n");
            return ret;
        }
        LOG_INFO("Get hrk file from remote successful\n");
    }

    ret = load_data_from_file(filename, buff, len);
    return ret;
}

int get_hsk_cek_cert(char* cert_file, const char* chip_id) {
    int cmd_ret = -1;
    char command_buff[256];

    sprintf(command_buff, "curl -o %s " KDS_CERT_SITE "%s", cert_file, chip_id);
    cmd_ret = system(command_buff);

    return (int)cmd_ret;
}

static int load_hsk_cek_file(const char* chip_id, void* hsk, size_t hsk_len, void* cek, size_t cek_len) {
    int ret;
    struct {
        CHIP_ROOT_CERT_t hsk;
        CSV_CERT_t cek;
    } __attribute__((aligned(1))) HCK_file;

    char hsk_cek_file_name[1024];
    snprintf(hsk_cek_file_name, 1024, "%s_%s", chip_id, HSK_CEK_FILENAME);
    if (access(hsk_cek_file_name, F_OK) == -1) {
        ret = get_hsk_cek_cert(hsk_cek_file_name, chip_id);
        if (ret == -1) {
            LOG_ERROR("Error:Download hsk-cek failed\n");
            return ret;
        }
        LOG_INFO("Get hsk-cek file from remote successful\n");
    }

    ret = load_data_from_file(hsk_cek_file_name, &HCK_file, sizeof(HCK_file));
    if (ret) {
        LOG_ERROR("Error: load HSK CEK file failed\n");
        return ret;
    }

    memcpy(hsk, &HCK_file.hsk, hsk_len);
    memcpy(cek, &HCK_file.cek, cek_len);
    return 0;
}

static int validate_cert_chain(struct csv_attestation_report* report, const char* chip_id, CSV_CERT_t* pek) {
    CSV_CERT_t cek;
    CHIP_ROOT_CERT_t hsk;
    CHIP_ROOT_CERT_t hrk;
    int success = 0;
    int ret;

    do {
        ret = load_hrk_file(ARK_FILENAME, &hrk, sizeof(CHIP_ROOT_CERT_t));
        if (ret) {
            LOG_ERROR("hrk.cert doesn't exist or size isn't correct\n");
            break;
        }
        if (hrk.key_usage != KEY_USAGE_TYPE_ARK) {
            LOG_ERROR("hrk.cert key_usage field isn't correct, please use command parse_cert to check hrk.cert\n");
        }

        ret = load_hsk_cek_file(chip_id, &hsk, sizeof(CHIP_ROOT_CERT_t), &cek, sizeof(CSV_CERT_t));
        if (ret) {
            printf("Error:load hsk-cek cert failed\n");
            break;
        }
        if (hsk.key_usage != KEY_USAGE_TYPE_ASK)  // Variable size
        {
            LOG_ERROR("hsk.cert key_usage field isn't correct, please use command parse_cert to check hsk.cert\n");
            break;
        }

        if (cek.pubkey_usage != KEY_USAGE_TYPE_CEK) {
            printf("cek.cert pub_key_usage field doesn't correct, please use command parse_cert to check cek.cert\n");
            break;
        }

        if (cek.sig1_usage != KEY_USAGE_TYPE_ASK) {
            LOG_ERROR("cek.cert sig_1_usage field isn't correct, please use command parse_cert to check cek.cert\n");
            break;
        }

        if (cek.sig2_usage != KEY_USAGE_TYPE_INVALID) {
            LOG_ERROR("cek.cert sig_2_usage field isn't correct, please use command parse_cert to check cek.cert\n");
            break;
        }

        success = 1;
    } while (0);

    if (!success) {
        LOG_ERROR("Error:load error cert file\n");
        return -1;
    }

    success = 0;
    do {
        ret = verify_hrk_cert_signature(&hrk);
        if (ret) {
            LOG_ERROR("hrk pubkey verify hrk cert failed\n");
            break;
        }
        LOG_INFO("hrk pubkey verify hrk cert successful\n");

        ret = verify_hsk_cert_signature(&hrk, &hsk);
        if (ret) {
            printf("hrk pubkey verify hsk cert failed\n");
            break;
        }
        LOG_INFO("hrk pubkey verify hsk cert successful\n");

        ret = verify_cek_cert_signature(&hsk, &cek);
        if (ret) {
            LOG_ERROR("hsk pubkey verify cek cert failed\n");
            break;
        }
        LOG_INFO("hsk pubkey verify cek cert successful\n");

        ret = verify_pek_cert_with_cek_signature(&cek, pek);
        if (ret) {
            LOG_ERROR("cek pubkey and verify pek cert failed\n");
            break;
        }
        LOG_INFO("cek pubkey verify pek cert successful\n");

        success = 1;
    } while (0);

    if (success) {
        LOG_INFO("validata cert chain successful\n");
        return 0;
    }

    return -1;
}

// full verify report will verify the cert chain and report
int full_verify_report(struct csv_attestation_report* report) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (NULL == report) {
        LOG_ERROR("report is null\n");
        return -1;
    }
    int i, j = 0;
    uint8_t chip_id[SN_LEN];
    j = ((uint8_t*)&report->reserved2 - (uint8_t*)report->sn) / sizeof(uint32_t);
    for (i = 0; i < j; i++)
        ((uint32_t*)chip_id)[i] = ((uint32_t*)report->sn)[i] ^ report->anonce;

    LOG_DEBUG("chip_id is %s\n", chip_id);
    csv_report_dump_part("sn", report->sn, 64);

    int ret = 0;
    CSV_CERT_t pek_cert;
    j = ((uint8_t*)report->sn - (uint8_t*)&report->pek_cert) / sizeof(uint32_t);
    for (i = 0; i < j; i++)
        ((uint32_t*)&pek_cert)[i] = ((uint32_t*)&report->pek_cert)[i] ^ report->anonce;
    ret = validate_cert_chain(report, (char*)chip_id, &pek_cert);
    if (ret) {
        LOG_ERROR("validata cert chain failed\n");
        return -1;
    }

    ret = csv_cert_verify((const char*)report, ATTESTATION_REPORT_SIGNED_SIZE, &report->ecc_sig1, &pek_cert.ecc_pubkey);
    if (0 == ret) {
        LOG_INFO("verify report succussful\n");
    } else {
        LOG_ERROR("verify report fail\n");
    }
    return ret;
}

// int main(int argc,char *argv[])
// {
//     struct csv_attestation_report report;
//     int ret = 0;
//     int i   = 0;
//     int j   = 0;

//     int verify_chain;

//     if(argc < 2){
//         printf("Error:lack one parameter\n");
//         return -1;
//     }

//     if(!strncasecmp(argv[1],"true",4)){
//         verify_chain = 1;
//     }else if(!strncasecmp(argv[1],"false",5)){
//         verify_chain = 0;
//     }else{
//         printf("Error:Invalid parameter\n");
//         return -1;
//     }

//     printf("verify attestation report\n");

//     printf("load attestation report from %s\n", ATTESTATION_REPORT_FILE);
//     ret = load_data_from_file(ATTESTATION_REPORT_FILE,&report,sizeof(struct csv_attestation_report));
//     if (ret) {
//         printf("load report from file fail\n");
//         return ret;
//     }

//     // retrieve mnonce, PEK cert and ChipId by report->anonce
//     j = sizeof(report.user_data) / sizeof(uint32_t);
//     for (i = 0; i < j; i++)
//         ((uint32_t *)g_user_data)[i] = ((uint32_t *)report.user_data)[i] ^ report.anonce;

//     j = sizeof(report.mnonce) / sizeof(uint32_t);
//     for (i = 0; i < j; i++)
//          ((uint32_t *)g_mnonce)[i] = ((uint32_t *)report.mnonce)[i] ^ report.anonce;

//     j = sizeof(report.measure) / sizeof(uint32_t);
//     for (i = 0; i < j; i++)
//         ((uint32_t *)g_measure)[i] = ((uint32_t *)report.measure.block)[i] ^ report.anonce;

//     j = ((uint8_t *)report.sn - (uint8_t *)&report.pek_cert) / sizeof(uint32_t);
//     for (i = 0; i < j; i++)
//         ((uint32_t *)&g_pek_cert)[i] = ((uint32_t *)&report.pek_cert)[i] ^ report.anonce;

//     j = ((uint8_t *)&report.reserved2 - (uint8_t *)report.sn) / sizeof(uint32_t);
//     for (i = 0; i < j; i++)
//         ((uint32_t *)g_chip_id)[i] = ((uint32_t *)report.sn)[i] ^ report.anonce;

//     if(verify_chain){
//         printf("\nValidate cert chain:\n");
//         ret = validate_cert_chain(&report);
//         if(ret){
//             printf("validata cert chain failed\n\n");
//             return -1;
//         }
//     }

//     printf("verify report\n");
//     ret = csv_attestation_report_verify(&report);

//     return ret;
// }
