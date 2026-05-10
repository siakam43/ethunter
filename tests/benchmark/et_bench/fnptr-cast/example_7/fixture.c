/* et_bench fixture: fnptr-cast/example_7 */
/* fnptr: ops->compute_native, targets: fletcher_4_scalar_native, fletcher_4_superscalar_native, fletcher_4_superscalar4_native */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define ASSERT(x) assert(x)
#define IS_P2ALIGNED(v, a) (((v) & ((a) - 1)) == 0)
#define P2ALIGN(v, a) ((v) & ~((a) - 1))
#define B_TRUE 1
#define B_FALSE 0
#define FLETCHER_MIN_SIMD_SIZE 256
#define IMPL_FASTEST 0
#define IMPL_CYCLE 1

typedef int boolean_t;

/* --- Fletcher-4 operation types --- */
typedef struct fletcher_4_ctx fletcher_4_ctx_t;

typedef void (*fletcher_4_init_t)(fletcher_4_ctx_t *);
typedef void (*fletcher_4_fini_t)(fletcher_4_ctx_t *);
typedef void (*fletcher_4_compute_t)(fletcher_4_ctx_t *, void *, size_t);
typedef boolean_t (*fletcher_4_valid_t)(const uint8_t *, size_t, const uint8_t *);

/* Ops struct — defines compute_native fnptr slot */
typedef struct fletcher_4_ops {
    fletcher_4_init_t    init_native;
    fletcher_4_fini_t    fini_native;
    fletcher_4_compute_t compute_native;
    fletcher_4_init_t    init_byteswap;
    fletcher_4_fini_t    fini_byteswap;
    fletcher_4_compute_t compute_byteswap;
    fletcher_4_valid_t   valid;
    boolean_t            uses_fpu;
    const char          *name;
} fletcher_4_ops_t;

/* ABD checksum types */
typedef struct {
    fletcher_4_ctx_t *acd_ctx;
    void *acd_private;
    int acd_byteorder;
} zio_abd_checksum_data_t;

typedef struct abd abd_t;

#define ZIO_CHECKSUM_NATIVE 1
#define ZIO_CHECKSUM_BSWAP   2

/* --- Stub helpers --- */
static void abd_iterate_func(abd_t *abd, size_t off, size_t size, int (*func)(void *, size_t, void *), void *priv);
static int kfpu_allowed(void);
static void kfpu_begin(void);

struct fletcher_4_ctx {
    uint32_t a, b, c, d;
};

static void abd_fletcher_4_simd2scalar(int native, void *data, size_t size, zio_abd_checksum_data_t *cdp) {
    (void)native; (void)data; (void)size; (void)cdp;
}

/* --- Target functions: three native compute implementations --- */
static void
fletcher_4_scalar_native(fletcher_4_ctx_t *ctx, void *data, size_t size)
{
    uint32_t *buf = (uint32_t *)data;
    size_t n = size / sizeof(uint32_t);
    for (size_t i = 0; i < n; i++) {
        ctx->a += buf[i];
        ctx->b += ctx->a;
    }
}

static void
fletcher_4_superscalar_native(fletcher_4_ctx_t *ctx, void *data, size_t size)
{
    uint32_t *buf = (uint32_t *)data;
    size_t n = size / sizeof(uint32_t);
    for (size_t i = 0; i < n; i += 2) {
        ctx->a += buf[i];
        ctx->b += ctx->a;
        if (i + 1 < n) {
            ctx->a += buf[i + 1];
            ctx->b += ctx->a;
        }
    }
}

static void
fletcher_4_superscalar4_native(fletcher_4_ctx_t *ctx, void *data, size_t size)
{
    uint32_t *buf = (uint32_t *)data;
    size_t n = size / sizeof(uint32_t);
    for (size_t i = 0; i < n; i += 4) {
        ctx->a += buf[i];
        ctx->b += ctx->a;
        if (i + 1 < n) { ctx->a += buf[i+1]; ctx->b += ctx->a; }
        if (i + 2 < n) { ctx->a += buf[i+2]; ctx->b += ctx->a; }
        if (i + 3 < n) { ctx->a += buf[i+3]; ctx->b += ctx->a; }
    }
}

