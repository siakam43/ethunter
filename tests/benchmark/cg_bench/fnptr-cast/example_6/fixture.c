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

	if (tmpdp != NULL) {
		ddura.ddura_holdfunc = dsl_dataset_hold_obj_string;
		pool = spa_name(tmpdp->dp_spa);
	} else {
		ddura.ddura_holdfunc = dsl_dataset_hold;
		pool = nvpair_name(pair);
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
	return (0);
}

/* Stub implementation for dsl_dataset_hold */
void dsl_dataset_hold(void) {}

/* Stub implementation for dsl_dataset_hold_obj_string */
void dsl_dataset_hold_obj_string(void) {}
