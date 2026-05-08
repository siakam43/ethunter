/* CG-Bench fixture: fnptr-struct/example_5 */
/* fnptr: uic->uic_cb, targets: spacemap_check_sm_log_cb, load_unflushed_cb, load_unflushed_svr_segs_cb, count_unflushed_space_cb, log_spacemap_obsolete_stats_cb */

static int iterate_through_spacemap_logs_cb(space_map_entry_t *sme, void *arg)
{
	unflushed_iter_cb_arg_t *uic = arg;
	return (uic->uic_cb(uic->uic_spa, sme, uic->uic_txg, uic->uic_arg));
}

static void iterate_through_spacemap_logs(spa_t *spa, zdb_log_sm_cb_t cb, void *arg)
{
	if (!spa_feature_is_active(spa, SPA_FEATURE_LOG_SPACEMAP))
		return;

	spa_config_enter(spa, SCL_CONFIG, FTAG, RW_READER);
	for (spa_log_sm_t *sls = avl_first(&spa->spa_sm_logs_by_txg);
	    sls; sls = AVL_NEXT(&spa->spa_sm_logs_by_txg, sls)) {
		...

		unflushed_iter_cb_arg_t uic = {
			.uic_spa = spa,
			.uic_txg = sls->sls_txg,
			.uic_arg = arg,
			.uic_cb = cb
		};
		VERIFY0(space_map_iterate(sm, space_map_length(sm),
		    iterate_through_spacemap_logs_cb, &uic));
		space_map_close(sm);
	}
	spa_config_exit(spa, SCL_CONFIG, FTAG);
}

int space_map_iterate(space_map_t *sm, uint64_t end, sm_cb_t callback, void *arg)
{
	uint64_t blksz = sm->sm_blksz;

	...
	for (uint64_t block_base = 0; block_base < end && error == 0;
	    block_base += blksz) {
		...

		for (uint64_t *block_cursor = block_start;
		    block_cursor < block_end && error == 0; block_cursor++) {
			uint64_t e = *block_cursor;

			...
			error = callback(&sme, arg);
		}
		dmu_buf_rele(db, FTAG);
	}
	return (error);
}

static void spacemap_check_sm_log(spa_t *spa, metaslab_verify_t *mv)
{
	iterate_through_spacemap_logs(spa, spacemap_check_sm_log_cb, mv);
}

static void load_unflushed_to_ms_allocatables(spa_t *spa, maptype_t maptype)
{
	iterate_through_spacemap_logs(spa, load_unflushed_cb, &maptype);
}

static void zdb_claim_removing(spa_t *spa, zdb_cb_t *zcb)
{
	...
	iterate_through_spacemap_logs(spa, load_unflushed_svr_segs_cb, svr);
        ...
}

static int64_t get_unflushed_alloc_space(spa_t *spa)
{
	if (dump_opt['L'])
		return (0);

	int64_t ualloc_space = 0;
	iterate_through_spacemap_logs(spa, count_unflushed_space_cb,
	    &ualloc_space);
	return (ualloc_space);
}

static void dump_log_spacemap_obsolete_stats(spa_t *spa)
{
	...
	iterate_through_spacemap_logs(spa,
	    log_spacemap_obsolete_stats_cb, &lsos);
    ...
}


/* Stub implementation for spacemap_check_sm_log_cb */
void spacemap_check_sm_log_cb(void) {}



/* Stub implementation for load_unflushed_cb */
void load_unflushed_cb(void) {}



/* Stub implementation for load_unflushed_svr_segs_cb */
void load_unflushed_svr_segs_cb(void) {}



/* Stub implementation for count_unflushed_space_cb */
void count_unflushed_space_cb(void) {}



/* Stub implementation for log_spacemap_obsolete_stats_cb */
void log_spacemap_obsolete_stats_cb(void) {}
