
#include <stdio.h>
#include <inttypes.h>
#include <sgx_urts.h>
#include "Enclave/id_enclave_u.h"
#include "pce/pce_enclave_u.h"
#include <stdint.h>
#include <unistd.h>

#define ENCRYPTED_PPID_LENGTH             384

/* Crypto_suite */
#define PCE_ALG_RSA_OAEP_3072 1

/* Signature_scheme */
#define PCE_NIST_P256_ECDSA_SHA256 0

#define PPID_RSA3072_ENCRYPTED  3

#define REF_RSA_OAEP_3072_MOD_SIZE   384 //hardcode n size to be 384
#define REF_RSA_OAEP_3072_EXP_SIZE     4 //hardcode e size to be 4

#define DEBUG_ENCLAVE 1
#define RELEASE_ENCLAVE 0

void print_decrypted_ppid(unsigned char decrypted_ppid[], size_t length) {
    printf("Decrypted PPID: ");
    for (size_t i = 0; i < length; ++i) {
        printf("%02x", decrypted_ppid[i]); // Print each byte in hex
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    sgx_launch_token_t token = {0};
    int updated = 0;
    sgx_status_t sgx_status = SGX_SUCCESS;
    sgx_status_t ecall_ret = SGX_SUCCESS;
    int ppid_ret;
    uint8_t decrypted_ppid[16];
    sgx_enclave_id_t eid = 0;

    sgx_key_128bit_t platform_id = { 0 };
    uint32_t buffer_size = 0;
    uint8_t * p_temp = NULL;

    sgx_enclave_id_t pce_enclave_eid = 0;
    sgx_enclave_id_t id_enclave_eid = 0;

    sgx_report_t id_enclave_report;
    uint32_t enc_key_size = REF_RSA_OAEP_3072_MOD_SIZE + REF_RSA_OAEP_3072_EXP_SIZE;
    uint8_t enc_public_key[REF_RSA_OAEP_3072_MOD_SIZE + REF_RSA_OAEP_3072_EXP_SIZE];
    uint8_t encrypted_ppid[REF_RSA_OAEP_3072_MOD_SIZE];
    uint32_t encrypted_ppid_ret_size;
    pce_info_t pce_info;
    uint8_t signature_scheme;
    sgx_target_info_t pce_target_info;

    memset(decrypted_ppid, 0x00, 16);

    if (SGX_SUCCESS != (sgx_status = sgx_create_enclave("Enclave/id_enclave.so", DEBUG_ENCLAVE, &token, &updated, &id_enclave_eid, NULL)))
    {
        fprintf(stderr, "Failed to create ID enclave. The error code is:  0x%04x.\n", sgx_status);
        return -1;
    }

    if (SGX_SUCCESS != (sgx_status = sgx_create_enclave("pce/libsgx_pce.signed.so.1.25.100.1", RELEASE_ENCLAVE, &token, &updated, &pce_enclave_eid, NULL)))
    {
        fprintf(stderr, "Failed to create PCE enclave. The error code is:  0x%04x. \n", sgx_status);
        return -1;
    }

    if (SGX_SUCCESS != (sgx_status = sgx_get_target_info(pce_enclave_eid, &pce_target_info))) {
        fprintf(stderr, "Failed to get pce target info. The error code is:  0x%04x.\n", sgx_status);
        goto CLEANUP;
    }

    sgx_status = ide_get_pce_encrypt_key(id_enclave_eid,
                                         &ecall_ret,
                                         &pce_target_info,
                                         &id_enclave_report,
                                         PCE_ALG_RSA_OAEP_3072,
                                         PPID_RSA3072_ENCRYPTED,
                                         enc_key_size,
                                         enc_public_key);
    if (SGX_SUCCESS != sgx_status) {
        fprintf(stderr, "Failed to call into the ID_ENCLAVE: get_report_and_pce_encrypt_key. The error code is: 0x%04x.\n", sgx_status);
        sgx_status = -1;
        goto CLEANUP;
    }

    if (SGX_SUCCESS != ecall_ret) {
        fprintf(stderr, "Failed to generate PCE encryption key. The error code is: 0x%04x.\n", ecall_ret);
        sgx_status = -1;
        goto CLEANUP;
    }

    sgx_status = get_pc_info(pce_enclave_eid,
                             (uint32_t*) &ecall_ret,
                             &id_enclave_report,
                             enc_public_key,
                             enc_key_size,
                             PCE_ALG_RSA_OAEP_3072,
                             encrypted_ppid,
                             REF_RSA_OAEP_3072_MOD_SIZE,
                             &encrypted_ppid_ret_size,
                             &pce_info,
                             &signature_scheme);

    if (SGX_SUCCESS != sgx_status) {
        fprintf(stderr, "Failed to call into PCE enclave: get_pc_info. The error code is: 0x%04x.\n", sgx_status);
        sgx_status = -1;
        goto CLEANUP;
    }
    if (SGX_SUCCESS != ecall_ret) {
        fprintf(stderr, "Failed to get PCE info. The error code is: 0x%04x.\n", ecall_ret);
        sgx_status = -1;
        goto CLEANUP;
    }

    if (signature_scheme != PCE_NIST_P256_ECDSA_SHA256) {
        fprintf(stderr, "PCE returned incorrect signature scheme.\n");
        sgx_status = -1;
        goto CLEANUP;
    }

    if (encrypted_ppid_ret_size != ENCRYPTED_PPID_LENGTH) {
        fprintf(stderr, "PCE returned incorrect encrypted PPID size.\n");
        sgx_status = -1;
        goto CLEANUP;
    }

    sgx_status = ide_decrypt_ppid(id_enclave_eid, &ecall_ret, ENCRYPTED_PPID_LENGTH, encrypted_ppid, decrypted_ppid);
    if (SGX_SUCCESS != sgx_status) {
        fprintf(stderr, "Failed to call into the ID_ENCLAVE: ide_decrypt_ppid. The error code is: 0x%04x.\n", sgx_status);
        sgx_status = -1;
        goto CLEANUP;
    }

    print_decrypted_ppid(decrypted_ppid, sizeof(decrypted_ppid));

    CLEANUP:
    if(pce_enclave_eid != 0) {
        sgx_destroy_enclave(pce_enclave_eid);
    }
    if(id_enclave_eid != 0) {
        sgx_destroy_enclave(id_enclave_eid);
    }
    return sgx_status;
}
