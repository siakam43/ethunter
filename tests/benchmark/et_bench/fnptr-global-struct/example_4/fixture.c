/* et_bench fixture: fnptr-global-struct/example_4 */
/* fnptr: vec->zvec_legacy_func, targets: all zfs_ioc_* legacy handlers */

#include <stdint.h>

typedef int boolean_t;
typedef uint64_t zfs_ioc_t;
#define B_TRUE  1
#define B_FALSE 0
#define POOL_NAME       1
#define DATASET_NAME    2
#define NO_NAME         3
#define POOL_CHECK_NONE         0
#define POOL_CHECK_SUSPENDED    1
#define POOL_CHECK_READONLY     2
#define ZFS_IOC_FIRST           0
#define ZFS_IOC_CHANNEL_PROGRAM 42

typedef struct zfs_cmd zfs_cmd_t;
struct zfs_cmd { char zc_name[256]; };

typedef int (zfs_ioc_legacy_func_t)(zfs_cmd_t *);
typedef int (zfs_secpolicy_func_t)(zfs_cmd_t *);
typedef enum {
    POOL_CHECK_OK
} zfs_ioc_poolcheck_t;
typedef enum {
    NAMECHECK_NONE
} zfs_ioc_namecheck_t;

typedef struct zfs_ioc_vec {
    zfs_ioc_legacy_func_t *zvec_legacy_func;
    zfs_secpolicy_func_t *zvec_secpolicy;
    zfs_ioc_namecheck_t zvec_namecheck;
    boolean_t zvec_allow_log;
    zfs_ioc_poolcheck_t zvec_pool_check;
} zfs_ioc_vec_t;

#define MAX_IOC 256
static zfs_ioc_vec_t zfs_ioc_vec[MAX_IOC];

typedef enum { ZFS_OK, ZFS_ERR } error_t;

int spa_open(const char *name, void **spa, const char *tag) { return 0; }
int spl_fstrans_mark(void) { return 0; }
void spl_fstrans_unmark(int cookie) {}

long zfsdev_ioctl_common(uint_t vecnum, zfs_cmd_t *zc, int flag)
{
    zfs_ioc_vec_t *vec = &zfs_ioc_vec[vecnum];
    int error = ZFS_OK;
    void *spa = NULL;
    int cookie;
    uint_t cmd = vecnum;

    if (vec->zvec_legacy_func != NULL) {
        if ((error == ZFS_OK ||
            (cmd == ZFS_IOC_CHANNEL_PROGRAM && error != ZFS_ERR)) &&
            vec->zvec_allow_log &&
            spa_open(zc->zc_name, &spa, "ftag") == 0) {
            /* logged path */
        }
    } else {
        cookie = spl_fstrans_mark();
        error = vec->zvec_legacy_func(zc);
        spl_fstrans_unmark(cookie);
    }
    return error;
}