/* Stub init/fini/valid functions */
static void fletcher_4_scalar_init(fletcher_4_ctx_t *ctx) { ctx->a = ctx->b = ctx->c = ctx->d = 0; }
static void fletcher_4_scalar_fini(fletcher_4_ctx_t *ctx) { (void)ctx; }
static void fletcher_4_superscalar_init(fletcher_4_ctx_t *ctx) { ctx->a = ctx->b = ctx->c = ctx->d = 0; }
static void fletcher_4_superscalar_fini(fletcher_4_ctx_t *ctx) { (void)ctx; }
static void fletcher_4_superscalar4_init(fletcher_4_ctx_t *ctx) { ctx->a = ctx->b = ctx->c = ctx->d = 0; }
static void fletcher_4_superscalar4_fini(fletcher_4_ctx_t *ctx) { (void)ctx; }
static boolean_t fletcher_4_scalar_valid(const uint8_t *a, size_t n, const uint8_t *b) { (void)a; (void)n; (void)b; return B_TRUE; }
static boolean_t fletcher_4_superscalar_valid(const uint8_t *a, size_t n, const uint8_t *b) { (void)a; (void)n; (void)b; return B_TRUE; }
static boolean_t fletcher_4_superscalar4_valid(const uint8_t *a, size_t n, const uint8_t *b) { (void)a; (void)n; (void)b; return B_TRUE; }

/* --- Ops tables: each ops struct maps to a different target via cast-compatible assignment --- */
static const fletcher_4_ops_t fletcher_4_scalar_ops = {
    .init_native = fletcher_4_scalar_init,
    .fini_native = fletcher_4_scalar_fini,
    .compute_native = fletcher_4_scalar_native,
    .init_byteswap = fletcher_4_scalar_init,
    .fini_byteswap = fletcher_4_scalar_fini,
    .compute_byteswap = fletcher_4_scalar_native,
    .valid = fletcher_4_scalar_valid,
    .uses_fpu = B_FALSE,
    .name = "scalar"
};

static const fletcher_4_ops_t fletcher_4_superscalar_ops = {
    .init_native = fletcher_4_superscalar_init,
    .fini_native = fletcher_4_superscalar_fini,
    .compute_native = fletcher_4_superscalar_native,
    .init_byteswap = fletcher_4_superscalar_init,
    .fini_byteswap = fletcher_4_superscalar_fini,
    .compute_byteswap = fletcher_4_superscalar_native,
    .valid = fletcher_4_superscalar_valid,
    .uses_fpu = B_FALSE,
    .name = "superscalar"
};

static const fletcher_4_ops_t fletcher_4_superscalar4_ops = {
    .init_native = fletcher_4_superscalar4_init,
    .fini_native = fletcher_4_superscalar4_fini,
    .compute_native = fletcher_4_superscalar4_native,
    .init_byteswap = fletcher_4_superscalar4_init,
    .fini_byteswap = fletcher_4_superscalar4_fini,
    .compute_byteswap = fletcher_4_superscalar4_native,
    .valid = fletcher_4_superscalar4_valid,
    .uses_fpu = B_FALSE,
    .name = "superscalar4"
};

static fletcher_4_ops_t fletcher_4_fastest_impl = {
    .name = "fastest",
    .valid = fletcher_4_scalar_valid
};

static const fletcher_4_ops_t *fletcher_4_impls[] = {
    &fletcher_4_scalar_ops,
    &fletcher_4_superscalar_ops,
    &fletcher_4_superscalar4_ops
};

static const fletcher_4_ops_t *fletcher_4_supp_impls[] = {
    &fletcher_4_scalar_ops,
    &fletcher_4_superscalar_ops,
    &fletcher_4_superscalar4_ops
};
static int fletcher_4_supp_impls_cnt = 3;
static int fletcher_4_initialized = 1;

