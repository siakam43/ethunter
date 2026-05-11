/* ET-Bench fixture: fnptr-struct/example_10 */
/* Scenario: OpenSSL DSA method — bn_mod_exp through DSA_METHOD vtable.
   fnptr: dsa->meth->bn_mod_exp
   target: NULL (checked before call, no actual target)
   caller: dsa_sign_setup */

#include <stddef.h>
#include <stdlib.h>

typedef struct BIGNUM BIGNUM;
typedef struct BN_CTX BN_CTX;
typedef struct BN_MONT_CTX BN_MONT_CTX;
typedef struct DSA DSA;
typedef struct DSA_METHOD DSA_METHOD;
typedef struct ENGINE ENGINE;

struct BIGNUM {
    void *d;
};

struct BN_CTX {
    int used;
};

struct BN_MONT_CTX {
    int n0;
};

struct DSA {
    DSA_METHOD *meth;
    BIGNUM *g;
    BIGNUM *p;
    BN_MONT_CTX *method_mont_p;
};

struct DSA_METHOD {
    const char *name;
    int (*dsa_do_sign)(const unsigned char *dgst, int dlen, DSA *dsa);
    int (*dsa_sign_setup)(DSA *dsa, BN_CTX *ctx, BIGNUM **kinvp, BIGNUM **rp);
    int (*dsa_do_verify)(const unsigned char *dgst, int dgst_len,
                         DSA_SIG *sig, DSA *dsa);
    int (*dsa_mod_exp)(DSA *dsa, BIGNUM *rr, BIGNUM *a1, BIGNUM *p1,
                       BIGNUM *a2, BIGNUM *p2, BIGNUM *m, BN_CTX *ctx,
                       BN_MONT_CTX *in_mont);
    int (*bn_mod_exp)(DSA *dsa, BIGNUM *r, BIGNUM *a,
                      const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
                      BN_MONT_CTX *m_ctx);
    int (*dsa_init)(DSA *dsa);
    int (*dsa_finish)(DSA *dsa);
    int flags;
    void *app_data;
    int (*dsa_param_gen)(DSA *dsa, int prime_len, int q_len);
    int (*dsa_keygen)(DSA *dsa);
};

typedef struct DSA_SIG {
    BIGNUM *r;
    BIGNUM *s;
} DSA_SIG;

#define DSA_FLAG_FIPS_METHOD 0x01

static int BN_mod_exp_mont(BIGNUM *r, BIGNUM *a, BIGNUM *p, BIGNUM *m,
                           BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    (void)r; (void)a; (void)p; (void)m; (void)ctx; (void)m_ctx;
    return 1;
}

/* Caller: checks dsa->meth->bn_mod_exp != NULL, then calls through struct */
static int dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in,
                          BIGNUM **kinvp, BIGNUM **rp,
                          const unsigned char *dgst, int dlen)
{
    BIGNUM *r = *rp;
    BIGNUM *k = *kinvp;
    BN_CTX *ctx = ctx_in;
    (void)dgst; (void)dlen;

    if (dsa->meth->bn_mod_exp != NULL) {
        if (!dsa->meth->bn_mod_exp(dsa, r, dsa->g, k, dsa->p, ctx,
                                       dsa->method_mont_p))
            goto err;
    }
    return 1;
err:
    return 0;
}

int DSA_meth_set_bn_mod_exp(DSA_METHOD *dsam,
    int (*bn_mod_exp)(DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *,
                       const BIGNUM *, BN_CTX *, BN_MONT_CTX *))
{
    dsam->bn_mod_exp = bn_mod_exp;
    return 1;
}

int (*DSA_meth_get_bn_mod_exp(const DSA_METHOD *dsam))
    (DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *,
     BN_MONT_CTX *)
{
    return dsam->bn_mod_exp;
}

static int dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa) {
    (void)dgst; (void)dlen; (void)dsa; return 0;
}

static int dsa_sign_setup_no_digest(DSA *dsa, BN_CTX *ctx, BIGNUM **kinvp, BIGNUM **rp) {
    (void)dsa; (void)ctx; (void)kinvp; (void)rp; return 0;
}