/* Legacy targets — called via zvec_legacy_func fnptr */
int zfs_ioc_clear(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_clear_fault(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_dataset_list_next(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_destroy(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_diff(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_dsobj_to_dsname(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_error_log(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_events_clear(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_events_next(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_events_seek(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_get_fsacl(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_inherit_prop(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_inject_fault(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_inject_list_next(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_next_obj(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_obj_to_path(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_obj_to_stats(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_objset_recvd_props(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_objset_stats(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_objset_zplprops(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_configs(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_create(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_destroy(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_export(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_freeze(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_get_history(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_get_props(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_import(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_reguid(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_scan(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_set_props(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_stats(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_tryimport(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_pool_upgrade(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_promote(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_recv(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_rename(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_send(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_send_progress(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_set_fsacl(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_set_prop(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_share(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_smb_acl(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_snapshot_list_next(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_space_written(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_tmp_snapshot(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_userspace_many(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_userspace_one(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_userspace_upgrade(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_vdev_add(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_vdev_attach(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_vdev_detach(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_vdev_remove(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_vdev_set_state(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_vdev_setfru(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_vdev_setpath(zfs_cmd_t *zc) { return 0; }
int zfs_ioc_vdev_split(zfs_cmd_t *zc) { return 0; }

/* secpolicy stubs */
int zfs_secpolicy_config(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_read(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_inject(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_diff(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_userspace_one(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_userspace_many(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_send(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_destroy(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_rename(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_recv(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_promote(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_inherit_prop(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_set_fsacl(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_share(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_smb_acl(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_userspace_upgrade(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_tmp_snapshot(zfs_cmd_t *zc) { return 0; }
int zfs_secpolicy_none(zfs_cmd_t *zc) { return 0; }

static void zfs_ioctl_register_legacy(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func, zfs_secpolicy_func_t *secpolicy,
    zfs_ioc_namecheck_t namecheck, boolean_t log_history,
    zfs_ioc_poolcheck_t pool_check)
{
    zfs_ioc_vec_t *vec = &zfs_ioc_vec[ioc - ZFS_IOC_FIRST];
    vec->zvec_legacy_func = func;
    vec->zvec_secpolicy = secpolicy;
    vec->zvec_namecheck = namecheck;
    vec->zvec_allow_log = log_history;
    vec->zvec_pool_check = pool_check;
}

static void zfs_ioctl_register_pool(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func, zfs_secpolicy_func_t *secpolicy,
    boolean_t log_history, zfs_ioc_poolcheck_t pool_check)
{
    zfs_ioctl_register_legacy(ioc, func, secpolicy,
        POOL_NAME, log_history, pool_check);
}

static void zfs_ioctl_register_pool_modify(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func)
{
    zfs_ioctl_register_legacy(ioc, func, zfs_secpolicy_config,
        POOL_NAME, B_TRUE, POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY);
}

static void zfs_ioctl_register_pool_meta(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func, zfs_secpolicy_func_t *secpolicy)
{
    zfs_ioctl_register_legacy(ioc, func, secpolicy,
        NO_NAME, B_FALSE, POOL_CHECK_NONE);
}

static void zfs_ioctl_register_dataset_read(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func)
{
    zfs_ioctl_register_legacy(ioc, func, zfs_secpolicy_read,
        DATASET_NAME, B_FALSE, POOL_CHECK_SUSPENDED);
}

static void zfs_ioctl_register_dataset_read_secpolicy(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func, zfs_secpolicy_func_t *secpolicy)
{
    zfs_ioctl_register_legacy(ioc, func, secpolicy,
        DATASET_NAME, B_FALSE, POOL_CHECK_SUSPENDED);
}

static void zfs_ioctl_register_dataset_modify(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func, zfs_secpolicy_func_t *secpolicy)
{
    zfs_ioctl_register_legacy(ioc, func, secpolicy,
        DATASET_NAME, B_TRUE, POOL_CHECK_SUSPENDED | POOL_CHECK_READONLY);
}

static void zfs_ioctl_register_dataset_nolog(zfs_ioc_t ioc,
    zfs_ioc_legacy_func_t *func, zfs_secpolicy_func_t *secpolicy,
    zfs_ioc_poolcheck_t pool_check)
{
    zfs_ioctl_register_legacy(ioc, func, secpolicy,
        DATASET_NAME, B_FALSE, pool_check);
}

static void zfs_ioctl_init(void)
{
    zfs_ioctl_register_pool(ZFS_IOC_POOL_CREATE, zfs_ioc_pool_create,
        zfs_secpolicy_config, B_TRUE, POOL_CHECK_NONE);
    zfs_ioctl_register_pool_modify(ZFS_IOC_POOL_SCAN, zfs_ioc_pool_scan);
    zfs_ioctl_register_pool_modify(ZFS_IOC_POOL_UPGRADE, zfs_ioc_pool_upgrade);
    zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_ADD, zfs_ioc_vdev_add);
    zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_REMOVE, zfs_ioc_vdev_remove);
    zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_SET_STATE, zfs_ioc_vdev_set_state);
    zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_ATTACH, zfs_ioc_vdev_attach);
    zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_DETACH, zfs_ioc_vdev_detach);
    zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_SETPATH, zfs_ioc_vdev_setpath);
    zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_SETFRU, zfs_ioc_vdev_setfru);
    zfs_ioctl_register_pool_modify(ZFS_IOC_POOL_SET_PROPS, zfs_ioc_pool_set_props);
    zfs_ioctl_register_pool_modify(ZFS_IOC_VDEV_SPLIT, zfs_ioc_vdev_split);
    zfs_ioctl_register_pool_modify(ZFS_IOC_POOL_REGUID, zfs_ioc_pool_reguid);

    zfs_ioctl_register_pool_meta(ZFS_IOC_POOL_CONFIGS, zfs_ioc_pool_configs,
        zfs_secpolicy_none);
    zfs_ioctl_register_pool_meta(ZFS_IOC_POOL_TRYIMPORT, zfs_ioc_pool_tryimport,
        zfs_secpolicy_config);
    zfs_ioctl_register_pool_meta(ZFS_IOC_INJECT_FAULT, zfs_ioc_inject_fault,
        zfs_secpolicy_inject);
    zfs_ioctl_register_pool_meta(ZFS_IOC_CLEAR_FAULT, zfs_ioc_clear_fault,
        zfs_secpolicy_inject);
    zfs_ioctl_register_pool_meta(ZFS_IOC_INJECT_LIST_NEXT, zfs_ioc_inject_list_next,
        zfs_secpolicy_inject);

    zfs_ioctl_register_pool(ZFS_IOC_POOL_DESTROY, zfs_ioc_pool_destroy,
        zfs_secpolicy_config, B_FALSE, POOL_CHECK_SUSPENDED);
    zfs_ioctl_register_pool(ZFS_IOC_POOL_EXPORT, zfs_ioc_pool_export,
        zfs_secpolicy_config, B_FALSE, POOL_CHECK_SUSPENDED);
    zfs_ioctl_register_pool(ZFS_IOC_POOL_STATS, zfs_ioc_pool_stats,
        zfs_secpolicy_read, B_FALSE, POOL_CHECK_NONE);
    zfs_ioctl_register_pool(ZFS_IOC_POOL_GET_PROPS, zfs_ioc_pool_get_props,
        zfs_secpolicy_read, B_FALSE, POOL_CHECK_NONE);
    zfs_ioctl_register_pool(ZFS_IOC_POOL_IMPORT, zfs_ioc_pool_import,
        zfs_secpolicy_config, B_TRUE, POOL_CHECK_NONE);
    zfs_ioctl_register_pool(ZFS_IOC_CLEAR, zfs_ioc_clear,
        zfs_secpolicy_config, B_TRUE, POOL_CHECK_READONLY);

    zfs_ioctl_register_dataset_read(ZFS_IOC_SPACE_WRITTEN, zfs_ioc_space_written);
    zfs_ioctl_register_dataset_read(ZFS_IOC_OBJSET_RECVD_PROPS, zfs_ioc_objset_recvd_props);
    zfs_ioctl_register_dataset_read(ZFS_IOC_NEXT_OBJ, zfs_ioc_next_obj);
    zfs_ioctl_register_dataset_read(ZFS_IOC_GET_FSACL, zfs_ioc_get_fsacl);
    zfs_ioctl_register_dataset_read(ZFS_IOC_OBJSET_STATS, zfs_ioc_objset_stats);
    zfs_ioctl_register_dataset_read(ZFS_IOC_OBJSET_ZPLPROPS, zfs_ioc_objset_zplprops);
    zfs_ioctl_register_dataset_read(ZFS_IOC_DATASET_LIST_NEXT, zfs_ioc_dataset_list_next);
    zfs_ioctl_register_dataset_read(ZFS_IOC_SNAPSHOT_LIST_NEXT, zfs_ioc_snapshot_list_next);
    zfs_ioctl_register_dataset_read(ZFS_IOC_SEND_PROGRESS, zfs_ioc_send_progress);

    zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_DIFF, zfs_ioc_diff,
        zfs_secpolicy_diff);
    zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_OBJ_TO_STATS, zfs_ioc_obj_to_stats,
        zfs_secpolicy_diff);
    zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_OBJ_TO_PATH, zfs_ioc_obj_to_path,
        zfs_secpolicy_diff);
    zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_USERSPACE_ONE, zfs_ioc_userspace_one,
        zfs_secpolicy_userspace_one);
    zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_USERSPACE_MANY, zfs_ioc_userspace_many,
        zfs_secpolicy_userspace_many);
    zfs_ioctl_register_dataset_read_secpolicy(ZFS_IOC_SEND, zfs_ioc_send,
        zfs_secpolicy_send);

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
    zfs_ioctl_register_dataset_modify(ZFS_IOC_INHERIT_PROP, zfs_ioc_inherit_prop,
        zfs_secpolicy_inherit_prop);
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

    zfs_ioctl_register_pool(ZFS_IOC_ERROR_LOG, zfs_ioc_error_log,
        zfs_secpolicy_inject, B_FALSE, POOL_CHECK_SUSPENDED);
    zfs_ioctl_register_pool(ZFS_IOC_DSOBJ_TO_DSNAME, zfs_ioc_dsobj_to_dsname,
        zfs_secpolicy_diff, B_FALSE, POOL_CHECK_SUSPENDED);
    zfs_ioctl_register_pool(ZFS_IOC_POOL_GET_HISTORY, zfs_ioc_pool_get_history,
        zfs_secpolicy_config, B_FALSE, POOL_CHECK_SUSPENDED);

    zfs_ioctl_register_legacy(ZFS_IOC_POOL_FREEZE, zfs_ioc_pool_freeze,
        zfs_secpolicy_config, NO_NAME, B_FALSE, POOL_CHECK_READONLY);
    zfs_ioctl_register_legacy(ZFS_IOC_EVENTS_NEXT, zfs_ioc_events_next,
        zfs_secpolicy_config, NO_NAME, B_FALSE, POOL_CHECK_NONE);
    zfs_ioctl_register_legacy(ZFS_IOC_EVENTS_CLEAR, zfs_ioc_events_clear,
        zfs_secpolicy_config, NO_NAME, B_FALSE, POOL_CHECK_NONE);
    zfs_ioctl_register_legacy(ZFS_IOC_EVENTS_SEEK, zfs_ioc_events_seek,
        zfs_secpolicy_config, NO_NAME, B_FALSE, POOL_CHECK_NONE);
}
