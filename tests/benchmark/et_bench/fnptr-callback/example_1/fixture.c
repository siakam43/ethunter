/* ET-Bench fixture: fnptr-callback/example_1 */
/* Based on curl's auth_create_digest_http_message pattern */
/* fnptr: convert_to_ascii, targets: auth_digest_md5_to_ascii, auth_digest_sha256_to_ascii */

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
    char userh[128];
    char *hashthis = NULL;

    if (digest->userhash) {
        hashthis = (char *)malloc(256);
        if (!hashthis)
            return CURLE_OUT_OF_MEMORY;
        snprintf(hashthis, 256, "%s:%s", userp,
                 digest->realm ? digest->realm : "");

        hash(hashbuf, (const unsigned char *)hashthis, strlen(hashthis));
        free(hashthis);

        convert_to_ascii(hashbuf, (unsigned char *)userh);
    }
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

void auth_digest_md5_to_ascii(unsigned char *src, unsigned char *dst) {
    for (int i = 0; i < 16; i++)
        dst[i] = src[i] ? src[i] : 0;
}

void auth_digest_sha256_to_ascii(unsigned char *src, unsigned char *dst) {
    for (int i = 0; i < 32; i++)
        dst[i] = src[i] ? src[i] : 0;
}
