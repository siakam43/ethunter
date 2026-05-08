/* CG-Bench fixture: fnptr-global-struct-array/example_5 */
/* fnptr: fstypes[protocol]->commit_shares, targets: nfs_commit_shares, smb_update_shares */

void sa_commit_shares(enum sa_protocol protocol)
{
	/* CSTYLED */
	VALIDATE_PROTOCOL(protocol, );

	fstypes[protocol]->commit_shares();
}

static const sa_fstype_t *fstypes[SA_PROTOCOL_COUNT] =
	{&libshare_nfs_type, &libshare_smb_type};

const sa_fstype_t libshare_nfs_type = {
        .enable_share = nfs_enable_share,
	.disable_share = nfs_disable_share,
	.is_shared = nfs_is_shared,

	.validate_shareopts = nfs_validate_shareopts,
	.commit_shares = nfs_commit_shares,
	.truncate_shares = nfs_truncate_shares,
};

const sa_fstype_t libshare_smb_type = {
	.enable_share = smb_enable_share,
	.disable_share = smb_disable_share,
	.is_shared = smb_is_share_active,

	.validate_shareopts = smb_validate_shareopts,
	.commit_shares = smb_update_shares,
};


/* Stub implementation for nfs_commit_shares */
void nfs_commit_shares(void) {}



/* Stub implementation for smb_update_shares */
void smb_update_shares(void) {}
