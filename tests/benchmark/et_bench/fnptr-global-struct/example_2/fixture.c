/* et_bench fixture: fnptr-global-struct/example_2 */
/* fnptr: Curl_ssl->sha256sum, targets: ossl_sha256sum */

#include <stdlib.h>
#include <stddef.h>

#define CURL_SHA256_DIGEST_LENGTH 32

typedef int CURLcode;
#define CURLE_OUT_OF_MEMORY 1

struct Curl_easy {
    int flags;
};

typedef struct Curl_ssl Curl_ssl_t;
struct Curl_ssl {
    int backend_id;
    const char *name;
    int (*get_internals)(void *);
    void (*close_one)(void *);
    void (*close_all)(void *);
    void (*session_free)(void *);
    int (*set_engine)(void *, const char *);
    int (*set_engine_default)(void *);
    void *(*engines_list)(void *);
    int (*false_start)(void);
    CURLcode (*sha256sum)(const unsigned char *, size_t,
                          unsigned char *, size_t);
};

static int ossl_get_internals(void *ctx) { return 0; }
static void ossl_close(void *ctx) {}
static void ossl_close_all(void *ctx) {}
static void ossl_session_free(void *ptr) {}
static int ossl_set_engine(void *data, const char *engine) { return 0; }
static int ossl_set_engine_default(void *data) { return 0; }
static void *ossl_engines_list(void *data) { return NULL; }
static int Curl_none_false_start(void) { return 0; }

CURLcode
ossl_sha256sum(const unsigned char *input, size_t inputlen,
               unsigned char *sha256sum, size_t sha256sumlen)
{
    for (size_t i = 0; i < inputlen && i < sha256sumlen; i++)
        sha256sum[i] = input[i];
    return 0;
}

const Curl_ssl_t Curl_ssl_openssl = {
    .backend_id = 1,
    .name = "openssl",
    .get_internals = ossl_get_internals,
    .close_one = ossl_close,
    .close_all = ossl_close_all,
    .session_free = ossl_session_free,
    .set_engine = ossl_set_engine,
    .set_engine_default = ossl_set_engine_default,
    .engines_list = ossl_engines_list,
    .false_start = Curl_none_false_start,
    .sha256sum = ossl_sha256sum,
};

const Curl_ssl_t *Curl_ssl = &Curl_ssl_openssl;

CURLcode
Curl_pin_peer_pubkey(struct Curl_easy *data,
                     const char *pinnedpubkey,
                     const unsigned char *pubkey, size_t pubkeylen)
{
    unsigned char *sha256sumdigest;
    CURLcode rc;

    sha256sumdigest = (unsigned char *)malloc(CURL_SHA256_DIGEST_LENGTH);
    if (!sha256sumdigest)
        return CURLE_OUT_OF_MEMORY;

    rc = Curl_ssl->sha256sum(pubkey, pubkeylen,
                             sha256sumdigest, CURL_SHA256_DIGEST_LENGTH);

    free(sha256sumdigest);
    return rc;
}
