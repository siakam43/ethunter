/* CG-Bench fixture: fnptr-global-struct/example_4 */
/* fnptr: vec->zvec_legacy_func, targets: zfs_ioc_clear, zfs_ioc_clear_fault, zfs_ioc_dataset_list_next, zfs_ioc_destroy, zfs_ioc_diff, zfs_ioc_dsobj_to_dsname, zfs_ioc_error_log, zfs_ioc_events_clear, zfs_ioc_events_next, zfs_ioc_events_seek, zfs_ioc_get_fsacl, zfs_ioc_inherit_prop, zfs_ioc_inject_fault, zfs_ioc_inject_list_next, zfs_ioc_next_obj, zfs_ioc_obj_to_path, zfs_ioc_obj_to_stats, zfs_ioc_objset_recvd_props, zfs_ioc_objset_stats, zfs_ioc_objset_zplprops, zfs_ioc_pool_configs, zfs_ioc_pool_create, zfs_ioc_pool_destroy, zfs_ioc_pool_export, zfs_ioc_pool_freeze, zfs_ioc_pool_get_history, zfs_ioc_pool_get_props, zfs_ioc_pool_import, zfs_ioc_pool_reguid, zfs_ioc_pool_scan, zfs_ioc_pool_set_props, zfs_ioc_pool_stats, zfs_ioc_pool_tryimport, zfs_ioc_pool_upgrade, zfs_ioc_promote, zfs_ioc_recv, zfs_ioc_rename, zfs_ioc_send, zfs_ioc_send_progress, zfs_ioc_set_fsacl, zfs_ioc_set_prop, zfs_ioc_share, zfs_ioc_smb_acl, zfs_ioc_snapshot_list_next, zfs_ioc_space_written, zfs_ioc_tmp_snapshot, zfs_ioc_userspace_many, zfs_ioc_userspace_one, zfs_ioc_userspace_upgrade, zfs_ioc_vdev_add, zfs_ioc_vdev_attach, zfs_ioc_vdev_detach, zfs_ioc_vdev_remove, zfs_ioc_vdev_set_state, zfs_ioc_vdev_setfru, zfs_ioc_vdev_setpath, zfs_ioc_vdev_split */

long zfsdev_ioctl_common(uint_t vecnum, zfs_cmd_t *zc, int flag)
{
	...
	if (vec->zvec_func != NULL) {
		....

		if ((error == 0 ||
		    (cmd == ZFS_IOC_CHANNEL_PROGRAM && error != EINVAL)) &&
		    vec->zvec_allow_log &&
		    spa_open(zc->zc_name, &spa, FTAG) == 0) {
			...
	} else {
		cookie = spl_fstrans_mark();
		error = vec->zvec_legacy_func(zc);
		spl_fstrans_unmark(cookie);
	}
    ...
    }
}

static void zfs_ioctl_register_legacy(zfs_ioc_t ioc, zfs_ioc_legacy_func_t *func,
    zfs_secpolicy_func_t *secpolicy, zfs_ioc_namecheck_t namecheck,
    boolean_t log_history, zfs_ioc_poolcheck_t pool_check)
{
	zfs_ioc_vec_t *vec = &zfs_ioc_vec[ioc - ZFS_IOC_FIRST];

	...

	vec->zvec_legacy_func = func;
	vec->zvec_secpolicy = secpolicy;
	vec->zvec_namecheck = namecheck;
	vec->zvec_allow_log = log_history;
	vec->zvec_pool_check = pool_check;
}

static void
zfs_ioctl_register_pool(zfs_ioc_t ioc, zfs_ioc_legacy_func_t *func,
    zfs_secpolicy_func_t *secpolicy, boolean_t log_history,
    zfs_ioc_poolcheck_t pool_check)
{
	zfs_ioctl_register_legacy(ioc, func, secpolicy,
	    POOL_NAME, log_history, pool_check);
}

void
zfs_ioctl_register_dataset_nolog(zfs_ioc_t ioc, zfs_ioc_legacy_func_t *func,
    zfs_secpolicy_func_t *secpolicy, zfs_ioc_poolcheck_t pool_check)
{
	zfs_ioctl_register_legacy(ioc, func, secpolicy,
	    DATASET_NAME, B_FALSE, pool_check);
}

