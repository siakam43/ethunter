/* ET-Bench fixture: fnptr-callback/example_11 */
/* Based on Solaris crypto ccm_encrypt_final pattern */
/* fnptr: xor_block, targets: aes_xor_block */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CRYPTO_SUCCESS 0
#define CRYPTO_NOT_SUPPORTED -1
#define AES_BLOCK_LEN 16
#define AES_CCM_MECH_INFO_TYPE 1
#define CCM_MODE 0x04
#define CTR_MODE 0x01

typedef struct crypto_mechanism {
    uint64_t cm_type;
} crypto_mechanism_t;

typedef struct crypto_key {
    void *data;
    size_t length;
} crypto_key_t;

typedef struct crypto_data {
    uint8_t *data;
    size_t offset;
    size_t length;
} crypto_data_t;

typedef void *crypto_spi_ctx_template_t;

typedef struct {
    uint8_t ccm_remainder[AES_BLOCK_LEN];
    size_t ccm_remainder_len;
    uint8_t ccm_mac[AES_BLOCK_LEN];
    uint8_t ccm_tmp[AES_BLOCK_LEN];
    uint8_t ccm_cb[AES_BLOCK_LEN];
    uint8_t ccm_keysched[256];
    size_t ccm_processed_data_len;
} ccm_ctx_t;

typedef struct {
    int ac_flags;
} aes_ctx_t;

typedef void *crypto_ctx_t;

int ccm_encrypt_final(ccm_ctx_t *ctx, crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
    int i;
    uint8_t *macp, *mac_buf, *lastp;

    if (ctx->ccm_remainder_len > 0) {

        mac_buf = (uint8_t *)ctx->ccm_mac;
        macp = (uint8_t *)ctx->ccm_remainder;

        /* calculate the CBC MAC */
        xor_block(macp, mac_buf);
        encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);

        /* calculate the counter mode */
        lastp = (uint8_t *)ctx->ccm_tmp;
        encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb, lastp);

        /* XOR with counter block */
        for (i = 0; i < (int)ctx->ccm_remainder_len; i++) {
            macp[i] ^= lastp[i];
        }
        ctx->ccm_processed_data_len += ctx->ccm_remainder_len;
    }
    return CRYPTO_SUCCESS;
}

static int
aes_encrypt_atomic(crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *plaintext, crypto_data_t *ciphertext,
    crypto_spi_ctx_template_t template)
{
    int ret = CRYPTO_SUCCESS;
    ccm_ctx_t aes_ctx = {0};

    if (ret == CRYPTO_SUCCESS) {
        if (mechanism->cm_type == AES_CCM_MECH_INFO_TYPE) {
            ret = ccm_encrypt_final((ccm_ctx_t *)&aes_ctx,
                ciphertext, AES_BLOCK_LEN, aes_encrypt_block,
                aes_xor_block);
        }
    }
    return ret;
}

static int
aes_encrypt_final(crypto_ctx_t *ctx, crypto_data_t *data)
{
    aes_ctx_t *aes_ctx;
    int ret;

    aes_ctx = (aes_ctx_t *)ctx;
    if (aes_ctx->ac_flags & CTR_MODE) {
        ret = CRYPTO_NOT_SUPPORTED;
    } else if (aes_ctx->ac_flags & CCM_MODE) {
        ret = ccm_encrypt_final((ccm_ctx_t *)aes_ctx, data,
            AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
    } else {
        ret = CRYPTO_SUCCESS;
    }
    return ret;
}

static int
aes_encrypt(crypto_ctx_t *ctx, crypto_data_t *plaintext,
    crypto_data_t *ciphertext)
{
    aes_ctx_t *aes_ctx;
    int ret = CRYPTO_SUCCESS;
    size_t saved_length = ciphertext->cd_length;

    aes_ctx = (aes_ctx_t *)ctx;
    if (aes_ctx->ac_flags & CCM_MODE) {
        ciphertext->cd_offset = ciphertext->cd_length;
        ciphertext->cd_length = saved_length - ciphertext->cd_length;
        ret = ccm_encrypt_final((ccm_ctx_t *)aes_ctx, ciphertext,
            AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
        if (ret != CRYPTO_SUCCESS) {
            return ret;
        }
    }
    return ret;
}

int aes_encrypt_block(const void *keysched, const uint8_t *in, uint8_t *out) {
    memcpy(out, in, AES_BLOCK_LEN);
    return 0;
}

void aes_xor_block(uint8_t *a, uint8_t *b) {
    for (int i = 0; i < AES_BLOCK_LEN; i++)
        a[i] ^= b[i];
}
