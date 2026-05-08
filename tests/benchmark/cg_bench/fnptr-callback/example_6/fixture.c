/* CG-Bench fixture: fnptr-callback/example_6 */
/* fnptr: ptr_getter, targets: tcache_bin_flush_ptr_getter */

JEMALLOC_ALWAYS_INLINE void
emap_edata_lookup_batch(tsd_t *tsd, emap_t *emap, size_t nptrs,
    emap_ptr_getter ptr_getter, void *ptr_getter_ctx,
    emap_metadata_visitor metadata_visitor, void *metadata_visitor_ctx,
    emap_batch_lookup_result_t *result) {
	...

	for (size_t i = 0; i < nptrs; i++) {
		const void *ptr = ptr_getter(ptr_getter_ctx, i);

		result[i].rtree_leaf = rtree_leaf_elm_lookup(tsd_tsdn(tsd),
		    &emap->rtree, rtree_ctx, (uintptr_t)ptr,
		    /* dependent */ true, /* init_missing */ false);
	}

	...
}

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


/* Stub implementation for tcache_bin_flush_ptr_getter */
void tcache_bin_flush_ptr_getter(void) {}
