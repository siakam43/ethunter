/* et_bench fixture: fnptr-global-struct/example_7 */
/* fnptr: aclp->z_ops->ace_size, targets: zfs_ace_v0_size, zfs_ace_fuid_size */

#include <stddef.h>
#include <stdint.h>

typedef int boolean_t;
typedef uint64_t umode_t;
#define B_TRUE  1
#define B_FALSE 0
#define ZFS_ACL_VERSION_FUID 2

typedef struct list list_t;
void list_create(list_t *, size_t, size_t);

typedef struct cred cred_t;
typedef struct vattr vattr_t;
typedef struct vsecattr vsecattr_t;
typedef struct zfsvfs zfsvfs_t;
typedef struct zidmap zidmap_t;
typedef struct znode znode_t;

struct znode {
    void *z_acl_lock;
    void *z_lock;
    uint64_t z_pflags;
    int i_mode;
};
typedef znode_t vnode_t;
#define ZTOZSB(zp)  ((zfsvfs_t *)0)
#define ZTOI(zp)    ((vnode_t *)(zp))
#define S_ISDIR(m)  ((m) & 040000)
#define S_ISLNK(m)  ((m) & 0120000)
#define FTAG "ftag"

#define ZFS_ACL_DISCARD      1
#define ZFS_ACL_GROUPMASK    2
#define ZFS_ACL_PASSTHROUGH  3
#define ZFS_ACL_PASSTHROUGH_X 4
#define ZFS_ACL_AUTO_INHERIT 0x100
#define ZFS_ACL_TRIVIAL      0x200

#define IS_ROOT_NODE 0x1

#define V4_ACL_WIDE_FLAGS 0xFF
#define ZFS_INHERIT_ACE   0x01
#define ZFS_XATTR         0x02

typedef enum {
    ZFS_ACL_INHERIT_DEFAULT
} zfs_acl_inherit_t;

struct zfsvfs {
    zfs_acl_inherit_t z_acl_inherit;
    int z_acl_mode;
};

/* ACL ops interface */
typedef struct acl_ops {
    uint64_t (*ace_mask_get)(void *acep);
    void     (*ace_mask_set)(void *acep, uint64_t mask);
    uint16_t (*ace_flags_get)(void *acep);
    void     (*ace_flags_set)(void *acep, uint16_t flags);
    uint16_t (*ace_type_get)(void *acep);
    void     (*ace_type_set)(void *acep, uint16_t type);
    uint64_t (*ace_who_get)(void *acep);
    void     (*ace_who_set)(void *acep, uint64_t who);
    size_t   (*ace_size)(void *acep);
    size_t   (*ace_abstract_size)(void);
    size_t   (*ace_mask_off)(void);
    void    *(*ace_data)(void *acep);
} acl_ops_t;

typedef struct zfs_acl_node {
    void *z_next;
} zfs_acl_node_t;

typedef struct zfs_acl {
    list_t z_acl;
    int z_version;
    acl_ops_t *z_ops;
    uint64_t z_hints;
    uint64_t z_fuid;
    uint64_t z_fgid;
} zfs_acl_t;

typedef struct zfs_acl_ids {
    zfs_acl_t *z_aclp;
    umode_t z_mode;
    uint64_t z_fuid;
    uint64_t z_fgid;
} zfs_acl_ids_t;

typedef void *mutex_t;
void mutex_enter(mutex_t *m);
void mutex_exit(mutex_t *m);

static void *kmem_zalloc(size_t sz, int flags) { return NULL; }

void *zfs_acl_next_ace(zfs_acl_t *aclp, void *acep, uint64_t *who,
                        uint64_t *access_mask, uint16_t *iflags,
                        uint16_t *type) { return NULL; }

void zfs_set_ace(zfs_acl_t *aclp, void *zacep, uint64_t access_mask,
                 uint16_t type, uint64_t who, uint16_t iflags) {}

zfs_acl_t *zfs_acl_alloc(int vers);
zfs_acl_t *zfs_acl_inherit(zfsvfs_t *zfsvfs, umode_t va_mode, zfs_acl_t *paclp,
                            uint64_t mode, boolean_t *need_chmod);
int zfs_acl_node_read(znode_t *zp, boolean_t lock, zfs_acl_t **aclp,
                       boolean_t must);
int zfs_acl_version_zp(znode_t *zp) { return 1; }
int zfs_ace_walk(void *data, uint8_t *ace_data, uint16_t *ace_flags,
                 uint16_t *ace_type, uint64_t *ace_who) { return 0; }
int ace_trivial_common(zfs_acl_t *aclp, int flags, int (*walk_fn)(void*, uint8_t*, uint16_t*, uint16_t*, uint64_t*)) { return 0; }
uint64_t zfs_mode_compute(uint64_t mode, zfs_acl_t *aclp, uint64_t *hints,
                          uint64_t fuid, uint64_t fgid) { return mode; }

