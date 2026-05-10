/* ET-Bench fixture: fnptr-callback/example_10 */
/* Based on Solaris crypto ccm_mode_encrypt_contiguous_blocks pattern */
/* fnptr: encrypt_block, targets: aes_encrypt_block */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CRYPTO_SUCCESS 0
#define AES_BLOCK_LEN 16
#define CTR_MODE  0x01
#define CCM_MODE  0x04

typedef struct crypto_data {
    uint8_t *data;
    size_t offset;
    size_t length;
} crypto_data_t;

typedef struct {
    uint8_t ccm_remainder[AES_BLOCK_LEN];
    size_t ccm_remainder_len;
    uint8_t ccm_mac[AES_BLOCK_LEN];
    uint8_t ccm_tmp[AES_BLOCK_LEN];
    uint8_t ccm_cb[AES_BLOCK_LEN];
    uint8_t ccm_keysched[256];
    uint8_t *ccm_copy_to;
    uint8_t ccm_mac_buf[AES_BLOCK_LEN];
} ccm_ctx_t;

typedef void *aes_ctx_t;

static int ctr_mode_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
    return 0;
}

int ccm_mode_encrypt_contiguous_blocks(ccm_ctx_t *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
    size_t remainder = length;
    size_t consumed = 0;
    uint8_t *blockp = (uint8_t *)data;
    uint8_t *mac_buf;

    if (length + ctx->ccm_remainder_len < block_size) {
        memcpy((uint8_t *)ctx->ccm_remainder + ctx->ccm_remainder_len,
            blockp, length);
        ctx->ccm_remainder_len += length;
        ctx->ccm_copy_to = blockp;
        return CRYPTO_SUCCESS;
    }

    mac_buf = (uint8_t *)ctx->ccm_mac_buf;

    do {
        xor_block(blockp, mac_buf);
        encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);

        encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb,
            (uint8_t *)ctx->ccm_tmp);

        consumed += block_size;
        ctx->ccm_copy_to = NULL;
        blockp += block_size;
        remainder -= block_size;

    } while (remainder > 0);

    return CRYPTO_SUCCESS;
}

int aes_encrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out)
{
    int rv;
    int flags = CCM_MODE;

    if (flags & CTR_MODE) {
        rv = ctr_mode_contiguous_blocks(ctx, data, length, out,
            AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
    } else if (flags & CCM_MODE) {
        rv = ccm_mode_encrypt_contiguous_blocks((ccm_ctx_t *)ctx, data, length,
            out, AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
            aes_xor_block);
    } else {
        rv = 0;
    }
    return rv;
}

int aes_encrypt_block(const void *keysched, const uint8_t *in, uint8_t *out) {
    memcpy(out, in, AES_BLOCK_LEN);
    return 0;
}

void aes_copy_block(uint8_t *src, uint8_t *dst) {
    memcpy(dst, src, AES_BLOCK_LEN);
}

void aes_xor_block(uint8_t *a, uint8_t *b) {
    for (int i = 0; i < AES_BLOCK_LEN; i++)
        a[i] ^= b[i];
}
