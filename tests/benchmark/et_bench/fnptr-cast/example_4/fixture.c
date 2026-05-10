/* et_bench fixture: fnptr-cast/example_4 */
/* fnptr: *context->md5_hash->md5_update_func, targets: my_md5_update */

#include <stdlib.h>
#include <string.h>

/* --- CURL MD5 types --- */
typedef int (*Curl_MD5_init_func)(void *);
typedef int (*Curl_MD5_update_func)(void *, const unsigned char *, unsigned int);
typedef int (*Curl_MD5_final_func)(unsigned char *, void *);

struct MD5_params {
    Curl_MD5_init_func    md5_init_func;
    Curl_MD5_update_func  md5_update_func;
    Curl_MD5_final_func   md5_final_func;
    size_t                context_size;
    unsigned int          result_size;
};

struct MD5_context {
    void *md5_hashctx;
    const struct MD5_params *md5_hash;
};

#define CURLX_FUNCTION_CAST(target_type, func) \
    (target_type)(void (*)(void))(func)

/* --- Target function: my_md5_update --- */
static int my_md5_update(void *ctx, const unsigned char *data, unsigned int len) {
    (void)ctx; (void)data; (void)len;
    return 0;
}

static int my_md5_init(void *ctx) {
    (void)ctx;
    return 0;
}

static int my_md5_final(unsigned char *digest, void *ctx) {
    (void)digest; (void)ctx;
    return 0;
}

typedef struct { int state[4]; } my_md5_ctx;

/* Cast assignment: targets assigned via macro cast to typedef types */
const struct MD5_params Curl_DIGEST_MD5 = {
    CURLX_FUNCTION_CAST(Curl_MD5_init_func, my_md5_init),
    CURLX_FUNCTION_CAST(Curl_MD5_update_func, my_md5_update),
    CURLX_FUNCTION_CAST(Curl_MD5_final_func, my_md5_final),
    sizeof(my_md5_ctx),
    16
};

/* --- Init: creates context and stores md5_params reference --- */
struct MD5_context *
Curl_MD5_init(const struct MD5_params *md5params)
{
    struct MD5_context *ctxt;

    ctxt = malloc(sizeof(*ctxt));
    if (!ctxt) return NULL;

    ctxt->md5_hashctx = malloc(md5params->context_size);
    ctxt->md5_hash = md5params;
    return ctxt;
}

/* --- Caller: Curl_MD5_update — calls through context->md5_hash->md5_update_func --- */
typedef enum { CURLE_OK = 0, CURLE_OUT_OF_MEMORY } CURLcode;

CURLcode
Curl_MD5_update(struct MD5_context *context,
                const unsigned char *data,
                unsigned int len)
{
    (*context->md5_hash->md5_update_func)(context->md5_hashctx, data, len);
    return CURLE_OK;
}

/* --- Higher-level caller using both init and update --- */
typedef struct {
    const char *apoptimestamp;
} pop3_conn;

CURLcode
pop3_perform_apop(void *data, pop3_conn *pop3c)
{
    CURLcode result = CURLE_OK;
    struct MD5_context *ctxt;

    (void)data;

    ctxt = Curl_MD5_init(&Curl_DIGEST_MD5);
    if (!ctxt)
        return CURLE_OUT_OF_MEMORY;

    Curl_MD5_update(ctxt, (const unsigned char *)pop3c->apoptimestamp,
                    (unsigned int)strlen(pop3c->apoptimestamp));

    free(ctxt->md5_hashctx);
    free(ctxt);
    return result;
}