/* The two ace_size targets */
size_t zfs_ace_v0_size(void *acep) { return 8; }
size_t zfs_ace_fuid_size(void *acep) { return 16; }

/* Other v0 ops */
uint64_t zfs_ace_v0_get_mask(void *acep) { return 0; }
void zfs_ace_v0_set_mask(void *acep, uint64_t m) {}
uint16_t zfs_ace_v0_get_flags(void *acep) { return 0; }
void zfs_ace_v0_set_flags(void *acep, uint16_t f) {}
uint16_t zfs_ace_v0_get_type(void *acep) { return 0; }
void zfs_ace_v0_set_type(void *acep, uint16_t t) {}
uint64_t zfs_ace_v0_get_who(void *acep) { return 0; }
void zfs_ace_v0_set_who(void *acep, uint64_t w) {}
size_t zfs_ace_v0_abstract_size(void) { return 8; }
size_t zfs_ace_v0_mask_off(void) { return 0; }
void *zfs_ace_v0_data(void *acep) { return acep; }

/* Other fuid ops */
uint64_t zfs_ace_fuid_get_mask(void *acep) { return 0; }
void zfs_ace_fuid_set_mask(void *acep, uint64_t m) {}
uint16_t zfs_ace_fuid_get_flags(void *acep) { return 0; }
void zfs_ace_fuid_set_flags(void *acep, uint16_t f) {}
uint16_t zfs_ace_fuid_get_type(void *acep) { return 0; }
void zfs_ace_fuid_set_type(void *acep, uint16_t t) {}
uint64_t zfs_ace_fuid_get_who(void *acep) { return 0; }
void zfs_ace_fuid_set_who(void *acep, uint64_t w) {}
size_t zfs_ace_fuid_abstract_size(void) { return 16; }
size_t zfs_ace_fuid_mask_off(void) { return 0; }
void *zfs_ace_fuid_data(void *acep) { return acep; }

/* Global ops structs */
static const acl_ops_t zfs_acl_v0_ops = {
    .ace_mask_get = zfs_ace_v0_get_mask,
    .ace_mask_set = zfs_ace_v0_set_mask,
    .ace_flags_get = zfs_ace_v0_get_flags,
    .ace_flags_set = zfs_ace_v0_set_flags,
    .ace_type_get = zfs_ace_v0_get_type,
    .ace_type_set = zfs_ace_v0_set_type,
    .ace_who_get = zfs_ace_v0_get_who,
    .ace_who_set = zfs_ace_v0_set_who,
    .ace_size = zfs_ace_v0_size,
    .ace_abstract_size = zfs_ace_v0_abstract_size,
    .ace_mask_off = zfs_ace_v0_mask_off,
    .ace_data = zfs_ace_v0_data,
};

static const acl_ops_t zfs_acl_fuid_ops = {
    .ace_mask_get = zfs_ace_fuid_get_mask,
    .ace_mask_set = zfs_ace_fuid_set_mask,
    .ace_flags_get = zfs_ace_fuid_get_flags,
    .ace_flags_set = zfs_ace_fuid_set_flags,
    .ace_type_get = zfs_ace_fuid_get_type,
    .ace_type_set = zfs_ace_fuid_set_type,
    .ace_who_get = zfs_ace_fuid_get_who,
    .ace_who_set = zfs_ace_fuid_set_who,
    .ace_size = zfs_ace_fuid_size,
    .ace_abstract_size = zfs_ace_fuid_abstract_size,
    .ace_mask_off = zfs_ace_fuid_mask_off,
    .ace_data = zfs_ace_fuid_data,
};

zfs_acl_t *zfs_acl_alloc(int vers)
{
    zfs_acl_t *aclp;
    aclp = (zfs_acl_t *)kmem_zalloc(sizeof(zfs_acl_t), 0);
    list_create(&aclp->z_acl, sizeof(zfs_acl_node_t), 0);
    aclp->z_version = vers;
    if (vers == ZFS_ACL_VERSION_FUID)
        aclp->z_ops = &zfs_acl_fuid_ops;
    else
        aclp->z_ops = &zfs_acl_v0_ops;
    return aclp;
}

static void zfs_acl_chmod(boolean_t isdir, uint64_t mode, boolean_t split,
                           boolean_t trim, zfs_acl_t *aclp)
{
    void *acep = NULL;
    void *zacep = NULL;
    uint64_t who, access_mask;
    uint16_t iflags, type;
    size_t ace_size;
    size_t new_bytes = 0;
    int new_count = 0;

    while ((acep = zfs_acl_next_ace(aclp, acep, &who, &access_mask,
        &iflags, &type)) != NULL) {
        zfs_set_ace(aclp, zacep, access_mask, type, who, iflags);
        ace_size = aclp->z_ops->ace_size(acep);
        zacep = (void *)((uintptr_t)zacep + ace_size);
        new_count++;
        new_bytes += ace_size;
    }
}