static void
zfs_ioctl_register_pool_modify(zfs_ioc_t ioc, zfs_ioc_legacy_func_t *func)
{
	zfs_ioctl_register_legacy(ioc, func, zfs_secpolicy_config,
	    POOL_NAME, B_TRUE, POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY);
}

static void
zfs_ioctl_register_pool_meta(zfs_ioc_t ioc, zfs_ioc_legacy_func_t *func,
    zfs_secpolicy_func_t *secpolicy)
{
	zfs_ioctl_register_legacy(ioc, func, secpolicy,
	    NO_NAME, B_FALSE, POOL_CHECK_NONE);
}

static void
zfs_ioctl_register_dataset_read_secpolicy(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func, zfs_secpolicy_func_t *secpolicy)
{
	zfs_ioctl_register_legacy(ioc, func, secpolicy,
	    DATASET_NAME, B_FALSE, POOL_CHECK_SUSPENDED);
}

static void
zfs_ioctl_register_dataset_read(zfs_ioc_t ioc, zfs_ioc_legacy_func_t *func)
{
	zfs_ioctl_register_dataset_read_secpolicy(ioc, func,
	    zfs_secpolicy_read);
}

static void
zfs_ioctl_register_dataset_modify(zfs_ioc_t ioc, zfs_ioc_legacy_func_t *func,
    zfs_secpolicy_func_t *secpolicy)
{
	zfs_ioctl_register_legacy(ioc, func, secpolicy,
	    DATASET_NAME, B_TRUE, POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY);
}

