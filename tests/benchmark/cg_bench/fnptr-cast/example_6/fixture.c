/* CG-Bench fixture: fnptr-cast/example_6 */
/* fnptr: holdfunc, targets: dsl_dataset_hold, dsl_dataset_hold_obj_string */

static void
dsl_dataset_user_release_sync(void *arg, dmu_tx_t *tx)
{
	dsl_dataset_user_release_arg_t *ddura = arg;
	dsl_holdfunc_t *holdfunc = ddura->ddura_holdfunc;
	dsl_pool_t *dp = dmu_tx_pool(tx);

	ASSERT(RRW_WRITE_HELD(&dp->dp_config_rwlock));

	for (nvpair_t *pair = nvlist_next_nvpair(ddura->ddura_chkholds, NULL);
	    pair != NULL; pair = nvlist_next_nvpair(ddura->ddura_chkholds,
	    pair)) {
		dsl_dataset_t *ds;
		const char *name = nvpair_name(pair);

		VERIFY0(holdfunc(dp, name, FTAG, &ds));

		dsl_dataset_user_release_sync_one(ds,
		    fnvpair_value_nvlist(pair), tx);
		if (nvlist_exists(ddura->ddura_todelete, name)) {
			ASSERT(ds->ds_userrefs == 0 &&
			    dsl_dataset_phys(ds)->ds_num_children == 1 &&
			    DS_IS_DEFER_DESTROY(ds));
			dsl_destroy_snapshot_sync_impl(ds, B_FALSE, tx);
		}
		dsl_dataset_rele(ds, FTAG);
	}
}

static int
dsl_dataset_user_release_impl(nvlist_t *holds, nvlist_t *errlist,
    dsl_pool_t *tmpdp)
{
	dsl_dataset_user_release_arg_t ddura;
	nvpair_t *pair;
	const char *pool;
	int error;

	pair = nvlist_next_nvpair(holds, NULL);
	if (pair == NULL)
		return (0);

	/*
	 * The release may cause snapshots to be destroyed; make sure they
	 * are not mounted.
	 */
	if (tmpdp != NULL) {
		/* Temporary holds are specified by dsobj string. */
		ddura.ddura_holdfunc = dsl_dataset_hold_obj_string;
		pool = spa_name(tmpdp->dp_spa);
    ...
	} else {
		/* Non-temporary holds are specified by name. */
		ddura.ddura_holdfunc = dsl_dataset_hold;
		pool = nvpair_name(pair);
    ...
	}

	ddura.ddura_holds = holds;
	ddura.ddura_errlist = errlist;
	VERIFY0(nvlist_alloc(&ddura.ddura_todelete, NV_UNIQUE_NAME,
	    KM_SLEEP));
	VERIFY0(nvlist_alloc(&ddura.ddura_chkholds, NV_UNIQUE_NAME,
	    KM_SLEEP));

	error = dsl_sync_task(pool, dsl_dataset_user_release_check,
	    dsl_dataset_user_release_sync, &ddura, 0,
	    ZFS_SPACE_CHECK_EXTRA_RESERVED);
	fnvlist_free(ddura.ddura_todelete);
	fnvlist_free(ddura.ddura_chkholds);

	return (error);
}

int
dsl_sync_task(const char *pool, dsl_checkfunc_t *checkfunc,
    dsl_syncfunc_t *syncfunc, void *arg,
    int blocks_modified, zfs_space_check_t space_check)
{
	return (dsl_sync_task_common(pool, checkfunc, syncfunc, NULL, arg,
	    blocks_modified, space_check, B_FALSE));
}

static int
dsl_sync_task_common(const char *pool, dsl_checkfunc_t *checkfunc,
    dsl_syncfunc_t *syncfunc, dsl_sigfunc_t *sigfunc, void *arg,
    int blocks_modified, zfs_space_check_t space_check, boolean_t early)
{
	...

	err = spa_open(pool, &spa, FTAG);
	if (err != 0)
		return (err);
	dp = spa_get_dsl(spa);

top:
	tx = dmu_tx_create_dd(dp->dp_mos_dir);
	VERIFY0(dmu_tx_assign(tx, TXG_WAIT));

	dst.dst_pool = dp;
	dst.dst_txg = dmu_tx_get_txg(tx);
	dst.dst_space = blocks_modified << DST_AVG_BLKSHIFT;
	dst.dst_space_check = space_check;
	dst.dst_checkfunc = checkfunc != NULL ? checkfunc : dsl_null_checkfunc;
	dst.dst_syncfunc = syncfunc;
	dst.dst_arg = arg;
	dst.dst_error = 0;
	dst.dst_nowaiter = B_FALSE;

	...

	spa_close(spa, FTAG);
	return (dst.dst_error);
}

int
spa_open(const char *name, spa_t **spapp, const void *tag)
{
	return (spa_open_common(name, spapp, tag, NULL, NULL));
}

static int
spa_open_common(const char *pool, spa_t **spapp, const void *tag,
    nvlist_t *nvpolicy, nvlist_t **config)
{
	...

	if (spa->spa_state == POOL_STATE_UNINITIALIZED) {
		zpool_load_policy_t policy;

		firstopen = B_TRUE;

		...

		if (state != SPA_LOAD_RECOVER)
			spa->spa_last_ubsync_txg = spa->spa_load_txg = 0;
		spa->spa_config_source = SPA_CONFIG_SRC_CACHEFILE;

		zfs_dbgmsg("spa_open_common: opening %s", pool);
		error = spa_load_best(spa, state, policy.zlp_txg,
		    policy.zlp_rewind);

		if (error == EBADF) {
			...
			return (SET_ERROR(ENOENT));
		}
	}

	return (0);
}