int zfs_acl_chmod_setattr(znode_t *zp, zfs_acl_t **aclp, uint64_t mode)
{
    int error = 0;
    mutex_enter(&zp->z_acl_lock);
    mutex_enter(&zp->z_lock);
    if (ZTOZSB(zp)->z_acl_mode == ZFS_ACL_DISCARD)
        *aclp = zfs_acl_alloc(zfs_acl_version_zp(zp));
    else
        error = zfs_acl_node_read(zp, B_TRUE, aclp, B_TRUE);

    if (error == 0) {
        (*aclp)->z_hints = zp->z_pflags & V4_ACL_WIDE_FLAGS;
        zfs_acl_chmod(S_ISDIR(ZTOI(zp)->i_mode), mode, B_TRUE,
            (ZTOZSB(zp)->z_acl_mode == ZFS_ACL_GROUPMASK), *aclp);
    }
    mutex_exit(&zp->z_lock);
    mutex_exit(&zp->z_acl_lock);
    return error;
}

int zfs_acl_ids_create(znode_t *dzp, int flag, vattr_t *vap, cred_t *cr,
    vsecattr_t *vsecp, zfs_acl_ids_t *acl_ids, zidmap_t *mnt_ns)
{
    zfs_acl_t *paclp = NULL;
    boolean_t inherited = B_FALSE;
    boolean_t trim = B_FALSE;
    zfsvfs_t *zfsvfs = NULL;
    boolean_t need_chmod = B_FALSE;

    acl_ids->z_mode = vap ? vap->va_mode : 0;

    if (acl_ids->z_aclp == NULL) {
        if (!(flag & IS_ROOT_NODE) &&
            (dzp->z_pflags & ZFS_INHERIT_ACE) &&
            !(dzp->z_pflags & ZFS_XATTR)) {
            acl_ids->z_aclp = zfs_acl_inherit(zfsvfs,
                vap ? vap->va_mode : 0, paclp, acl_ids->z_mode, &need_chmod);
            inherited = B_TRUE;
        } else {
            acl_ids->z_aclp = zfs_acl_alloc(zfs_acl_version_zp(dzp));
            acl_ids->z_aclp->z_hints |= ZFS_ACL_TRIVIAL;
        }
        mutex_exit(&dzp->z_lock);
        mutex_exit(&dzp->z_acl_lock);

        if (need_chmod) {
            if (S_ISDIR(vap ? vap->va_mode : 0))
                acl_ids->z_aclp->z_hints |= ZFS_ACL_AUTO_INHERIT;

            if (zfsvfs && zfsvfs->z_acl_mode == ZFS_ACL_GROUPMASK &&
                zfsvfs->z_acl_inherit != ZFS_ACL_PASSTHROUGH &&
                zfsvfs->z_acl_inherit != ZFS_ACL_PASSTHROUGH_X)
                trim = B_TRUE;
            zfs_acl_chmod(vap ? vap->va_mode : 0, acl_ids->z_mode, B_FALSE,
                trim, acl_ids->z_aclp);
        }
    }

    if (inherited || vsecp) {
        acl_ids->z_mode = zfs_mode_compute(acl_ids->z_mode,
            acl_ids->z_aclp, &acl_ids->z_aclp->z_hints,
            acl_ids->z_fuid, acl_ids->z_fgid);
        if (ace_trivial_common(acl_ids->z_aclp, 0, zfs_ace_walk) == 0)
            acl_ids->z_aclp->z_hints |= ZFS_ACL_TRIVIAL;
    }
    return 0;
}

zfs_acl_t *zfs_acl_inherit(zfsvfs_t *zfsvfs, umode_t va_mode, zfs_acl_t *paclp,
    uint64_t mode, boolean_t *need_chmod)
{
    void *pacep = NULL;
    uint64_t who, access_mask;
    uint16_t iflags, type;
    zfs_acl_t *aclp;
    zfs_acl_inherit_t aclinherit;

    aclp = zfs_acl_alloc(paclp ? paclp->z_version : 1);
    aclinherit = zfsvfs ? zfsvfs->z_acl_inherit : ZFS_ACL_INHERIT_DEFAULT;
    if (aclinherit == ZFS_ACL_DISCARD || S_ISLNK(va_mode))
        return aclp;

    while ((pacep = zfs_acl_next_ace(paclp, pacep, &who,
        &access_mask, &iflags, &type)) != NULL) {
        /* iterate */
    }
    return aclp;
}