static void
zfs_ioctl_init(void)
{
	zfs_ioctl_register("snapshot", ZFS_IOC_SNAPSHOT,
	    zfs_ioc_snapshot, zfs_secpolicy_snapshot, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_snapshot, ARRAY_SIZE(zfs_keys_snapshot));

	zfs_ioctl_register("log_history", ZFS_IOC_LOG_HISTORY,
	    zfs_ioc_log_history, zfs_secpolicy_log_history, NO_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_FALSE, B_FALSE,
	    zfs_keys_log_history, ARRAY_SIZE(zfs_keys_log_history));

	zfs_ioctl_register("space_snaps", ZFS_IOC_SPACE_SNAPS,
	    zfs_ioc_space_snaps, zfs_secpolicy_read, DATASET_NAME,
	    POOL_CHECK_SUSPENDED, B_FALSE, B_FALSE,
	    zfs_keys_space_snaps, ARRAY_SIZE(zfs_keys_space_snaps));

	zfs_ioctl_register("send", ZFS_IOC_SEND_NEW,
	    zfs_ioc_send_new, zfs_secpolicy_send_new, DATASET_NAME,
	    POOL_CHECK_SUSPENDED, B_FALSE, B_FALSE,
	    zfs_keys_send_new, ARRAY_SIZE(zfs_keys_send_new));

	zfs_ioctl_register("send_space", ZFS_IOC_SEND_SPACE,
	    zfs_ioc_send_space, zfs_secpolicy_read, DATASET_NAME,
	    POOL_CHECK_SUSPENDED, B_FALSE, B_FALSE,
	    zfs_keys_send_space, ARRAY_SIZE(zfs_keys_send_space));

	zfs_ioctl_register("create", ZFS_IOC_CREATE,
	    zfs_ioc_create, zfs_secpolicy_create_clone, DATASET_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_create, ARRAY_SIZE(zfs_keys_create));

	zfs_ioctl_register("clone", ZFS_IOC_CLONE,
	    zfs_ioc_clone, zfs_secpolicy_create_clone, DATASET_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_clone, ARRAY_SIZE(zfs_keys_clone));

	zfs_ioctl_register("remap", ZFS_IOC_REMAP,
	    zfs_ioc_remap, zfs_secpolicy_none, DATASET_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_FALSE, B_TRUE,
	    zfs_keys_remap, ARRAY_SIZE(zfs_keys_remap));

	zfs_ioctl_register("destroy_snaps", ZFS_IOC_DESTROY_SNAPS,
	    zfs_ioc_destroy_snaps, zfs_secpolicy_destroy_snaps, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_destroy_snaps, ARRAY_SIZE(zfs_keys_destroy_snaps));

	zfs_ioctl_register("hold", ZFS_IOC_HOLD,
	    zfs_ioc_hold, zfs_secpolicy_hold, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_hold, ARRAY_SIZE(zfs_keys_hold));
	zfs_ioctl_register("release", ZFS_IOC_RELEASE,
	    zfs_ioc_release, zfs_secpolicy_release, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_release, ARRAY_SIZE(zfs_keys_release));

	zfs_ioctl_register("get_holds", ZFS_IOC_GET_HOLDS,
	    zfs_ioc_get_holds, zfs_secpolicy_read, DATASET_NAME,
	    POOL_CHECK_SUSPENDED, B_FALSE, B_FALSE,
	    zfs_keys_get_holds, ARRAY_SIZE(zfs_keys_get_holds));

	zfs_ioctl_register("rollback", ZFS_IOC_ROLLBACK,
	    zfs_ioc_rollback, zfs_secpolicy_rollback, DATASET_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_FALSE, B_TRUE,
	    zfs_keys_rollback, ARRAY_SIZE(zfs_keys_rollback));

	zfs_ioctl_register("bookmark", ZFS_IOC_BOOKMARK,
	    zfs_ioc_bookmark, zfs_secpolicy_bookmark, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_bookmark, ARRAY_SIZE(zfs_keys_bookmark));

	zfs_ioctl_register("get_bookmarks", ZFS_IOC_GET_BOOKMARKS,
	    zfs_ioc_get_bookmarks, zfs_secpolicy_read, DATASET_NAME,
	    POOL_CHECK_SUSPENDED, B_FALSE, B_FALSE,
	    zfs_keys_get_bookmarks, ARRAY_SIZE(zfs_keys_get_bookmarks));

	zfs_ioctl_register("get_bookmark_props", ZFS_IOC_GET_BOOKMARK_PROPS,
	    zfs_ioc_get_bookmark_props, zfs_secpolicy_read, ENTITY_NAME,
	    POOL_CHECK_SUSPENDED, B_FALSE, B_FALSE, zfs_keys_get_bookmark_props,
	    ARRAY_SIZE(zfs_keys_get_bookmark_props));

	zfs_ioctl_register("destroy_bookmarks", ZFS_IOC_DESTROY_BOOKMARKS,
	    zfs_ioc_destroy_bookmarks, zfs_secpolicy_destroy_bookmarks,
	    POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_destroy_bookmarks,
	    ARRAY_SIZE(zfs_keys_destroy_bookmarks));

	zfs_ioctl_register("receive", ZFS_IOC_RECV_NEW,
	    zfs_ioc_recv_new, zfs_secpolicy_recv, DATASET_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_recv_new, ARRAY_SIZE(zfs_keys_recv_new));
	zfs_ioctl_register("load-key", ZFS_IOC_LOAD_KEY,
	    zfs_ioc_load_key, zfs_secpolicy_load_key,
	    DATASET_NAME, POOL_CHECK_SUSPENDED, B_TRUE, B_TRUE,
	    zfs_keys_load_key, ARRAY_SIZE(zfs_keys_load_key));
	zfs_ioctl_register("unload-key", ZFS_IOC_UNLOAD_KEY,
	    zfs_ioc_unload_key, zfs_secpolicy_load_key,
	    DATASET_NAME, POOL_CHECK_SUSPENDED, B_TRUE, B_TRUE,
	    zfs_keys_unload_key, ARRAY_SIZE(zfs_keys_unload_key));
	zfs_ioctl_register("change-key", ZFS_IOC_CHANGE_KEY,
	    zfs_ioc_change_key, zfs_secpolicy_change_key,
	    DATASET_NAME, POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY,
	    B_TRUE, B_TRUE, zfs_keys_change_key,
	    ARRAY_SIZE(zfs_keys_change_key));

	zfs_ioctl_register("sync", ZFS_IOC_POOL_SYNC,
	    zfs_ioc_pool_sync, zfs_secpolicy_none, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_FALSE, B_FALSE,
	    zfs_keys_pool_sync, ARRAY_SIZE(zfs_keys_pool_sync));
	zfs_ioctl_register("reopen", ZFS_IOC_POOL_REOPEN, zfs_ioc_pool_reopen,
	    zfs_secpolicy_config, POOL_NAME, POOL_CHECK_SUSPENDED, B_TRUE,
	    B_TRUE, zfs_keys_pool_reopen, ARRAY_SIZE(zfs_keys_pool_reopen));

	zfs_ioctl_register("channel_program", ZFS_IOC_CHANNEL_PROGRAM,
	    zfs_ioc_channel_program, zfs_secpolicy_config,
	    POOL_NAME, POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE,
	    B_TRUE, zfs_keys_channel_program,
	    ARRAY_SIZE(zfs_keys_channel_program));

	zfs_ioctl_register("redact", ZFS_IOC_REDACT,
	    zfs_ioc_redact, zfs_secpolicy_config, DATASET_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_redact, ARRAY_SIZE(zfs_keys_redact));

	zfs_ioctl_register("zpool_checkpoint", ZFS_IOC_POOL_CHECKPOINT,
	    zfs_ioc_pool_checkpoint, zfs_secpolicy_config, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_pool_checkpoint, ARRAY_SIZE(zfs_keys_pool_checkpoint));

	zfs_ioctl_register("zpool_discard_checkpoint",
	    ZFS_IOC_POOL_DISCARD_CHECKPOINT, zfs_ioc_pool_discard_checkpoint,
	    zfs_secpolicy_config, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_pool_discard_checkpoint,
	    ARRAY_SIZE(zfs_keys_pool_discard_checkpoint));

	zfs_ioctl_register("initialize", ZFS_IOC_POOL_INITIALIZE,
	    zfs_ioc_pool_initialize, zfs_secpolicy_config, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_pool_initialize, ARRAY_SIZE(zfs_keys_pool_initialize));

	zfs_ioctl_register("trim", ZFS_IOC_POOL_TRIM,
	    zfs_ioc_pool_trim, zfs_secpolicy_config, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_TRUE, B_TRUE,
	    zfs_keys_pool_trim, ARRAY_SIZE(zfs_keys_pool_trim));

	zfs_ioctl_register("wait", ZFS_IOC_WAIT,
	    zfs_ioc_wait, zfs_secpolicy_none, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_FALSE, B_FALSE,
	    zfs_keys_pool_wait, ARRAY_SIZE(zfs_keys_pool_wait));

	zfs_ioctl_register("wait_fs", ZFS_IOC_WAIT_FS,
	    zfs_ioc_wait_fs, zfs_secpolicy_none, DATASET_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_FALSE, B_FALSE,
	    zfs_keys_fs_wait, ARRAY_SIZE(zfs_keys_fs_wait));

	zfs_ioctl_register("set_bootenv", ZFS_IOC_SET_BOOTENV,
	    zfs_ioc_set_bootenv, zfs_secpolicy_config, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_FALSE, B_TRUE,
	    zfs_keys_set_bootenv, ARRAY_SIZE(zfs_keys_set_bootenv));

	zfs_ioctl_register("get_bootenv", ZFS_IOC_GET_BOOTENV,
	    zfs_ioc_get_bootenv, zfs_secpolicy_none, POOL_NAME,
	    POOL_CHECK_SUSPENDED, B_FALSE, B_TRUE,
	    zfs_keys_get_bootenv, ARRAY_SIZE(zfs_keys_get_bootenv));

	zfs_ioctl_register("zpool_vdev_get_props", ZFS_IOC_VDEV_GET_PROPS,
	    zfs_ioc_vdev_get_props, zfs_secpolicy_read, POOL_NAME,
	    POOL_CHECK_NONE, B_FALSE, B_FALSE, zfs_keys_vdev_get_props,
	    ARRAY_SIZE(zfs_keys_vdev_get_props));

	zfs_ioctl_register("zpool_vdev_set_props", ZFS_IOC_VDEV_SET_PROPS,
	    zfs_ioc_vdev_set_props, zfs_secpolicy_config, POOL_NAME,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY, B_FALSE, B_FALSE,
	    zfs_keys_vdev_set_props, ARRAY_SIZE(zfs_keys_vdev_set_props));

	zfs_ioctl_register("scrub", ZFS_IOC_POOL_SCRUB,
	    zfs_ioc_pool_scrub, zfs_secpolicy_config, POOL_NAME,
	    POOL_CHECK_NONE, B_TRUE, B_TRUE,
	    zfs_keys_pool_scrub, ARRAY_SIZE(zfs_keys_pool_scrub));

	/* IOCTLS that use the legacy function signature */

	zfs_ioctl_register_legacy(ZFS_IOC_POOL_FREEZE, zfs_ioc_pool_freeze,
	    zfs_secpolicy_config, NO_NAME, B_FALSE, POOL_CHECK_READONLY);

	zfs_ioctl_register_pool(ZFS_IOC_POOL_CREATE, zfs_ioc_pool_create,
	    zfs_secpolicy_config, B_TRUE, POOL_CHECK_NONE);
	zfs_ioctl_register_pool_modify(ZFS_IOC_POOL_SCAN,
	    zfs_ioc_pool_scan);
	zfs_ioctl_register_pool_modify(ZFS_IOC_POOL_UPGRADE,
	    zfs_ioc_pool_upgrade);
	zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_ADD,
	    zfs_ioc_vdev_add);
	zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_REMOVE,
	    zfs_ioc_vdev_remove);
	zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_SET_STATE,
	    zfs_ioc_vdev_set_state);
	zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_ATTACH,
	    zfs_ioc_vdev_attach);
	zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_DETACH,
	    zfs_ioc_vdev_detach);
	zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_SETPATH,
	    zfs_ioc_vdev_setpath);
	zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_SETFRU,
	    zfs_ioc_vdev_setfru);
	zfs_ioctl_register_pool_modify(ZFS_IOC_POOL_SET_PROPS,
	    zfs_ioc_pool_set_props);
	zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_SPLIT,
	    zfs_ioc_vdev_split);
	zfs_ioctl_register_pool_modify(ZFS_IOC_POOL_REGUID,
	    zfs_ioc_pool_reguid);

	zfs_ioctl_register_pool_meta(ZFS_IOC_POOL_CONFIGS,
	    zfs_ioc_pool_configs, zfs_secpolicy_none);
	zfs_ioctl_register_pool_meta(ZFS_IOC_POOL_TRYIMPORT,
	    zfs_ioc_pool_tryimport, zfs_secpolicy_config);
	zfs_ioctl_register_pool_meta(ZFS_IOC_INJECT_FAULT,
	    zfs_ioc_inject_fault, zfs_secpolicy_inject);
	zfs_ioctl_register_pool_meta(ZFS_IOC_CLEAR_FAULT,
	    zfs_ioc_clear_fault, zfs_secpolicy_inject);
	zfs_ioctl_register_pool_meta(ZFS_IOC_INJECT_LIST_NEXT,
	    zfs_ioc_inject_list_next, zfs_secpolicy_inject);

	/*
	 * pool destroy, and export don't log the history as part of
	 * zfsdev_ioctl, but rather zfs_ioc_pool_export
	 * does the logging of those commands.
	 */
	zfs_ioctl_register_pool(ZFS_IOC_POOL_DESTROY, zfs_ioc_pool_destroy,
	    zfs_secpolicy_config, B_FALSE, POOL_CHECK_SUSPENDED);
	zfs_ioctl_register_pool(ZFS_IOC_POOL_EXPORT, zfs_ioc_pool_export,
	    zfs_secpolicy_config, B_FALSE, POOL_CHECK_SUSPENDED);

	zfs_ioctl_register_pool(ZFS_IOC_POOL_STATS, zfs_ioc_pool_stats,
	    zfs_secpolicy_read, B_FALSE, POOL_CHECK_NONE);
	zfs_ioctl_register_pool(ZFS_IOC_POOL_GET_PROPS, zfs_ioc_pool_get_props,
	    zfs_secpolicy_read, B_FALSE, POOL_CHECK_NONE);

	zfs_ioctl_register_pool(ZFS_IOC_ERROR_LOG, zfs_ioc_error_log,
	    zfs_secpolicy_inject, B_FALSE, POOL_CHECK_SUSPENDED);
	zfs_ioctl_register_pool(ZFS_IOC_DSOBJ_TO_DSNAME,
	    zfs_ioc_dsobj_to_dsname,
	    zfs_secpolicy_diff, B_FALSE, POOL_CHECK_SUSPENDED);
	zfs_ioctl_register_pool(ZFS_IOC_POOL_GET_HISTORY,
	    zfs_ioc_pool_get_history,
	    zfs_secpolicy_config, B_FALSE, POOL_CHECK_SUSPENDED);

	zfs_ioctl_register_pool(ZFS_IOC_POOL_IMPORT, zfs_ioc_pool_import,
	    zfs_secpolicy_config, B_TRUE, POOL_CHECK_NONE);

	zfs_ioctl_register_pool(ZFS_IOC_CLEAR, zfs_ioc_clear,
	    zfs_secpolicy_config, B_TRUE, POOL_CHECK_READONLY);

	zfs_ioctl_register_dataset_read(ZFS_IOC_SPACE_WRITTEN,
	    zfs_ioc_space_written);
	zfs_ioctl_register_dataset_read(ZFS_IOC_OBJSET_RECVD_PROPS,
	    zfs_ioc_objset_recvd_props);
	zfs_ioctl_register_dataset_read(ZFS_IOC_NEXT_OBJ,
	    zfs_ioc_next_obj);
	zfs_ioctl_register_dataset_read(ZFS_IOC_GET_FSACL,
	    zfs_ioc_get_fsacl);
	zfs_ioctl_register_dataset_read(ZFS_IOC_OBJSET_STATS,
	    zfs_ioc_objset_stats);
	zfs_ioctl_register_dataset_read(ZFS_IOC_OBJSET_ZPLPROPS,
	    zfs_ioc_objset_zplprops);
	zfs_ioctl_register_dataset_read(ZFS_IOC_DATASET_LIST_NEXT,
	    zfs_ioc_dataset_list_next);
	zfs_ioctl_register_dataset_read(ZFS_IOC_SNAPSHOT_LIST_NEXT,
	    zfs_ioc_snapshot_list_next);
	zfs_ioctl_register_dataset_read(ZFS_IOC_SEND_PROGRESS,
	    zfs_ioc_send_progress);

	zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_DIFF,
	    zfs_ioc_diff, zfs_secpolicy_diff);
	zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_OBJ_TO_STATS,
	    zfs_ioc_obj_to_stats, zfs_secpolicy_diff);
	zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_OBJ_TO_PATH,
	    zfs_ioc_obj_to_path, zfs_secpolicy_diff);
	zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_USERSPACE_ONE,
	    zfs_ioc_userspace_one, zfs_secpolicy_userspace_one);
	zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_USERSPACE_MANY,
	    zfs_ioc_userspace_many, zfs_secpolicy_userspace_many);
	zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_SEND,
	    zfs_ioc_send, zfs_secpolicy_send);

	zfs_ioctl_register_dataset_modify(ZFS_IOC_SET_PROP, zfs_ioc_set_prop,
	    zfs_secpolicy_none);
	zfs_ioctl_register_dataset_modify(ZFS_IOC_DESTROY, zfs_ioc_destroy,
	    zfs_secpolicy_destroy);
	zfs_ioctl_register_dataset_modify(ZFS_IOC_RENAME, zfs_ioc_rename,
	    zfs_secpolicy_rename);
	zfs_ioctl_register_dataset_modify(ZFS_IOC_RECV, zfs_ioc_recv,
	    zfs_secpolicy_recv);
	zfs_ioctl_register_dataset_modify(ZFS_IOC_PROMOTE, zfs_ioc_promote,
	    zfs_secpolicy_promote);
	zfs_ioctl_register_dataset_modify(ZFS_IOC_INHERIT_PROP,
	    zfs_ioc_inherit_prop, zfs_secpolicy_inherit_prop);
	zfs_ioctl_register_dataset_modify(ZFS_IOC_SET_FSACL, zfs_ioc_set_fsacl,
	    zfs_secpolicy_set_fsacl);

	zfs_ioctl_register_dataset_nolog(ZFS_IOC_SHARE, zfs_ioc_share,
	    zfs_secpolicy_share, POOL_CHECK_NONE);
	zfs_ioctl_register_dataset_nolog(ZFS_IOC_SMB_ACL, zfs_ioc_smb_acl,
	    zfs_secpolicy_smb_acl, POOL_CHECK_NONE);
	zfs_ioctl_register_dataset_nolog(ZFS_IOC_USERSPACE_UPGRADE,
	    zfs_ioc_userspace_upgrade, zfs_secpolicy_userspace_upgrade,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY);
	zfs_ioctl_register_dataset_nolog(ZFS_IOC_TMP_SNAPSHOT,
	    zfs_ioc_tmp_snapshot, zfs_secpolicy_tmp_snapshot,
	    POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY);

	zfs_ioctl_register_legacy(ZFS_IOC_EVENTS_NEXT, zfs_ioc_events_next,
	    zfs_secpolicy_config, NO_NAME, B_FALSE, POOL_CHECK_NONE);
	zfs_ioctl_register_legacy(ZFS_IOC_EVENTS_CLEAR, zfs_ioc_events_clear,
	    zfs_secpolicy_config, NO_NAME, B_FALSE, POOL_CHECK_NONE);
	zfs_ioctl_register_legacy(ZFS_IOC_EVENTS_SEEK, zfs_ioc_events_seek,
	    zfs_secpolicy_config, NO_NAME, B_FALSE, POOL_CHECK_NONE);

	zfs_ioctl_init_os();
}


