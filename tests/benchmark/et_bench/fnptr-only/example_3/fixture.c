/* ET-Bench fixture: fnptr-only/example_3 */
/* fnptr: md_final_raw, targets: tls1_md5_final_raw, tls1_sha1_final_raw, tls1_sha256_final_raw, tls1_sha512_final_raw */
/* Source: OpenSSL-style TLS record layer */

#include <stdlib.h>
#include <string.h>

#define NID_md5     4
#define NID_sha1    64
#define NID_sha224  675
#define NID_sha256  672
#define NID_sha384  673
#define NID_sha512  674

#define LARGEST_DIGEST_CTX 128

typedef struct {
    int type;
} EVP_MD_CTX;

static int EVP_MD_CTX_type(const EVP_MD_CTX *ctx) {
    return ctx->type;
}

static void tls1_md5_final_raw(void *ctx, unsigned char *md_out) {
    memset(md_out, 0, 16);
}

static void tls1_sha1_final_raw(void *ctx, unsigned char *md_out) {
    memset(md_out, 0, 20);
}

static void tls1_sha256_final_raw(void *ctx, unsigned char *md_out) {
    memset(md_out, 0, 32);
}

static void tls1_sha512_final_raw(void *ctx, unsigned char *md_out) {
    memset(md_out, 0, 64);
}

static int md_final_raw_caller(const EVP_MD_CTX *ctx,
                                unsigned char *mac_out,
                                size_t mac_out_size)
{
    union {
        double align;
        unsigned char c[sizeof(LARGEST_DIGEST_CTX)];
    } md_state;

    void (*md_final_raw)(void *ctx, unsigned char *md_out);
    size_t md_size = 0;

    switch (EVP_MD_CTX_type(ctx)) {
    case NID_md5:
        md_final_raw = tls1_md5_final_raw;
        md_size = 16;
        break;
    case NID_sha1:
        md_final_raw = tls1_sha1_final_raw;
        md_size = 20;
        break;
    case NID_sha224:
        md_final_raw = tls1_sha256_final_raw;
        md_size = 28;
        break;
    case NID_sha256:
        md_final_raw = tls1_sha256_final_raw;
        md_size = 32;
        break;
    case NID_sha384:
        md_final_raw = tls1_sha512_final_raw;
        md_size = 48;
        break;
    case NID_sha512:
        md_final_raw = tls1_sha512_final_raw;
        md_size = 64;
        break;
    default:
        return 0;
    }

    if (md_size > mac_out_size)
        return 0;

    md_final_raw(md_state.c, mac_out);
    return 1;
}
