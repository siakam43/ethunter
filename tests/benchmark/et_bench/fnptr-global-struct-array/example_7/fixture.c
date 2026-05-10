/* ET-Bench fixture: fnptr-global-struct-array/example_7 */
/* fnptr: ops->transform, targets: sha256_generic, sha512_generic, tf_sha512_transform_x64, tf_sha256_transform_x64 */
/* Pattern: SHA2 ops table with transform function pointers selected at init, used in update */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define likely(x) __builtin_expect(!!(x), 1)

#define SHA256_HMAC_BLOCK_SIZE 64
#define SHA512_HMAC_BLOCK_SIZE 128

#define SHA256_MECH_INFO_TYPE 1
#define SHA256_HMAC_MECH_INFO_TYPE 2
#define SHA256_HMAC_GEN_MECH_INFO_TYPE 3
#define SHA384_MECH_INFO_TYPE 4
#define SHA384_HMAC_MECH_INFO_TYPE 5
#define SHA384_HMAC_GEN_MECH_INFO_TYPE 6
#define SHA512_MECH_INFO_TYPE 7
#define SHA512_HMAC_MECH_INFO_TYPE 8
#define SHA512_HMAC_GEN_MECH_INFO_TYPE 9
#define SHA512_224_MECH_INFO_TYPE 10
#define SHA512_256_MECH_INFO_TYPE 11

#define IMPL_FASTEST 0
#define IMPL_CYCLE 1

/* Transform function pointer types */
typedef struct sha256_ops {
    const char *name;
    void (*transform)(uint32_t *state, const uint8_t *data, uint32_t blocks);
    int (*is_supported)(void);
} sha256_ops_t;

typedef struct sha512_ops {
    const char *name;
    void (*transform)(uint64_t *state, const uint8_t *data, uint32_t blocks);
    int (*is_supported)(void);
} sha512_ops_t;

/* SHA2 context structs */
typedef struct sha256_ctx {
    uint32_t state[8];
    uint64_t count[2];
    uint8_t wbuf[64];
    const sha256_ops_t *ops;
} sha256_ctx;

typedef struct sha512_ctx {
    uint64_t state[8];
    uint64_t count[2];
    uint8_t wbuf[128];
    const sha512_ops_t *ops;
} sha512_ctx;

typedef struct {
    int algotype;
    sha256_ctx sha256;
    sha512_ctx sha512;
} SHA2_CTX;

typedef struct {
    int hc_mech_type;
    SHA2_CTX hc_icontext;
    SHA2_CTX hc_ocontext;
} sha2_hmac_ctx_t;

/* Target transform functions */
static void sha256_generic(uint32_t *state, const uint8_t *data, uint32_t blocks) {
    (void)state; (void)data; (void)blocks;
}

static void sha512_generic(uint64_t *state, const uint8_t *data, uint32_t blocks) {
    (void)state; (void)data; (void)blocks;
}

static void tf_sha512_transform_x64(uint64_t *state, const uint8_t *data, uint32_t blocks) {
    (void)state; (void)data; (void)blocks;
}

static void tf_sha256_transform_x64(uint32_t *state, const uint8_t *data, uint32_t blocks) {
    (void)state; (void)data; (void)blocks;
}

static int sha2_is_supported(void) { return 1; }

/* Implementation tables */
static const sha256_ops_t *const sha256_impls[] = {
    &sha256_generic_impl,
    &sha256_x64_impl,
};

static const sha512_ops_t *const sha512_impls[] = {
    &sha512_generic_impl,
    &sha512_x64_impl,
};

static const sha256_ops_t sha256_generic_impl = {
    "generic", sha256_generic, sha2_is_supported,
};

static const sha512_ops_t sha512_generic_impl = {
    "generic", sha512_generic, sha2_is_supported,
};

static const sha512_ops_t sha512_x64_impl = {
    "x64", tf_sha512_transform_x64, sha2_is_supported,
};