/* Stub implementation for zfs_ioc_clear */
void zfs_ioc_clear(void) {}



/* Stub implementation for zfs_ioc_clear_fault */
void zfs_ioc_clear_fault(void) {}



/* Stub implementation for zfs_ioc_dataset_list_next */
void zfs_ioc_dataset_list_next(void) {}



/* Stub implementation for zfs_ioc_destroy */
void zfs_ioc_destroy(void) {}



/* Stub implementation for zfs_ioc_diff */
void zfs_ioc_diff(void) {}



/* Stub implementation for zfs_ioc_dsobj_to_dsname */
void zfs_ioc_dsobj_to_dsname(void) {}



/* Stub implementation for zfs_ioc_error_log */
void zfs_ioc_error_log(void) {}



/* Stub implementation for zfs_ioc_events_clear */
void zfs_ioc_events_clear(void) {}



/* Stub implementation for zfs_ioc_events_next */
void zfs_ioc_events_next(void) {}



/* Stub implementation for zfs_ioc_events_seek */
void zfs_ioc_events_seek(void) {}



/* Stub implementation for zfs_ioc_get_fsacl */
void zfs_ioc_get_fsacl(void) {}



/* Stub implementation for zfs_ioc_inherit_prop */
void zfs_ioc_inherit_prop(void) {}