static int dsa_do_verify(const unsigned char *dgst, int dgst_len, DSA_SIG *sig, DSA *dsa) {
    (void)dgst; (void)dgst_len; (void)sig; (void)dsa; return 0;
}

static int dsa_init(DSA *dsa) { (void)dsa; return 1; }
static int dsa_finish(DSA *dsa) { (void)dsa; return 1; }

static DSA_METHOD openssl_dsa_meth = {
    "OpenSSL DSA method",
    dsa_do_sign,
    dsa_sign_setup_no_digest,
    dsa_do_verify,
    NULL,                       /* dsa_mod_exp */
    NULL,                       /* dsa_bn_mod_exp */
    dsa_init,
    dsa_finish,
    DSA_FLAG_FIPS_METHOD,
    NULL,
    NULL,
    NULL
};

const DSA_METHOD *DSA_OpenSSL(void)
{
    return &openssl_dsa_meth;
}

int (*DSA_meth_get_mod_exp(const DSA_METHOD *dsam))
        (DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *, const BIGNUM *,
         const BIGNUM *, const BIGNUM *, BN_CTX *, BN_MONT_CTX *)
{
    return dsam->dsa_mod_exp;
}

int DSA_meth_set_mod_exp(DSA_METHOD *dsam,
    int (*mod_exp)(DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *,
                    const BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *,
                    BN_MONT_CTX *))
{
    dsam->dsa_mod_exp = mod_exp;
    return 1;
}

static DSA_METHOD *capi_dsa_method = NULL;
static int dsa_capi_idx = 0;
static const DSA_METHOD *ossl_dsa_meth;

static int capi_dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa) {
    (void)dgst; (void)dlen; (void)dsa; return 0;
}

static int capi_dsa_free(DSA *dsa) { (void)dsa; return 1; }

int DSA_get_ex_new_index(int a, void *b, void *c, void *d, int e) {
    (void)a; (void)b; (void)c; (void)d; (void)e; return 0;
}

int DSA_meth_set_sign(DSA_METHOD *d, int (*s)(const unsigned char *, int, DSA *)) {
    (void)d; (void)s; return 1;
}

int DSA_meth_set_verify(DSA_METHOD *d, int (*v)(const unsigned char *, int, DSA_SIG *, DSA *)) {
    (void)d; (void)v; return 1;
}

int DSA_meth_set_finish(DSA_METHOD *d, int (*f)(DSA *)) {
    (void)d; (void)f; return 1;
}

int DSA_meth_get_verify(const DSA_METHOD *d) {
    (void)d; return 0;
}

int DSA_meth_get_mod_exp(const DSA_METHOD *d) {
    (void)d; return 0;
}

int DSA_meth_get_bn_mod_exp(const DSA_METHOD *d) {
    (void)d; return 0;
}

int capi_init(ENGINE *e) {
    dsa_capi_idx = DSA_get_ex_new_index(0, NULL, NULL, NULL, 0);
    ossl_dsa_meth = DSA_OpenSSL();
    if (   !DSA_meth_set_sign(capi_dsa_method, capi_dsa_do_sign)
        || !DSA_meth_set_verify(capi_dsa_method,
                                (int (*)(const unsigned char *, int, DSA_SIG *, DSA *))DSA_meth_get_verify(ossl_dsa_meth))
        || !DSA_meth_set_finish(capi_dsa_method, capi_dsa_free)
        || !DSA_meth_set_mod_exp(capi_dsa_method,
                                    (int (*)(DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *,
                                              const BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *,
                                              BN_MONT_CTX *))DSA_meth_get_mod_exp(ossl_dsa_meth))
        || !DSA_meth_set_bn_mod_exp(capi_dsa_method,
                                (int (*)(DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *,
                                          const BIGNUM *, BN_CTX *, BN_MONT_CTX *))DSA_meth_get_bn_mod_exp(ossl_dsa_meth))
        || !bind_dsa_methods(capi_dsa_method)) {
        return 0;
    }
    return 1;
}

/* Binding: wire BN_mod_exp_mont as the bn_mod_exp target */
void bind_dsa_methods(DSA_METHOD *dsam)
{
    DSA_meth_set_bn_mod_exp(dsam, BN_mod_exp_mont);
}
