/* CG-Bench fixture: fnptr-global-struct/example_7 */
/* fnptr: aclp->z_ops->ace_size, targets: zfs_ace_v0_size, zfs_ace_fuid_size */

static void
zfs_acl_chmod(boolean_t isdir, uint64_t mode, boolean_t split, boolean_t trim,
    zfs_acl_t *aclp)
{
	...
	while ((acep = zfs_acl_next_ace(aclp, acep, &who, &access_mask,
	    &iflags, &type))) {
		...
		zfs_set_ace(aclp, zacep, access_mask, type, who, iflags);
		ace_size = aclp->z_ops->ace_size(acep);
		zacep = (void *)((uintptr_t)zacep + ace_size);
		new_count++;
		new_bytes += ace_size;
	}
}

int
zfs_acl_chmod_setattr(znode_t *zp, zfs_acl_t **aclp, uint64_t mode)
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

	return (error);
}

int
zfs_acl_ids_create(znode_t *dzp, int flag, vattr_t *vap, cred_t *cr,
    vsecattr_t *vsecp, zfs_acl_ids_t *acl_ids, zidmap_t *mnt_ns)
{
	...

	memset(acl_ids, 0, sizeof (zfs_acl_ids_t));
	acl_ids->z_mode = vap->va_mode;

	...

	if (acl_ids->z_aclp == NULL) {
		...
		if (!(flag & IS_ROOT_NODE) &&
		    (dzp->z_pflags & ZFS_INHERIT_ACE) &&
		    !(dzp->z_pflags & ZFS_XATTR)) {
			...
			acl_ids->z_aclp = zfs_acl_inherit(zfsvfs,
			    vap->va_mode, paclp, acl_ids->z_mode, &need_chmod);
			inherited = B_TRUE;
		} else {
			acl_ids->z_aclp =
			    zfs_acl_alloc(zfs_acl_version_zp(dzp));
			acl_ids->z_aclp->z_hints |= ZFS_ACL_TRIVIAL;
		}
		mutex_exit(&dzp->z_lock);
		mutex_exit(&dzp->z_acl_lock);

		if (need_chmod) {
			if (S_ISDIR(vap->va_mode))
				acl_ids->z_aclp->z_hints |=
				    ZFS_ACL_AUTO_INHERIT;

			if (zfsvfs->z_acl_mode == ZFS_ACL_GROUPMASK &&
			    zfsvfs->z_acl_inherit != ZFS_ACL_PASSTHROUGH &&
			    zfsvfs->z_acl_inherit != ZFS_ACL_PASSTHROUGH_X)
				trim = B_TRUE;
			zfs_acl_chmod(vap->va_mode, acl_ids->z_mode, B_FALSE,
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

	return (0);
}

static zfs_acl_t *
zfs_acl_inherit(zfsvfs_t *zfsvfs, umode_t va_mode, zfs_acl_t *paclp,
    uint64_t mode, boolean_t *need_chmod)
{
	void		*pacep = NULL;
	void		*acep;
	...

	aclp = zfs_acl_alloc(paclp->z_version);
	aclinherit = zfsvfs->z_acl_inherit;
	if (aclinherit == ZFS_ACL_DISCARD || S_ISLNK(va_mode))
		return (aclp);

	while ((pacep = zfs_acl_next_ace(paclp, pacep, &who,
	    &access_mask, &iflags, &type))) {
			...
	}

	return (aclp);
}

zfs_acl_t *
zfs_acl_alloc(int vers)
{
	zfs_acl_t *aclp;

	aclp = kmem_zalloc(sizeof (zfs_acl_t), KM_SLEEP);
	list_create(&aclp->z_acl, sizeof (zfs_acl_node_t),
	    offsetof(zfs_acl_node_t, z_next));
	aclp->z_version = vers;
	if (vers == ZFS_ACL_VERSION_FUID)
		aclp->z_ops = &zfs_acl_fuid_ops;
	else
		aclp->z_ops = &zfs_acl_v0_ops;
	return (aclp);
}

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
	.ace_data = zfs_ace_v0_data
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
	.ace_data = zfs_ace_fuid_data
};


/* Stub implementation for zfs_ace_v0_size */
void zfs_ace_v0_size(void) {}



/* Stub implementation for zfs_ace_fuid_size */
void zfs_ace_fuid_size(void) {}
