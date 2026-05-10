/* ET-Bench fixture: fnptr-struct/example_13 */
/* Scenario: OpenSSL GCM AES-NI — block cipher function pointer dispatch.
   fnptr: block (local variable holding ctx->block)
   target: aesni_encrypt
   caller: CRYPTO_gcm128_encrypt */

#include <stddef.h>
#include <string.h>

typedef struct AES_KEY {
    unsigned int rd_key[60];
    int rounds;
} AES_KEY;

typedef void (*block128_f)(const unsigned char *in, unsigned char *out,
                           const AES_KEY *key);

typedef void (*ctr128_f)(const unsigned char *in, unsigned char *out,
                         size_t blocks, const AES_KEY *key,
                         const unsigned char ivec[16]);

typedef union gcm128_context {
    struct {
        unsigned char c[16];
    } Yi, EKi, Xi;
    unsigned long u[2];
} gcm128_union;

typedef struct GCM128_CONTEXT {
    gcm128_union Yi, EKi, Xi;
    void *key;
    block128_f block;
    unsigned long len[2];
    unsigned char buf[16];
    int key_set;
} GCM128_CONTEXT;

typedef struct EVP_AES_GCM_CTX {
    GCM128_CONTEXT gcm;
    AES_KEY ks;
    ctr128_f ctr;
    int iv_set;
    int key_set;
    unsigned char iv[16];
    size_t ivlen;
    unsigned char tls_aad[16];
    int tls_aad_len;
} EVP_AES_GCM_CTX;

typedef struct EVP_CIPHER_CTX {
    int encrypt;
    int buf_len;
    unsigned char buf[32];
    void *cipher_data;
    size_t key_length;
} EVP_CIPHER_CTX;

#define EVP_C_DATA(t, ctx) ((t *)((ctx)->cipher_data))

/* Forward declarations */
static void aesni_set_encrypt_key(const unsigned char *userKey, int bits,
                                   AES_KEY *key);

/* Target: AES-NI encrypt function */
void aesni_encrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key)
{
    /* In real code this uses AES-NI instructions */
    (void)in; (void)out; (void)key;
}

/* Caller: invokes (*block) through a local variable extracted from ctx->block */
int CRYPTO_gcm128_encrypt(GCM128_CONTEXT *ctx,
                          const unsigned char *in, unsigned char *out,
                          size_t len)
{
    block128_f block = ctx->block;
    AES_KEY *key = (AES_KEY *)ctx->key;
    (void)in; (void)out;

    for (size_t i = 0; i < len; i += 16) {
        (*block)(ctx->Yi.c, ctx->EKi.c, key);
        ctx->Xi.u[1]++;
    }
    return 0;
}

static int aes_gcm_tls_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                              const unsigned char *in, size_t len)
{
    EVP_AES_GCM_CTX *gctx = (EVP_AES_GCM_CTX *)ctx->cipher_data;
    int rv = -1;

    if (out != in
        || len < (16 + 16))
        return -1;

    if (ctx->encrypt) {
        if (gctx->ctr) {
            size_t bulk = 0;
            if (len >= 32 && gctx->ctr != NULL) {
                if (CRYPTO_gcm128_encrypt(&gctx->gcm, NULL, NULL, 0))
                    return -1;

                bulk = len / 16 * 16;
                gctx->gcm.len[1] += bulk;
            }
        }
    }
    return rv;
}

void CRYPTO_gcm128_init(GCM128_CONTEXT *ctx, void *key, block128_f block)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->block = block;
    ctx->key = key;
}

void CRYPTO_gcm128_setiv(GCM128_CONTEXT *ctx, const unsigned char *iv,
                         size_t len) {
    (void)ctx; (void)iv; (void)len;
}

int CRYPTO_gcm128_aad(GCM128_CONTEXT *ctx, const unsigned char *aad,
                      size_t len) {
    (void)ctx; (void)aad; (void)len;
    return 0;
}

void aesni_set_encrypt_key(const unsigned char *userKey, int bits,
                           AES_KEY *key)
{
    (void)userKey; (void)bits; (void)key;
}

static int aesni_gcm_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                              const unsigned char *iv, int enc)
{
    EVP_AES_GCM_CTX *gctx = (EVP_AES_GCM_CTX *)ctx->cipher_data;
    (void)enc;

    if (!iv && !key)
        return 1;
    if (key) {
        aesni_set_encrypt_key(key, ctx->key_length * 8,
                              &gctx->ks);
        CRYPTO_gcm128_init(&gctx->gcm, &gctx->ks, (block128_f)aesni_encrypt);
        gctx->ctr = NULL;

        if (iv == NULL && gctx->iv_set)
            iv = gctx->iv;
        if (iv) {
            CRYPTO_gcm128_setiv(&gctx->gcm, iv, gctx->ivlen);
            gctx->iv_set = 1;
        }
        gctx->key_set = 1;
    }
    return 1;
}
