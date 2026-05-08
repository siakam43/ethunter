/* CG-Bench fixture: fnptr-global-struct/example_11 */
/* fnptr: sshkey_ed25519_funcs.serialize_public, targets: ssh_ed25519_serialize_public */

static int
ssh_ed25519_sk_serialize_public(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;

	if ((r = sshkey_ed25519_funcs.serialize_public(key, b, opts)) != 0)
		return r;
	if ((r = sshkey_serialize_sk(key, b)) != 0)
		return r;

	return 0;
}

/* NB. not static; used by ED25519-SK */
const struct sshkey_impl_funcs sshkey_ed25519_funcs = {
	/* .size = */		NULL,
	/* .alloc = */		NULL,
	/* .cleanup = */	ssh_ed25519_cleanup,
	/* .equal = */		ssh_ed25519_equal,
	/* .ssh_serialize_public = */ ssh_ed25519_serialize_public,
	/* .ssh_deserialize_public = */ ssh_ed25519_deserialize_public,
	/* .ssh_serialize_private = */ ssh_ed25519_serialize_private,
	/* .ssh_deserialize_private = */ ssh_ed25519_deserialize_private,
	/* .generate = */	ssh_ed25519_generate,
	/* .copy_public = */	ssh_ed25519_copy_public,
	/* .sign = */		ssh_ed25519_sign,
	/* .verify = */		ssh_ed25519_verify,
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


/* Stub implementation for ssh_ed25519_serialize_public */
void ssh_ed25519_serialize_public(void) {}