static const sha256_ops_t sha256_x64_impl = {
    "x64", tf_sha256_transform_x64, sha2_is_supported,
};

static uint32_t generic_impl_chosen = 0;
static const sha256_ops_t *generic_supp_impls[4];
static const sha512_ops_t *generic_supp_impls512[4];
static int generic_supp_impls_cnt = 0;
static sha256_ops_t generic_fastest_impl = {.name = "fastest"};

static void generic_impl_init(void) {
    int i, c;
    if (likely(generic_supp_impls_cnt != 0))
        return;
    for (i = 0, c = 0; i < ARRAY_SIZE(sha256_impls); i++) {
        const sha256_ops_t *impl = sha256_impls[i];
        if (impl->is_supported && impl->is_supported())
            generic_supp_impls[c++] = impl;
    }
    generic_supp_impls_cnt = c;
}

static uint32_t IMPL_READ(uint32_t v) { return v; }

static const sha256_ops_t *sha256_get_ops(void) {
    const sha256_ops_t *ops = NULL;
    uint32_t idx, impl = IMPL_READ(generic_impl_chosen);
    static uint32_t cycle_count = 0;

    generic_impl_init();
    switch (impl) {
    case IMPL_FASTEST:
        ops = &generic_fastest_impl;
        break;
    case IMPL_CYCLE:
        idx = (++cycle_count) % generic_supp_impls_cnt;
        ops = generic_supp_impls[idx];
        break;
    default:
        ops = generic_supp_impls[impl];
        break;
    }
    return ops;
}

static const sha512_ops_t *sha512_get_ops(void) {
    (void)sha256_get_ops; /* reuse selection logic */
    return &sha512_generic_impl;
}

/* Caller: ops->transform called through sha256_update */
static void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len) {
    uint64_t pos = ctx->count[0];
    uint64_t total = ctx->count[1];
    uint8_t *m = ctx->wbuf;
    const sha256_ops_t *ops = ctx->ops;

    if (pos && pos + len >= 64) {
        memcpy(m + pos, data, 64 - pos);
        ops->transform(ctx->state, m, 1);
        len -= 64 - pos;
        total += (64 - pos) * 8;
        data += 64 - pos;
        pos = 0;
    }

    if (len >= 64) {
        uint32_t blocks = len / 64;
        uint32_t bytes = blocks * 64;
        ops->transform(ctx->state, data, blocks);
        len -= bytes;
        total += (bytes) * 8;
        data += bytes;
    }
    memcpy(m + pos, data, len);

    pos += len;
    total += len * 8;
    ctx->count[0] = pos;
    ctx->count[1] = total;
}

static void sha512_update(sha512_ctx *ctx, const uint8_t *data, size_t len) {
    (void)ctx; (void)data; (void)len;
}

void SHA2Update(SHA2_CTX *ctx, const void *data, size_t len) {
    if (len == 0)
        return;
    switch (ctx->algotype) {
    case SHA256_MECH_INFO_TYPE:
    case SHA256_HMAC_MECH_INFO_TYPE:
    case SHA256_HMAC_GEN_MECH_INFO_TYPE:
        sha256_update(&ctx->sha256, data, len);
        break;
    case SHA384_MECH_INFO_TYPE:
    case SHA512_MECH_INFO_TYPE:
        sha512_update(&ctx->sha512, data, len);
        break;
    }
}

void SHA2Init(int algotype, SHA2_CTX *ctx) {
    sha256_ctx *ctx256 = &ctx->sha256;
    sha512_ctx *ctx512 = &ctx->sha512;

    memset(ctx, 0, sizeof(*ctx));
    ctx->algotype = algotype;
    switch (ctx->algotype) {
    case SHA256_MECH_INFO_TYPE:
        ctx256->count[0] = 0;
        ctx256->ops = sha256_get_ops();
        break;
    case SHA512_MECH_INFO_TYPE:
        ctx512->count[0] = 0;
        ctx512->ops = sha512_get_ops();
        break;
    }
}
