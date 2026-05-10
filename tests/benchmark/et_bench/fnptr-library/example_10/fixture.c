/* ET-Bench fixture: fnptr-library/example_10 */
/* fnptr: ctx->lookup_crls, targets: crls_http_cb, X509_STORE_CTX_get1_crls */
/* Pattern: library CRL lookup with function pointer set during store initialization */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct X509_NAME X509_NAME;
typedef struct X509_CRL X509_CRL;
typedef struct X509 X509;
typedef struct X509_STORE X509_STORE;
typedef struct X509_STORE_CTX X509_STORE_CTX;

/* CRL lookup callback type */
typedef struct X509_CRL *(*X509_STORE_CTX_lookup_crls_fn)(X509_STORE_CTX *ctx, X509_NAME *nm);

struct X509_STORE_CTX {
    X509_STORE *ctx;
    X509_NAME *nm;
    X509_STORE_CTX_lookup_crls_fn lookup_crls;
};

struct X509_STORE {
    void *objs;
    X509_STORE_CTX_lookup_crls_fn lookup_crls;
};

void X509_STORE_set_lookup_crls(X509_STORE *store, X509_STORE_CTX_lookup_crls_fn lookup_crls)
{
    store->lookup_crls = lookup_crls;
}

#define X509_STORE_set_lookup_crls_cb(ctx, func) \
    X509_STORE_set_lookup_crls((ctx), (func))

static struct X509_CRL *crls_http_cb(X509_STORE_CTX *ctx, X509_NAME *nm)
{
    (void)ctx; (void)nm;
    /* Download CRLs from HTTP distribution points */
    return NULL;
}

void store_setup_crl_download(X509_STORE *st)
{
    X509_STORE_set_lookup_crls_cb(st, crls_http_cb);
}

int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509,
                        void *chain)
{
    (void)x509; (void)chain;
    ctx->ctx = store;
    if (store && store->lookup_crls)
        ctx->lookup_crls = store->lookup_crls;
    else
        ctx->lookup_crls = X509_STORE_CTX_get1_crls;
    return 0;
}

struct X509_CRL *X509_STORE_CTX_get1_crls(X509_STORE_CTX *ctx, X509_NAME *nm)
{
    /* Default lookup from store's cached CRL list */
    (void)ctx; (void)nm;
    return NULL;
}

static int get_crl_delta(X509_STORE_CTX *ctx, X509_CRL **pcrl, X509_CRL **pdcrl, X509 *x)
{
    (void)pcrl; (void)pdcrl; (void)x;
    struct X509_CRL **skcrl;
    skcrl = ctx->lookup_crls(ctx, ctx->nm);
    return 0;
}
