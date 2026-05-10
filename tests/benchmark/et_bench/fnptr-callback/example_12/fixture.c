/* ET-Bench fixture: fnptr-callback/example_12 */
/* Based on curl's auth_create_digest_http_message pattern (hash fnptr) */
/* fnptr: hash, targets: Curl_md5it, Curl_sha256it */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum {
    CURLE_OK = 0,
    CURLE_OUT_OF_MEMORY
} CURLcode;

typedef struct {
    int algo;
    int userhash;
    const char *realm;
} digestdata;

typedef struct Curl_easy {
    int dummy;
} Curl_easy;

#define ALGO_MD5SESS    3
#define ALGO_SHA512_256SESS 6
#define DEBUGASSERT(x)  ((void)0)

static CURLcode auth_create_digest_http_message(
    Curl_easy *data,
    const char *userp,
    const char *passwdp,
    const unsigned char *request,
    const unsigned char *uripath,
    digestdata *digest,
    char **outptr, size_t *outlen,
    void (*convert_to_ascii)(unsigned char *, unsigned char *),
    CURLcode (*hash)(unsigned char *, const unsigned char *, const size_t))
{
    unsigned char hashbuf[64];
    char *hashthis = NULL;
    size_t hashlen = 0;

    if (userp && digest->realm)
        hashlen = strlen(userp) + 1 + strlen(digest->realm);
    hashthis = (char *)malloc(hashlen + 1);
    if (!hashthis)
        return CURLE_OUT_OF_MEMORY;

    if (digest->realm)
        snprintf(hashthis, hashlen + 1, "%s:%s", userp, digest->realm);
    else
        snprintf(hashthis, hashlen + 1, "%s:", userp);

    hash(hashbuf, (const unsigned char *)hashthis, strlen(hashthis));
    free(hashthis);

    return CURLE_OK;
}

CURLcode Curl_auth_create_digest_http_message(
    Curl_easy *data,
    const char *userp,
    const char *passwdp,
    const unsigned char *request,
    const unsigned char *uripath,
    digestdata *digest,
    char **outptr, size_t *outlen)
{
    if (digest->algo <= ALGO_MD5SESS)
        return auth_create_digest_http_message(data, userp, passwdp,
                                               request, uripath, digest,
                                               outptr, outlen,
                                               auth_digest_md5_to_ascii,
                                               Curl_md5it);

    DEBUGASSERT(digest->algo <= ALGO_SHA512_256SESS);
    return auth_create_digest_http_message(data, userp, passwdp,
                                           request, uripath, digest,
                                           outptr, outlen,
                                           auth_digest_sha256_to_ascii,
                                           Curl_sha256it);
}

CURLcode Curl_md5it(unsigned char *out, const unsigned char *in, const size_t len) {
    memset(out, 0, 16);
    return CURLE_OK;
}

CURLcode Curl_sha256it(unsigned char *out, const unsigned char *in, const size_t len) {
    memset(out, 0, 32);
    return CURLE_OK;
}

void auth_digest_md5_to_ascii(unsigned char *src, unsigned char *dst) {}
void auth_digest_sha256_to_ascii(unsigned char *src, unsigned char *dst) {}
