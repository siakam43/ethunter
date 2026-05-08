/* CG-Bench fixture: fnptr-global-struct/example_10 */
/* fnptr: sshkey_ecdsa_funcs.equal, targets: ssh_ecdsa_equal */

static int
ssh_ecdsa_sk_equal(const struct sshkey *a, const struct sshkey *b)
{
	if (!sshkey_sk_fields_equal(a, b))
		return 0;
	if (!sshkey_ecdsa_funcs.equal(a, b))
		return 0;
	return 1;
}

/* NB. not static; used by ECDSA-SK */
const struct sshkey_impl_funcs sshkey_ecdsa_funcs = {
	/* .size = */		ssh_ecdsa_size,
	/* .alloc = */		NULL,
	/* .cleanup = */	ssh_ecdsa_cleanup,
	/* .equal = */		ssh_ecdsa_equal,
	/* .ssh_serialize_public = */ ssh_ecdsa_serialize_public,
	/* .ssh_deserialize_public = */ ssh_ecdsa_deserialize_public,
	/* .ssh_serialize_private = */ ssh_ecdsa_serialize_private,
	/* .ssh_deserialize_private = */ ssh_ecdsa_deserialize_private,
	/* .generate = */	ssh_ecdsa_generate,
	/* .copy_public = */	ssh_ecdsa_copy_public,
	/* .sign = */		ssh_ecdsa_sign,
	/* .verify = */		ssh_ecdsa_verify,
};

struct sshkey_impl_funcs {
	u_int (*size)(const struct sshkey *);	/* optional */
	int (*alloc)(struct sshkey *);		/* optional */
	void (*cleanup)(struct sshkey *);	/* optional */
	int (*equal)(const struct sshkey *, const struct sshkey *);
	int (*serialize_public)(const struct sshkey *, struct sshbuf *,
	    enum sshkey_serialize_rep);
	int (*deserialize_public)(const char *, struct sshbuf *,
	    struct sshkey *);
	int (*serialize_private)(const struct sshkey *, struct sshbuf *,
	    enum sshkey_serialize_rep);
	int (*deserialize_private)(const char *, struct sshbuf *,
	    struct sshkey *);
	int (*generate)(struct sshkey *, int);	/* optional */
	int (*copy_public)(const struct sshkey *, struct sshkey *);
	int (*sign)(struct sshkey *, u_char **, size_t *,
	    const u_char *, size_t, const char *,
	    const char *, const char *, u_int); /* optional */
	int (*verify)(const struct sshkey *, const u_char *, size_t,
	    const u_char *, size_t, const char *, u_int,
	    struct sshkey_sig_details **);
};


/* Stub implementation for ssh_ecdsa_equal */
void ssh_ecdsa_equal(void) {}
