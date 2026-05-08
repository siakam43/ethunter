/* CG-Bench fixture: fnptr-global-struct-array/example_10 */
/* fnptr: impl->funcs->size, targets: ssh_rsa_size */

u_int
sshkey_size(const struct sshkey *k)
{
	const struct sshkey_impl *impl;

	if ((impl = sshkey_impl_from_key(k)) == NULL)
		return 0;
	if (impl->funcs->size != NULL)
		return impl->funcs->size(k);
	return impl->keybits;
}

static const struct sshkey_impl *
sshkey_impl_from_key(const struct sshkey *k)
{
	if (k == NULL)
		return NULL;
	return sshkey_impl_from_type_nid(k->type, k->ecdsa_nid);
}

static const struct sshkey_impl *
sshkey_impl_from_type_nid(int type, int nid)
{
	int i;

	for (i = 0; keyimpls[i] != NULL; i++) {
		if (keyimpls[i]->type == type &&
		    (keyimpls[i]->nid == 0 || keyimpls[i]->nid == nid))
			return keyimpls[i];
	}
	return NULL;
}

const struct sshkey_impl * const keyimpls[] = {
	&sshkey_ed25519_impl,
	&sshkey_ed25519_cert_impl,
#ifdef ENABLE_SK
	&sshkey_ed25519_sk_impl,
	&sshkey_ed25519_sk_cert_impl,
#endif
#ifdef WITH_OPENSSL
# ifdef OPENSSL_HAS_ECC
	&sshkey_ecdsa_nistp256_impl,
	&sshkey_ecdsa_nistp256_cert_impl,
	&sshkey_ecdsa_nistp384_impl,
	&sshkey_ecdsa_nistp384_cert_impl,
#  ifdef OPENSSL_HAS_NISTP521
	&sshkey_ecdsa_nistp521_impl,
	&sshkey_ecdsa_nistp521_cert_impl,
#  endif /* OPENSSL_HAS_NISTP521 */
#  ifdef ENABLE_SK
	&sshkey_ecdsa_sk_impl,
	&sshkey_ecdsa_sk_cert_impl,
	&sshkey_ecdsa_sk_webauthn_impl,
#  endif /* ENABLE_SK */
# endif /* OPENSSL_HAS_ECC */
	&sshkey_dss_impl,
	&sshkey_dsa_cert_impl,
	&sshkey_rsa_impl,
	&sshkey_rsa_cert_impl,
	&sshkey_rsa_sha256_impl,
	&sshkey_rsa_sha256_cert_impl,
	&sshkey_rsa_sha512_impl,
	&sshkey_rsa_sha512_cert_impl,
#endif /* WITH_OPENSSL */
#ifdef WITH_XMSS
	&sshkey_xmss_impl,
	&sshkey_xmss_cert_impl,
#endif
	NULL
};

const struct sshkey_impl sshkey_rsa_impl = {
	/* .name = */		"ssh-rsa",
	/* .shortname = */	"RSA",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_RSA,
	/* .nid = */		0,
	/* .cert = */		0,
	/* .sigonly = */	0,
	/* .keybits = */	0,
	/* .funcs = */		&sshkey_rsa_funcs,
};

static const struct sshkey_impl_funcs sshkey_rsa_funcs = {
	/* .size = */		ssh_rsa_size,
	/* .alloc = */		ssh_rsa_alloc,
	/* .cleanup = */	ssh_rsa_cleanup,
	/* .equal = */		ssh_rsa_equal,
	/* .ssh_serialize_public = */ ssh_rsa_serialize_public,
	/* .ssh_deserialize_public = */ ssh_rsa_deserialize_public,
	/* .ssh_serialize_private = */ ssh_rsa_serialize_private,
	/* .ssh_deserialize_private = */ ssh_rsa_deserialize_private,
	/* .generate = */	ssh_rsa_generate,
	/* .copy_public = */	ssh_rsa_copy_public,
	/* .sign = */		ssh_rsa_sign,
	/* .verify = */		ssh_rsa_verify,
};


/* Stub implementation for ssh_rsa_size */
void ssh_rsa_size(void) {}
