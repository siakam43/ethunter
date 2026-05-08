/* CG-Bench fixture: fnptr-struct/example_10 */
/* fnptr: dsa->meth->bn_mod_exp, targets: NULL */

static int dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in,
                          BIGNUM **kinvp, BIGNUM **rp,
                          const unsigned char *dgst, int dlen)
{
    if ((dsa)->meth->bn_mod_exp != NULL) {
            if (!dsa->meth->bn_mod_exp(dsa, r, dsa->g, k, dsa->p, ctx,
                                       dsa->method_mont_p))
                goto err;
    } else {
            if (!BN_mod_exp_mont(r, dsa->g, k, dsa->p, ctx, dsa->method_mont_p))
                goto err;
    }
}

int DSA_meth_set_bn_mod_exp(DSA_METHOD *dsam,
    int (*bn_mod_exp) (DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *,
                       const BIGNUM *, BN_CTX *, BN_MONT_CTX *))
{
    dsam->bn_mod_exp = bn_mod_exp;
    return 1;
}

static int capi_init(ENGINE *e)
{
    /* Setup DSA Method */
    dsa_capi_idx = DSA_get_ex_new_index(0, NULL, NULL, NULL, 0);
    ossl_dsa_meth = DSA_OpenSSL();
    if (   !DSA_meth_set_sign(capi_dsa_method, capi_dsa_do_sign)
        || !DSA_meth_set_verify(capi_dsa_method,
                                DSA_meth_get_verify(ossl_dsa_meth))
        || !DSA_meth_set_finish(capi_dsa_method, capi_dsa_free)
        || !DSA_meth_set_mod_exp(capi_dsa_method,
                                    DSA_meth_get_mod_exp(ossl_dsa_meth))
        || !DSA_meth_set_bn_mod_exp(capi_dsa_method,
                                DSA_meth_get_bn_mod_exp(ossl_dsa_meth))) {
        goto memerr;
    }
}

int (*DSA_meth_get_bn_mod_exp(const DSA_METHOD *dsam))
    (DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *,
     BN_MONT_CTX *)
{
    return dsam->bn_mod_exp;
}

const DSA_METHOD *DSA_OpenSSL(void)
{
    return &openssl_dsa_meth;
}

static DSA_METHOD openssl_dsa_meth = {
    "OpenSSL DSA method",
    dsa_do_sign,
    dsa_sign_setup_no_digest,
    dsa_do_verify,
    NULL,                       /* dsa_mod_exp, */
    NULL,                       /* dsa_bn_mod_exp, */
    dsa_init,
    dsa_finish,
    DSA_FLAG_FIPS_METHOD,
    NULL,
    NULL,
    NULL
};

int (*DSA_meth_get_mod_exp(const DSA_METHOD *dsam))
        (DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *, const BIGNUM *,
         const BIGNUM *, const BIGNUM *, BN_CTX *, BN_MONT_CTX *)
{
    return dsam->dsa_mod_exp;
}

int DSA_meth_set_mod_exp(DSA_METHOD *dsam,
    int (*mod_exp) (DSA *, BIGNUM *, const BIGNUM *, const BIGNUM *,
                    const BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *,
                    BN_MONT_CTX *))
{
    dsam->dsa_mod_exp = mod_exp;
    return 1;
}


/* Stub implementation for NULL */
void NULL(void) {}
