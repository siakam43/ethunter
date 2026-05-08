/* CG-Bench fixture: fnptr-struct/example_13 */
/* fnptr: block, targets: aesni_encrypt */

int CRYPTO_gcm128_encrypt(GCM128_CONTEXT *ctx,
                          const unsigned char *in, unsigned char *out,
                          size_t len)
{
    block128_f block = ctx->block;
    (*block) (ctx->Yi.c, ctx->EKi.c, key);
}

static int aes_gcm_tls_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                              const unsigned char *in, size_t len)
{
    EVP_AES_GCM_CTX *gctx = EVP_C_DATA(EVP_AES_GCM_CTX,ctx);
    int rv = -1;
    /* Encrypt/decrypt must be performed in place */
    if (out != in
        || len < (EVP_GCM_TLS_EXPLICIT_IV_LEN + EVP_GCM_TLS_TAG_LEN))
        return -1;
    /*
     * Set IV from start of buffer or generate IV and write to start of
     * buffer.
     */
    if (EVP_CIPHER_CTX_ctrl(ctx, ctx->encrypt ? EVP_CTRL_GCM_IV_GEN
                                              : EVP_CTRL_GCM_SET_IV_INV,
                            EVP_GCM_TLS_EXPLICIT_IV_LEN, out) <= 0)
        goto err;
    /* Use saved AAD */
    if (CRYPTO_gcm128_aad(&gctx->gcm, ctx->buf, gctx->tls_aad_len))
        goto err;
    /* Fix buffer and length to point to payload */
    in += EVP_GCM_TLS_EXPLICIT_IV_LEN;
    out += EVP_GCM_TLS_EXPLICIT_IV_LEN;
    len -= EVP_GCM_TLS_EXPLICIT_IV_LEN + EVP_GCM_TLS_TAG_LEN;
    if (ctx->encrypt) {
        /* Encrypt payload */
        if (gctx->ctr) {
            size_t bulk = 0;
#if defined(AES_GCM_ASM)
            if (len >= 32 && AES_GCM_ASM(gctx)) {
                if (CRYPTO_gcm128_encrypt(&gctx->gcm, NULL, NULL, 0))
                    return -1;

                bulk = AES_gcm_encrypt(in, out, len,
                                       gctx->gcm.key,
                                       gctx->gcm.Yi.c, gctx->gcm.Xi.u);
                gctx->gcm.len.u[1] += bulk;
            }
#endif
        }
    }
}

void CRYPTO_gcm128_init(GCM128_CONTEXT *ctx, void *key, block128_f block)
{
    const union {
        long one;
        char little;
    } is_endian = { 1 };

    memset(ctx, 0, sizeof(*ctx));
    ctx->block = block;
    ctx->key = key;
}

static int aesni_gcm_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
                              const unsigned char *iv, int enc)
{
    EVP_AES_GCM_CTX *gctx = EVP_C_DATA(EVP_AES_GCM_CTX,ctx);
    if (!iv && !key)
        return 1;
    if (key) {
        aesni_set_encrypt_key(key, EVP_CIPHER_CTX_key_length(ctx) * 8,
                              &gctx->ks.ks);
        CRYPTO_gcm128_init(&gctx->gcm, &gctx->ks, (block128_f) aesni_encrypt);
        gctx->ctr = (ctr128_f) aesni_ctr32_encrypt_blocks;
        /*
         * If we have an iv can set it directly, otherwise use saved IV.
         */
        if (iv == NULL && gctx->iv_set)
            iv = gctx->iv;
        if (iv) {
            CRYPTO_gcm128_setiv(&gctx->gcm, iv, gctx->ivlen);
            gctx->iv_set = 1;
        }
        gctx->key_set = 1;
    }
}

void aesni_encrypt(const unsigned char *in, unsigned char *out,
                   const AES_KEY *key);


/* Stub implementation for aesni_encrypt */
void aesni_encrypt(const unsigned char *in, unsigned char *out, const AES_KEY *key) {}
