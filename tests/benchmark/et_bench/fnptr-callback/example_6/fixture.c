/* ET-Bench fixture: fnptr-callback/example_6 */
/* Based on jemalloc's emap_edata_lookup_batch pattern */
/* fnptr: ptr_getter, targets: tcache_bin_flush_ptr_getter */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef int tsdn_t;
typedef struct tsd { tsdn_t tsdn; } tsd_t;

static tsdn_t *tsd_tsdn(tsd_t *tsd) { return &tsd->tsdn; }

typedef struct {
    void *rtree_leaf;
} emap_batch_lookup_result_t;

typedef struct emap {
    int dummy;
} emap_t;

typedef struct rtree { int dummy; } rtree_t;
typedef int rtree_ctx_t;
static rtree_ctx_t rtree_ctx_storage;
#define rtree_ctx (&rtree_ctx_storage)

typedef struct {
    rtree_t rtree;
} emap_global_t;
static emap_global_t arena_emap_global;

typedef const void *(*emap_ptr_getter)(void *ctx, size_t i);
typedef void (*emap_metadata_visitor)(void *ctx, size_t i, void *result);

static void *rtree_leaf_elm_lookup(tsdn_t *tsdn, rtree_t *rtree,
    rtree_ctx_t *ctx, uintptr_t ptr, int dependent, int init_missing) {
    return (void *)(uintptr_t)ptr;
}

static void
emap_edata_lookup_batch(tsd_t *tsd, emap_global_t *emap, size_t nptrs,
    emap_ptr_getter ptr_getter, void *ptr_getter_ctx,
    emap_metadata_visitor metadata_visitor, void *metadata_visitor_ctx,
    emap_batch_lookup_result_t *result) {
    for (size_t i = 0; i < nptrs; i++) {
        const void *ptr = ptr_getter(ptr_getter_ctx, i);

        result[i].rtree_leaf = rtree_leaf_elm_lookup(tsd_tsdn(tsd),
            &emap->rtree, rtree_ctx, (uintptr_t)ptr,
            1, 0);
    }
}

typedef struct cache_bin_ptr_array {
    void **ptrs;
    size_t count;
} cache_bin_ptr_array_t;

typedef unsigned int szind_t;

static void
tcache_bin_flush_size_check_fail(cache_bin_ptr_array_t *arr, szind_t binind,
    size_t nflush, emap_batch_lookup_result_t *edatas) {
    for (size_t i = 0; i < nflush; i++) {
        if (edatas[i].rtree_leaf == NULL)
            abort();
    }
}

const void *
tcache_bin_flush_ptr_getter(void *ctx, size_t i) {
    cache_bin_ptr_array_t *arr = (cache_bin_ptr_array_t *)ctx;
    if (i < arr->count)
        return arr->ptrs[i];
    return NULL;
}

static void tcache_bin_flush_metadata_visitor(void *ctx, size_t i, void *result) {}

static int config_opt_safety_checks = 1;
#define unlikely(x) __builtin_expect(!!(x), 0)

static void
tcache_bin_flush_edatas_lookup(tsd_t *tsd, cache_bin_ptr_array_t *arr,
    szind_t binind, size_t nflush, emap_batch_lookup_result_t *edatas) {

    size_t szind_sum = binind * nflush;
    emap_edata_lookup_batch(tsd, &arena_emap_global, nflush,
        &tcache_bin_flush_ptr_getter, (void *)arr,
        &tcache_bin_flush_metadata_visitor, (void *)&szind_sum,
        edatas);
    if (config_opt_safety_checks && unlikely(szind_sum != 0)) {
        tcache_bin_flush_size_check_fail(arr, binind, nflush, edatas);
    }
}
