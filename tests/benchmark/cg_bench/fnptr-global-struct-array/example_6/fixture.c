/* CG-Bench fixture: fnptr-global-struct-array/example_6 */
/* fnptr: command_table[i].func, targets: zfs_do_version, zfs_do_create, zfs_do_destroy, zfs_do_snapshot, zfs_do_rollback, zfs_do_clone, zfs_do_promote, zfs_do_rename, zfs_do_bookmark, zfs_do_channel_program, zfs_do_list, zfs_do_set, zfs_do_get, zfs_do_inherit, zfs_do_upgrade, zfs_do_userspace, zfs_do_project, zfs_do_mount, zfs_do_unmount, zfs_do_share, zfs_do_unshare, zfs_do_send, zfs_do_receive, zfs_do_allow, zfs_do_receive, zfs_do_allow, zfs_do_unallow, zfs_do_hold, zfs_do_holds, zfs_do_release, zfs_do_diff, zfs_do_load_key, zfs_do_unload_key, zfs_do_change_key, zfs_do_redact, zfs_do_wait, zfs_do_zone, zfs_do_unzone */

int
main(int argc, char **argv)
{
	...
	libzfs_mnttab_cache(g_zfs, B_TRUE);
	if (find_command_idx(cmdname, &i) == 0) {
		current_command = &command_table[i];
		ret = command_table[i].func(argc - 1, newargv + 1);
  }
}

static zfs_command_t command_table[] = {
	{ "version",	zfs_do_version, 	HELP_VERSION		},
	{ NULL },
	{ "create",	zfs_do_create,		HELP_CREATE		},
	{ "destroy",	zfs_do_destroy,		HELP_DESTROY		},
	{ NULL },
	{ "snapshot",	zfs_do_snapshot,	HELP_SNAPSHOT		},
	{ "rollback",	zfs_do_rollback,	HELP_ROLLBACK		},
	{ "clone",	zfs_do_clone,		HELP_CLONE		},
	{ "promote",	zfs_do_promote,		HELP_PROMOTE		},
	{ "rename",	zfs_do_rename,		HELP_RENAME		},
	{ "bookmark",	zfs_do_bookmark,	HELP_BOOKMARK		},
	{ "program",    zfs_do_channel_program, HELP_CHANNEL_PROGRAM    },
	{ NULL },
	{ "list",	zfs_do_list,		HELP_LIST		},
	{ NULL },
	{ "set",	zfs_do_set,		HELP_SET		},
	{ "get",	zfs_do_get,		HELP_GET		},
	{ "inherit",	zfs_do_inherit,		HELP_INHERIT		},
	{ "upgrade",	zfs_do_upgrade,		HELP_UPGRADE		},
	{ NULL },
	{ "userspace",	zfs_do_userspace,	HELP_USERSPACE		},
	{ "groupspace",	zfs_do_userspace,	HELP_GROUPSPACE		},
	{ "projectspace", zfs_do_userspace,	HELP_PROJECTSPACE	},
	{ NULL },
	{ "project",	zfs_do_project,		HELP_PROJECT		},
	{ NULL },
	{ "mount",	zfs_do_mount,		HELP_MOUNT		},
	{ "unmount",	zfs_do_unmount,		HELP_UNMOUNT		},
	{ "share",	zfs_do_share,		HELP_SHARE		},
	{ "unshare",	zfs_do_unshare,		HELP_UNSHARE		},
	{ NULL },
	{ "send",	zfs_do_send,		HELP_SEND		},
	{ "receive",	zfs_do_receive,		HELP_RECEIVE		},
	{ NULL },
	{ "allow",	zfs_do_allow,		HELP_ALLOW		},
	{ NULL },
	{ "unallow",	zfs_do_unallow,		HELP_UNALLOW		},
	{ NULL },
	{ "hold",	zfs_do_hold,		HELP_HOLD		},
	{ "holds",	zfs_do_holds,		HELP_HOLDS		},
	{ "release",	zfs_do_release,		HELP_RELEASE		},
	{ "diff",	zfs_do_diff,		HELP_DIFF		},
	{ "load-key",	zfs_do_load_key,	HELP_LOAD_KEY		},
	{ "unload-key",	zfs_do_unload_key,	HELP_UNLOAD_KEY		},
	{ "change-key",	zfs_do_change_key,	HELP_CHANGE_KEY		},
	{ "redact",	zfs_do_redact,		HELP_REDACT		},
	{ "wait",	zfs_do_wait,		HELP_WAIT		},

#ifdef __FreeBSD__
	{ "jail",	zfs_do_jail,		HELP_JAIL		},
	{ "unjail",	zfs_do_unjail,		HELP_UNJAIL		},
#endif

#ifdef __linux__
	{ "zone",	zfs_do_zone,		HELP_ZONE		},
	{ "unzone",	zfs_do_unzone,		HELP_UNZONE		},
#endif
};