/* Runtime implementation selector */
static struct { uint32_t chosen; } fletcher_4_impl_chosen;

static inline const fletcher_4_ops_t *
fletcher_4_impl_get(void)
{
    if (!kfpu_allowed())
        return &fletcher_4_superscalar4_ops;

    const fletcher_4_ops_t *ops = NULL;
    uint32_t impl = fletcher_4_impl_chosen.chosen;

    switch (impl) {
    case IMPL_FASTEST:
        ops = &fletcher_4_fastest_impl;
        break;
    case IMPL_CYCLE: {
        static uint32_t cycle_count = 0;
        uint32_t idx = (++cycle_count) % fletcher_4_supp_impls_cnt;
        ops = fletcher_4_supp_impls[idx];
        break;
    }
    default:
        ops = fletcher_4_supp_impls[impl];
        break;
    }
    return ops;
}

/* --- Caller: abd_fletcher_4_iter — calls through ops->compute_native --- */
static int
abd_fletcher_4_iter(void *data, size_t size, void *private)
{
    zio_abd_checksum_data_t *cdp = (zio_abd_checksum_data_t *)private;
    fletcher_4_ctx_t *ctx = cdp->acd_ctx;
    fletcher_4_ops_t *ops = (fletcher_4_ops_t *)cdp->acd_private;
    boolean_t native = cdp->acd_byteorder == ZIO_CHECKSUM_NATIVE;
    uint64_t asize = P2ALIGN(size, FLETCHER_MIN_SIMD_SIZE);

    ASSERT(IS_P2ALIGNED(size, sizeof(uint32_t)));

    if (asize > 0) {
        if (native)
            ops->compute_native(ctx, data, asize);
        else
            ops->compute_byteswap(ctx, data, asize);

        size -= asize;
        data = (char *)data + asize;
    }

    if (size > 0) {
        abd_fletcher_4_simd2scalar(native, data, size, cdp);
    }

    return 0;
}

/* ABD checksum function table */
typedef void (*zio_abd_checksum_init_t)(zio_abd_checksum_data_t *);
typedef void (*zio_abd_checksum_fini_t)(zio_abd_checksum_data_t *);
typedef int (*zio_abd_checksum_iter_t)(void *, size_t, void *);

typedef struct zio_abd_checksum_func {
    zio_abd_checksum_init_t *acf_init;
    zio_abd_checksum_fini_t *acf_fini;
    zio_abd_checksum_iter_t *acf_iter;
} zio_abd_checksum_func_t;

static void abd_fletcher_4_init(zio_abd_checksum_data_t *cdp) {
    const fletcher_4_ops_t *ops = fletcher_4_impl_get();
    cdp->acd_private = (void *)ops;
    if (ops->uses_fpu == B_TRUE)
        kfpu_begin();
    if (cdp->acd_byteorder == ZIO_CHECKSUM_NATIVE)
        ops->init_native(cdp->acd_ctx);
    else
        ops->init_byteswap(cdp->acd_ctx);
}

static void abd_fletcher_4_fini(zio_abd_checksum_data_t *cdp) { (void)cdp; }

zio_abd_checksum_func_t fletcher_4_abd_ops = {
    .acf_init = abd_fletcher_4_init,
    .acf_fini = abd_fletcher_4_fini,
    .acf_iter = abd_fletcher_4_iter
};

static inline void
abd_fletcher_4_impl(abd_t *abd, uint64_t size, zio_abd_checksum_data_t *acdp)
{
    fletcher_4_abd_ops.acf_init(acdp);
    abd_iterate_func(abd, 0, size, fletcher_4_abd_ops.acf_iter, acdp);
    fletcher_4_abd_ops.acf_fini(acdp);
}

/* Stub helpers */
static void abd_iterate_func(abd_t *abd, size_t off, size_t size, int (*func)(void *, size_t, void *), void *priv) { (void)abd; (void)off; (void)size; func(priv, size, priv); }
static int kfpu_allowed(void) { return 0; }
static void kfpu_begin(void) {}
