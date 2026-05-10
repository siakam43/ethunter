/* ET-Bench fixture: fnptr-global-struct-array/example_5 */
/* fnptr: fstypes[protocol]->commit_shares, targets: nfs_commit_shares, smb_update_shares */
/* Pattern: global array of filesystem type structs with commit function pointer per type */

#include <stdio.h>
#include <stdlib.h>

#define SA_PROTOCOL_NFS 0
#define SA_PROTOCOL_SMB 1
#define SA_PROTOCOL_COUNT 2
#define VALIDATE_PROTOCOL(p, r) do { if ((p) >= SA_PROTOCOL_COUNT) return r; } while(0)

typedef struct sa_fstype {
    int (*enable_share)(void);
    int (*disable_share)(void);
    int (*is_shared)(void);
    int (*validate_shareopts)(void);
    int (*commit_shares)(void);
    int (*truncate_shares)(void);
} sa_fstype_t;

enum sa_protocol {
    SA_PROTOCOL_NFS_E = SA_PROTOCOL_NFS,
    SA_PROTOCOL_SMB_E = SA_PROTOCOL_SMB,
    SA_PROTOCOL_COUNT_E = SA_PROTOCOL_COUNT
};

static int nfs_enable_share(void) { return 0; }
static int nfs_disable_share(void) { return 0; }
static int nfs_is_shared(void) { return 0; }
static int nfs_validate_shareopts(void) { return 0; }
static int nfs_truncate_shares(void) { return 0; }

static int smb_enable_share(void) { return 0; }
static int smb_disable_share(void) { return 0; }
static int smb_is_share_active(void) { return 0; }
static int smb_validate_shareopts(void) { return 0; }

/* Target functions */
static int nfs_commit_shares(void) { return 0; }
static int smb_update_shares(void) { return 0; }

static const sa_fstype_t libshare_nfs_type = {
    nfs_enable_share,
    nfs_disable_share,
    nfs_is_shared,
    nfs_validate_shareopts,
    nfs_commit_shares,
    nfs_truncate_shares,
};

static const sa_fstype_t libshare_smb_type = {
    smb_enable_share,
    smb_disable_share,
    smb_is_share_active,
    smb_validate_shareopts,
    smb_update_shares,
    nfs_truncate_shares,
};

static const sa_fstype_t *fstypes[SA_PROTOCOL_COUNT] =
    { &libshare_nfs_type, &libshare_smb_type };

void sa_commit_shares(enum sa_protocol protocol)
{
    if (protocol >= SA_PROTOCOL_COUNT) return;
    fstypes[protocol]->commit_shares();
}