/* Stub implementation for zfs_ioc_inject_fault */
void zfs_ioc_inject_fault(void) {}



/* Stub implementation for zfs_ioc_inject_list_next */
void zfs_ioc_inject_list_next(void) {}



/* Stub implementation for zfs_ioc_next_obj */
void zfs_ioc_next_obj(void) {}



/* Stub implementation for zfs_ioc_obj_to_path */
void zfs_ioc_obj_to_path(void) {}



/* Stub implementation for zfs_ioc_obj_to_stats */
void zfs_ioc_obj_to_stats(void) {}



/* Stub implementation for zfs_ioc_objset_recvd_props */
void zfs_ioc_objset_recvd_props(void) {}



/* Stub implementation for zfs_ioc_objset_stats */
void zfs_ioc_objset_stats(void) {}



/* Stub implementation for zfs_ioc_objset_zplprops */
void zfs_ioc_objset_zplprops(void) {}



/* Stub implementation for zfs_ioc_pool_configs */
void zfs_ioc_pool_configs(void) {}



/* Stub implementation for zfs_ioc_pool_create */
void zfs_ioc_pool_create(void) {}



/* Stub implementation for zfs_ioc_pool_destroy */
void zfs_ioc_pool_destroy(void) {}



/* Stub implementation for zfs_ioc_pool_export */
void zfs_ioc_pool_export(void) {}