static int
spa_load_best(spa_t *spa, spa_load_state_t state, uint64_t max_request,
    int rewind_flags)
{
	nvlist_t *loadinfo = NULL;
	nvlist_t *config = NULL;
	int load_error, rewind_error;
	uint64_t safe_rewind_txg;
	uint64_t min_txg;

	...

	load_error = rewind_error = spa_load(spa, state, SPA_IMPORT_EXISTING);
	if (load_error == 0)
		return (0);
	...
}

static int
spa_load(spa_t *spa, spa_load_state_t state, spa_import_type_t type)
{
	const char *ereport = FM_EREPORT_ZFS_POOL;
	int error;

	spa->spa_load_state = state;
	(void) spa_import_progress_set_state(spa_guid(spa),
	    spa_load_state(spa));

	gethrestime(&spa->spa_loaded_ts);
	error = spa_load_impl(spa, type, &ereport);

	...
	(void) spa_import_progress_set_state(spa_guid(spa),
	    spa_load_state(spa));

	return (error);
}

static int spa_load_impl(spa_t *spa, spa_import_type_t type, const char **ereport)
{
	int error = 0;
	boolean_t missing_feat_write = B_FALSE;
	boolean_t checkpoint_rewind =
	    (spa->spa_import_flags & ZFS_IMPORT_CHECKPOINT);
	boolean_t update_config_cache = B_FALSE;

	...
	if (spa_writeable(spa) && (spa->spa_load_state == SPA_LOAD_RECOVER ||
	    spa->spa_load_max_txg == UINT64_MAX)) {
		uint64_t config_cache_txg = spa->spa_config_txg;

		...

		/*
		 * Traverse the ZIL and claim all blocks.
		 */
		spa_ld_claim_log_blocks(spa);

		/*
		 * Kick-off the syncing thread.
		 */
		spa->spa_sync_on = B_TRUE;
		txg_sync_start(spa->spa_dsl_pool);
		mmp_thread_start(spa);

		...
  }

	spa_load_note(spa, "LOADED");

	return (0);
}

void
txg_sync_start(dsl_pool_t *dp)
{
	...

	tx->tx_quiesce_thread = thread_create(NULL, 0, txg_quiesce_thread,
	    dp, 0, &p0, TS_RUN, defclsyspri);

	tx->tx_sync_thread = thread_create(NULL, 0, txg_sync_thread,
	    dp, 0, &p0, TS_RUN, defclsyspri);

	mutex_exit(&tx->tx_sync_lock);
}

static __attribute__((noreturn)) void
txg_sync_thread(void *arg)
{
	dsl_pool_t *dp = arg;
	spa_t *spa = dp->dp_spa;
	tx_state_t *tx = &dp->dp_tx;
	callb_cpr_t cpr;
	clock_t start, delta;

	(void) spl_fstrans_mark();
	txg_thread_enter(tx, &cpr);

	start = delta = 0;
	for (;;) {
		...

		txg_stat_t *ts = spa_txg_history_init_io(spa, txg, dp);
		start = ddi_get_lbolt();
		spa_sync(spa, txg);
		...
		txg_dispatch_callbacks(dp, txg);
	}
}

void
spa_sync(spa_t *spa, uint64_t txg)
{
	vdev_t *vd = NULL;

	VERIFY(spa_writeable(spa));

	...

	spa_sync_adjust_vdev_max_queue_depth(spa);

	spa_sync_condense_indirect(spa, tx);

	spa_sync_iterate_to_convergence(spa, tx);

  ...

	dsl_pool_sync_done(dp, txg);
	...
	spa->spa_ubsync = spa->spa_uberblock;
	spa_config_exit(spa, SCL_CONFIG, FTAG);

	spa_handle_ignored_writes(spa);

	spa_async_dispatch(spa);
}

static void
spa_sync_iterate_to_convergence(spa_t *spa, dmu_tx_t *tx)
{
  ...

	do {
		int pass = ++spa->spa_sync_pass;
    ...
		dsl_pool_sync(dp, txg);

		...
		spa_sync_deferred_frees(spa, tx);
	} while (dmu_objset_is_dirty(mos, txg));
}

void
dsl_pool_sync(dsl_pool_t *dp, uint64_t txg)
{
	...

	list_create(&synced_datasets, sizeof (dsl_dataset_t),
	    offsetof(dsl_dataset_t, ds_synced_link));

	tx = dmu_tx_create_assigned(dp, txg);
	...
	if (!txg_list_empty(&dp->dp_sync_tasks, txg)) {
		dsl_sync_task_t *dst;

		ASSERT3U(spa_sync_pass(dp->dp_spa), ==, 1);
		while ((dst =
		    txg_list_remove(&dp->dp_early_sync_tasks, txg)) != NULL) {
			ASSERT(dsl_early_sync_task_verify(dp, txg));
			dsl_sync_task_sync(dst, tx);
		}
	}

	dmu_tx_commit(tx);

	DTRACE_PROBE2(dsl_pool_sync__done, dsl_pool_t *dp, dp, uint64_t, txg);
}

void
dsl_sync_task_sync(dsl_sync_task_t *dst, dmu_tx_t *tx)
{
	...

	rrw_enter(&dp->dp_config_rwlock, RW_WRITER, FTAG);
	dst->dst_error = dst->dst_checkfunc(dst->dst_arg, tx);
	if (dst->dst_error == 0)
		dst->dst_syncfunc(dst->dst_arg, tx);
	rrw_exit(&dp->dp_config_rwlock, FTAG);
	if (dst->dst_nowaiter)
		kmem_free(dst, sizeof (*dst));
}


/* Stub implementation for dsl_dataset_hold */
void dsl_dataset_hold(void) {}



/* Stub implementation for dsl_dataset_hold_obj_string */
void dsl_dataset_hold_obj_string(void) {}
