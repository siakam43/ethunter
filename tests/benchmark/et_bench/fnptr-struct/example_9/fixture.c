/* ET-Bench fixture: fnptr-struct/example_9 */
/* Scenario: OpenSSL SSL security debug callback chain.
   fnptr: sdb->old_cb
   target: ssl_security_default_callback
   caller: security_callback_debug */

#include <stddef.h>
#include <stdio.h>

typedef struct SSL SSL;
typedef struct SSL_CTX SSL_CTX;
typedef struct CERT CERT;

struct CERT {
    int (*sec_cb)(const SSL *s, const SSL_CTX *ctx, int op, int bits, int nid, void *other, void *ex);
    void *sec_ex;
    void *dh_tmp;
    void *key;
    int references;
    int sec_level;
    void *lock;
};

typedef struct security_debug_ex {
    int (*old_cb)(const SSL *s, const SSL_CTX *ctx, int op, int bits, int nid, void *other, void *ex);
    void *out;
    int verbose;
} security_debug_ex;

struct SSL_CTX {
    CERT *cert;
    int session_cache_mode;
};

struct SSL {
    CERT *cert;
};

#define OPENSSL_TLS_SECURITY_LEVEL 1
#define BIO_printf(bio, fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define BIO_write(bio, buf, len) fwrite(buf, 1, len, stderr)

static FILE *bio_err = stderr;
static FILE *bio_s_out = stdout;

static int (*SSL_CTX_get_security_callback(const SSL_CTX *ctx))(const SSL *, const SSL_CTX *, int, int, int, void *, void *)
{
    if (ctx == NULL || ctx->cert == NULL)
        return NULL;
    return ctx->cert->sec_cb;
}

static void SSL_CTX_set_security_callback(SSL_CTX *ctx,
                                   int (*cb)(const SSL *s, const SSL_CTX *ctx,
                                             int op, int bits, int nid,
                                             void *other, void *ex))
{
    ctx->cert->sec_cb = cb;
}

static void SSL_CTX_set0_security_ex_data(SSL_CTX *ctx, void *ex) {
    (void)ctx; (void)ex;
}

/* Target: default security callback */
static int ssl_security_default_callback(const SSL *s, const SSL_CTX *ctx,
                                         int op, int bits, int nid, void *other,
                                         void *ex)
{
    int level;
    (void)bits; (void)nid; (void)other; (void)ex;

    if (ctx)
        level = 1;
    else
        level = 0;

    if (level <= 0) {
        if (op == 1 && bits < 80)
            return 0;
        return 1;
    }
    if (level > 5)
        level = 5;

    switch (op) {
    default:
        return 0;
    }
}

/* Caller: invokes sdb->old_cb through the struct */
static int security_callback_debug(const SSL *s, const SSL_CTX *ctx,
                                   int op, int bits, int nid,
                                   void *other, void *ex)
{
    security_debug_ex *sdb = ex;
    int rv;
    rv = sdb->old_cb(s, ctx, op, bits, nid, other, ex);
    if (sdb->verbose) {
        BIO_printf(bio_err, "security callback: op=%d bits=%d rv=%d\n", op, bits, rv);
    }
    return rv;
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

CERT *ssl_cert_new(void)
{
    CERT *ret = calloc(1, sizeof(*ret));
    if (ret == NULL)
        return NULL;
    ret->key = NULL;
    ret->references = 1;
    ret->sec_cb = ssl_security_default_callback;
    ret->sec_level = OPENSSL_TLS_SECURITY_LEVEL;
    ret->sec_ex = NULL;
    ret->lock = NULL;
    return ret;
}
