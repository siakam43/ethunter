/* CG-Bench fixture: fnptr-struct/example_9 */
/* fnptr: sdb->old_cb, targets: ssl_security_default_callback */

static int security_callback_debug(const SSL *s, const SSL_CTX *ctx,
                                   int op, int bits, int nid,
                                   void *other, void *ex)
{
    security_debug_ex *sdb = ex;
    int rv, show_bits = 1, cert_md = 0;
    const char *nm;
    int show_nm;
    rv = sdb->old_cb(s, ctx, op, bits, nid, other, ex);
}

void ssl_ctx_security_debug(SSL_CTX *ctx, int verbose)
{
    static security_debug_ex sdb;

    sdb.out = bio_err;
    sdb.verbose = verbose;
    sdb.old_cb = SSL_CTX_get_security_callback(ctx);
    SSL_CTX_set_security_callback(ctx, security_callback_debug);
    SSL_CTX_set0_security_ex_data(ctx, &sdb);
}

void SSL_CTX_set_security_callback(SSL_CTX *ctx,
                                   int (*cb) (const SSL *s, const SSL_CTX *ctx,
                                              int op, int bits, int nid,
                                              void *other, void *ex))
{
    ctx->cert->sec_cb = cb;
}

int (*SSL_CTX_get_security_callback(const SSL_CTX *ctx)) (const SSL *s,
                                                          const SSL_CTX *ctx,
                                                          int op, int bits,
                                                          int nid,
                                                          void *other,
                                                          void *ex) {
    return ctx->cert->sec_cb;
}

int ssl_security(const SSL *s, int op, int bits, int nid, void *other)
{
    return s->cert->sec_cb(s, NULL, op, bits, nid, other, s->cert->sec_ex);
}

int ssl_ctx_security(const SSL_CTX *ctx, int op, int bits, int nid, void *other)
{
    return ctx->cert->sec_cb(NULL, ctx, op, bits, nid, other,
                             ctx->cert->sec_ex);
}

int s_client_main(int argc, char **argv)
{
    ctx = SSL_CTX_new(meth);
    if (ctx == NULL) {
        ERR_print_errors(bio_err);
        goto end;
    }
    if (srp_arg.srplogin) {
        if (!srp_lateuser && !SSL_CTX_set_srp_username(ctx, srp_arg.srplogin)) {
            BIO_printf(bio_err, "Unable to set SRP username\n");
            goto end;
        }
        srp_arg.msg = c_msg;
        srp_arg.debug = c_debug;
        SSL_CTX_set_srp_cb_arg(ctx, &srp_arg);
        SSL_CTX_set_srp_client_pwd_callback(ctx, ssl_give_srp_client_pwd_cb);
        SSL_CTX_set_srp_strength(ctx, srp_arg.strength);
        if (c_msg || c_debug || srp_arg.amp == 0)
            SSL_CTX_set_srp_verify_param_callback(ctx,
                                                  ssl_srp_verify_param_cb);
    }
}

# define tls1_ctx_ctrl ssl3_ctx_ctrl

int SSL_CTX_set_srp_username(SSL_CTX *ctx, char *name)
{
    return tls1_ctx_ctrl(ctx, SSL_CTRL_SET_TLS_EXT_SRP_USERNAME, 0, name);
}