/* Stub implementation for zfs_ioc_pool_freeze */
void zfs_ioc_pool_freeze(void) {}



/* Stub implementation for zfs_ioc_pool_get_history */
void zfs_ioc_pool_get_history(void) {}



/* Stub implementation for zfs_ioc_pool_get_props */
void zfs_ioc_pool_get_props(void) {}



/* Stub implementation for zfs_ioc_pool_import */
void zfs_ioc_pool_import(void) {}



/* Stub implementation for zfs_ioc_pool_reguid */
void zfs_ioc_pool_reguid(void) {}



/* Stub implementation for zfs_ioc_pool_scan */
void zfs_ioc_pool_scan(void) {}



/* Stub implementation for zfs_ioc_pool_set_props */
void zfs_ioc_pool_set_props(void) {}



/* Stub implementation for zfs_ioc_pool_stats */
void zfs_ioc_pool_stats(void) {}



/* Stub implementation for zfs_ioc_pool_tryimport */
void zfs_ioc_pool_tryimport(void) {}



/* Stub implementation for zfs_ioc_pool_upgrade */
void zfs_ioc_pool_upgrade(void) {}



/* Stub implementation for zfs_ioc_promote */
void zfs_ioc_promote(void) {}



/* Stub implementation for zfs_ioc_recv */
void zfs_ioc_recv(void) {}