/* Stub implementation for zfs_do_version */
void zfs_do_version(void) {}



/* Stub implementation for zfs_do_create */
void zfs_do_create(void) {}



/* Stub implementation for zfs_do_destroy */
void zfs_do_destroy(void) {}



/* Stub implementation for zfs_do_snapshot */
void zfs_do_snapshot(void) {}



/* Stub implementation for zfs_do_rollback */
void zfs_do_rollback(void) {}



/* Stub implementation for zfs_do_clone */
void zfs_do_clone(void) {}



/* Stub implementation for zfs_do_promote */
void zfs_do_promote(void) {}



/* Stub implementation for zfs_do_rename */
void zfs_do_rename(void) {}



/* Stub implementation for zfs_do_bookmark */
void zfs_do_bookmark(void) {}



/* Stub implementation for zfs_do_channel_program */
void zfs_do_channel_program(void) {}



/* Stub implementation for zfs_do_list */
void zfs_do_list(void) {}



/* Stub implementation for zfs_do_set */
void zfs_do_set(void) {}



/* Stub implementation for zfs_do_get */
void zfs_do_get(void) {}



/* Stub implementation for zfs_do_inherit */
void zfs_do_inherit(void) {}



/* Stub implementation for zfs_do_upgrade */
void zfs_do_upgrade(void) {}



/* Stub implementation for zfs_do_userspace */
void zfs_do_userspace(void) {}



/* Stub implementation for zfs_do_project */
void zfs_do_project(void) {}



/* Stub implementation for zfs_do_mount */
void zfs_do_mount(void) {}



/* Stub implementation for zfs_do_unmount */
void zfs_do_unmount(void) {}



/* Stub implementation for zfs_do_share */
void zfs_do_share(void) {}



/* Stub implementation for zfs_do_unshare */
void zfs_do_unshare(void) {}



/* Stub implementation for zfs_do_send */
void zfs_do_send(void) {}



/* Stub implementation for zfs_do_receive */
void zfs_do_receive(void) {}



/* Stub implementation for zfs_do_allow */
void zfs_do_allow(void) {}



/* Stub implementation for zfs_do_receive */
void zfs_do_receive(void) {}



/* Stub implementation for zfs_do_allow */
void zfs_do_allow(void) {}



/* Stub implementation for zfs_do_unallow */
void zfs_do_unallow(void) {}



/* Stub implementation for zfs_do_hold */
void zfs_do_hold(void) {}



/* Stub implementation for zfs_do_holds */
void zfs_do_holds(void) {}



/* Stub implementation for zfs_do_release */
void zfs_do_release(void) {}



/* Stub implementation for zfs_do_diff */
void zfs_do_diff(void) {}



/* Stub implementation for zfs_do_load_key */
void zfs_do_load_key(void) {}



/* Stub implementation for zfs_do_unload_key */
void zfs_do_unload_key(void) {}



/* Stub implementation for zfs_do_change_key */
void zfs_do_change_key(void) {}



/* Stub implementation for zfs_do_redact */
void zfs_do_redact(void) {}



/* Stub implementation for zfs_do_wait */
void zfs_do_wait(void) {}



/* Stub implementation for zfs_do_zone */
void zfs_do_zone(void) {}



/* Stub implementation for zfs_do_unzone */
void zfs_do_unzone(void) {}