long ssl3_ctx_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
{
    DH *dh = (DH *)parg;
    EVP_PKEY *pkdh = NULL;
    if (dh == NULL) {
        SSLerr(SSL_F_SSL3_CTX_CTRL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    pkdh = ssl_dh_to_pkey(dh);
    if (pkdh == NULL) {
        SSLerr(SSL_F_SSL3_CTX_CTRL, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    if (!ssl_ctx_security(ctx, SSL_SECOP_TMP_DH,
                            EVP_PKEY_security_bits(pkdh), 0, pkdh)) {
        SSLerr(SSL_F_SSL3_CTX_CTRL, SSL_R_DH_KEY_TOO_SMALL);
        EVP_PKEY_free(pkdh);
        return 0;
    }
    EVP_PKEY_free(ctx->cert->dh_tmp);
    ctx->cert->dh_tmp = pkdh;
    return 1;
}

SSL_CTX *SSL_CTX_new(const SSL_METHOD *meth)
{
    if ((ret->cert = ssl_cert_new()) == NULL)
        goto err;
}

CERT *ssl_cert_new(void)
{
    CERT *ret = OPENSSL_zalloc(sizeof(*ret));

    if (ret == NULL) {
        SSLerr(SSL_F_SSL_CERT_NEW, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    ret->key = &(ret->pkeys[SSL_PKEY_RSA]);
    ret->references = 1;
    ret->sec_cb = ssl_security_default_callback;
    ret->sec_level = OPENSSL_TLS_SECURITY_LEVEL;
    ret->sec_ex = NULL;
    ret->lock = CRYPTO_THREAD_lock_new();
    if (ret->lock == NULL) {
        SSLerr(SSL_F_SSL_CERT_NEW, ERR_R_MALLOC_FAILURE);
        OPENSSL_free(ret);
        return NULL;
    }

    return ret;
}

static int ssl_security_default_callback(const SSL *s, const SSL_CTX *ctx,
                                         int op, int bits, int nid, void *other,
                                         void *ex)
{
    int level, minbits;
    static const int minbits_table[5] = { 80, 112, 128, 192, 256 };
    if (ctx)
        level = SSL_CTX_get_security_level(ctx);
    else
        level = SSL_get_security_level(s);

    if (level <= 0) {
        /*
         * No EDH keys weaker than 1024-bits even at level 0, otherwise,
         * anything goes.
         */
        if (op == SSL_SECOP_TMP_DH && bits < 80)
            return 0;
        return 1;
    }
    if (level > 5)
        level = 5;
    minbits = minbits_table[level - 1];
    switch (op) {
    case SSL_SECOP_CIPHER_SUPPORTED:
    case SSL_SECOP_CIPHER_SHARED:
    case SSL_SECOP_CIPHER_CHECK:
        {
            const SSL_CIPHER *c = other;
            /* No ciphers below security level */
            if (bits < minbits)
                return 0;
            /* No unauthenticated ciphersuites */
            if (c->algorithm_auth & SSL_aNULL)
                return 0;
            /* No MD5 mac ciphersuites */
            if (c->algorithm_mac & SSL_MD5)
                return 0;
            /* SHA1 HMAC is 160 bits of security */
            if (minbits > 160 && c->algorithm_mac & SSL_SHA1)
                return 0;
            /* Level 2: no RC4 */
            if (level >= 2 && c->algorithm_enc == SSL_RC4)
                return 0;
            /* Level 3: forward secure ciphersuites only */
            if (level >= 3 && c->min_tls != TLS1_3_VERSION &&
                               !(c->algorithm_mkey & (SSL_kEDH | SSL_kEECDH)))
                return 0;
            break;
        }
    case SSL_SECOP_VERSION:
        if (!SSL_IS_DTLS(s)) {
            /* SSLv3 not allowed at level 2 */
            if (nid <= SSL3_VERSION && level >= 2)
                return 0;
            /* TLS v1.1 and above only for level 3 */
            if (nid <= TLS1_VERSION && level >= 3)
                return 0;
            /* TLS v1.2 only for level 4 and above */
            if (nid <= TLS1_1_VERSION && level >= 4)
                return 0;
        } else {
            /* DTLS v1.2 only for level 4 and above */
            if (DTLS_VERSION_LT(nid, DTLS1_2_VERSION) && level >= 4)
                return 0;
        }
        break;

    case SSL_SECOP_COMPRESSION:
        if (level >= 2)
            return 0;
        break;
    case SSL_SECOP_TICKET:
        if (level >= 3)
            return 0;
        break;
    default:
        if (bits < minbits)
            return 0;
    }
    return 1;
}