/* Stub implementation for zfs_ioc_rename */
void zfs_ioc_rename(void) {}



/* Stub implementation for zfs_ioc_send */
void zfs_ioc_send(void) {}



/* Stub implementation for zfs_ioc_send_progress */
void zfs_ioc_send_progress(void) {}



/* Stub implementation for zfs_ioc_set_fsacl */
void zfs_ioc_set_fsacl(void) {}



/* Stub implementation for zfs_ioc_set_prop */
void zfs_ioc_set_prop(void) {}



/* Stub implementation for zfs_ioc_share */
void zfs_ioc_share(void) {}



/* Stub implementation for zfs_ioc_smb_acl */
void zfs_ioc_smb_acl(void) {}



/* Stub implementation for zfs_ioc_snapshot_list_next */
void zfs_ioc_snapshot_list_next(void) {}



/* Stub implementation for zfs_ioc_space_written */
void zfs_ioc_space_written(void) {}



/* Stub implementation for zfs_ioc_tmp_snapshot */
void zfs_ioc_tmp_snapshot(void) {}



/* Stub implementation for zfs_ioc_userspace_many */
void zfs_ioc_userspace_many(void) {}



/* Stub implementation for zfs_ioc_userspace_one */
void zfs_ioc_userspace_one(void) {}



/* Stub implementation for zfs_ioc_userspace_upgrade */
void zfs_ioc_userspace_upgrade(void) {}



/* Stub implementation for zfs_ioc_vdev_add */
void zfs_ioc_vdev_add(void) {}



/* Stub implementation for zfs_ioc_vdev_attach */
void zfs_ioc_vdev_attach(void) {}



/* Stub implementation for zfs_ioc_vdev_detach */
void zfs_ioc_vdev_detach(void) {}



/* Stub implementation for zfs_ioc_vdev_remove */
void zfs_ioc_vdev_remove(void) {}



/* Stub implementation for zfs_ioc_vdev_set_state */
void zfs_ioc_vdev_set_state(void) {}



/* Stub implementation for zfs_ioc_vdev_setfru */
void zfs_ioc_vdev_setfru(void) {}



/* Stub implementation for zfs_ioc_vdev_setpath */
void zfs_ioc_vdev_setpath(void) {}



/* Stub implementation for zfs_ioc_vdev_split */
void zfs_ioc_vdev_split(void) {}
