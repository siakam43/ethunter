/* et_bench fixture: fnptr-cast/example_3 */
/* fnptr: *md5params->md5_init_func, targets: my_md5_init */

#include <stdlib.h>

/* --- CURL MD5 parameters and function pointer types --- */
typedef int (*Curl_MD5_init_func)(void *);
typedef int (*Curl_MD5_update_func)(void *, const unsigned char *, unsigned int);
typedef int (*Curl_MD5_final_func)(unsigned char *, void *);

/* MD5 parameters struct — holds function pointer slots */
struct MD5_params {
    Curl_MD5_init_func    md5_init_func;
    Curl_MD5_update_func  md5_update_func;
    Curl_MD5_final_func   md5_final_func;
    size_t                context_size;
    unsigned int          result_size;
};

/* MD5 context returned by init */
struct MD5_context {
    void *md5_hashctx;
    const struct MD5_params *md5_hash;
};

/* --- Cast macro: converts a function pointer to the typedef type --- */
#define CURLX_FUNCTION_CAST(target_type, func) \
    (target_type)(void (*)(void))(func)

/* --- Target function: my_md5_init --- */
static int my_md5_init(void *ctx) {
    return 0;
}

static int my_md5_update(void *ctx, const unsigned char *data, unsigned int len) {
    (void)ctx; (void)data; (void)len;
    return 0;
}

static int my_md5_final(unsigned char *digest, void *ctx) {
    (void)digest; (void)ctx;
    return 0;
}

/* MD5 context type for sizeof */
typedef struct { int state[4]; } my_md5_ctx;

/* --- The cast assignment: targets assigned via macro cast to typedef types --- */
const struct MD5_params Curl_DIGEST_MD5 = {
    CURLX_FUNCTION_CAST(Curl_MD5_init_func, my_md5_init),
    CURLX_FUNCTION_CAST(Curl_MD5_update_func, my_md5_update),
    CURLX_FUNCTION_CAST(Curl_MD5_final_func, my_md5_final),
    sizeof(my_md5_ctx),
    16
};

/* --- Caller: Curl_MD5_init — calls through the fnptr --- */
struct MD5_context *
Curl_MD5_init(const struct MD5_params *md5params)
{
    struct MD5_context *ctxt;

    ctxt = malloc(sizeof(*ctxt));
    if (!ctxt) return NULL;

    ctxt->md5_hashctx = malloc(md5params->context_size);
    if (!ctxt->md5_hashctx) {
        free(ctxt);
        return NULL;
    }

    if ((*md5params->md5_init_func)(ctxt->md5_hashctx)) {
        free(ctxt->md5_hashctx);
        free(ctxt);
        return NULL;
    }

    return ctxt;
}

/* --- Higher-level caller: auth_decode_digest_md5_message --- */
typedef enum { CURLE_OK = 0, CURLE_OUT_OF_MEMORY } CURLcode;

CURLcode
auth_decode_digest_md5_message(const char *chlg,
                               char *nonce, int nonce_len,
                               char *realm, int realm_len,
                               char *algorithm, int algorithm_len,
                               char *qop_options, int qop_options_len)
{
    CURLcode result = CURLE_OK;
    struct MD5_context *ctxt;

    (void)chlg; (void)nonce; (void)nonce_len; (void)realm;
    (void)realm_len; (void)algorithm; (void)algorithm_len;
    (void)qop_options; (void)qop_options_len;

    ctxt = Curl_MD5_init(&Curl_DIGEST_MD5);
    if (!ctxt)
        return CURLE_OUT_OF_MEMORY;

    free(ctxt->md5_hashctx);
    free(ctxt);
    return result;
}
