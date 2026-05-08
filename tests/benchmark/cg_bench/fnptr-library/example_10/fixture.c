/* CG-Bench fixture: fnptr-library/example_10 */
/* fnptr: ctx->lookup_crls, targets: crls_http_cb, X509_STORE_CTX_get1_crls */

static int get_crl_delta(X509_STORE_CTX *ctx,
                         X509_CRL **pcrl, X509_CRL **pdcrl, X509 *x)
{
  skcrl = ctx->lookup_crls(ctx, nm);
}

void X509_STORE_set_lookup_crls(X509_STORE *ctx,
                                X509_STORE_CTX_lookup_crls_fn lookup_crls)
{
    ctx->lookup_crls = lookup_crls;
}

void store_setup_crl_download(X509_STORE *st)
{
    X509_STORE_set_lookup_crls_cb(st, crls_http_cb);
}

#define X509_STORE_set_lookup_crls_cb(ctx, func) \
    X509_STORE_set_lookup_crls((ctx), (func))

int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509,
                        STACK_OF(X509) *chain)
{
  if (store && store->lookup_crls)
      ctx->lookup_crls = store->lookup_crls;
  else
      ctx->lookup_crls = X509_STORE_CTX_get1_crls;
  return 0;
}

STACK_OF(X509_CRL) *X509_STORE_CTX_get1_crls(X509_STORE_CTX *ctx, X509_NAME *nm)
{
    int i, idx, cnt;
    STACK_OF(X509_CRL) *sk = sk_X509_CRL_new_null();
    X509_CRL *x;
    X509_OBJECT *obj, *xobj = X509_OBJECT_new();
    X509_STORE *store = ctx->ctx;

    /* Always do lookup to possibly add new CRLs to cache */
    if (sk == NULL
            || xobj == NULL
            || store == NULL
            || !X509_STORE_CTX_get_by_subject(ctx, X509_LU_CRL, nm, xobj)) {
        X509_OBJECT_free(xobj);
        sk_X509_CRL_free(sk);
        return NULL;
    }
    X509_OBJECT_free(xobj);
    X509_STORE_lock(store);
    idx = x509_object_idx_cnt(store->objs, X509_LU_CRL, nm, &cnt);
    if (idx < 0) {
        X509_STORE_unlock(store);
        sk_X509_CRL_free(sk);
        return NULL;
    }

    for (i = 0; i < cnt; i++, idx++) {
        obj = sk_X509_OBJECT_value(store->objs, idx);
        x = obj->data.crl;
        if (!X509_CRL_up_ref(x)) {
            X509_STORE_unlock(store);
            sk_X509_CRL_pop_free(sk, X509_CRL_free);
            return NULL;
        }
        if (!sk_X509_CRL_push(sk, x)) {
            X509_STORE_unlock(store);
            X509_CRL_free(x);
            sk_X509_CRL_pop_free(sk, X509_CRL_free);
            return NULL;
        }
    }
    X509_STORE_unlock(store);
    return sk;
}

static STACK_OF(X509_CRL) *crls_http_cb(X509_STORE_CTX *ctx, X509_NAME *nm)
{
    X509 *x;
    STACK_OF(X509_CRL) *crls = NULL;
    X509_CRL *crl;
    STACK_OF(DIST_POINT) *crldp;

    crls = sk_X509_CRL_new_null();
    if (!crls)
        return NULL;
    x = X509_STORE_CTX_get_current_cert(ctx);
    crldp = X509_get_ext_d2i(x, NID_crl_distribution_points, NULL, NULL);
    crl = load_crl_crldp(crldp);
    sk_DIST_POINT_pop_free(crldp, DIST_POINT_free);
    if (!crl) {
        sk_X509_CRL_free(crls);
        return NULL;
    }
    sk_X509_CRL_push(crls, crl);
    /* Try to download delta CRL */
    crldp = X509_get_ext_d2i(x, NID_freshest_crl, NULL, NULL);
    crl = load_crl_crldp(crldp);
    sk_DIST_POINT_pop_free(crldp, DIST_POINT_free);
    if (crl)
        sk_X509_CRL_push(crls, crl);
    return crls;
}