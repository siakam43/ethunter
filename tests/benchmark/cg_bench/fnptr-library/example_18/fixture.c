/* CG-Bench fixture: fnptr-library/example_18 */
/* fnptr: kex->verify_host_key, targets: key_print_wrapper, _ssh_verify_host_key */

int
kex_verify_host_key(struct ssh *ssh, struct sshkey *server_host_key)
{
	struct kex *kex = ssh->kex;

	if (kex->verify_host_key == NULL) {
		error_f("missing hostkey verifier");
		return SSH_ERR_INVALID_ARGUMENT;
	}
	if (server_host_key->type != kex->hostkey_type ||
	    (kex->hostkey_type == KEY_ECDSA &&
	    server_host_key->ecdsa_nid != kex->hostkey_nid))
		return SSH_ERR_KEY_TYPE_MISMATCH;
	if (kex->verify_host_key(server_host_key, ssh) == -1)
		return  SSH_ERR_SIGNATURE_INVALID;
	return 0;
}

int
ssh_init(struct ssh **sshp, int is_server, struct kex_params *kex_params)
{
	struct ssh *ssh;

	if ((ssh = ssh_packet_set_connection(NULL, -1, -1)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (is_server)
		ssh_packet_set_server(ssh);

	ssh->kex->server = is_server;
	if (is_server) {
		ssh->kex->kex[KEX_C25519_SHA256] = kex_gen_client;
		ssh->kex->kex[KEX_KEM_SNTRUP761X25519_SHA512] = kex_gen_client;
		ssh->kex->verify_host_key =&_ssh_verify_host_key;
	}
	*sshp = ssh;
	return 0;
}

int
_ssh_verify_host_key(struct sshkey *hostkey, struct ssh *ssh)
{
	struct key_entry *k;

	debug3_f("need %s", sshkey_type(hostkey));
	TAILQ_FOREACH(k, &ssh->public_keys, next) {
		debug3_f("check %s", sshkey_type(k->key));
		if (sshkey_equal_public(hostkey, k->key))
			return (0);	/* ok */
	}
	return (-1);	/* failed */
}

int
ssh_set_verify_host_key_callback(struct ssh *ssh,
    int (*cb)(struct sshkey *, struct ssh *))
{
	if (cb == NULL || ssh->kex == NULL)
		return SSH_ERR_INVALID_ARGUMENT;

	ssh->kex->verify_host_key = cb;

	return 0;
}

static void
keygrab_ssh2(con *c)
{
	ssh_set_verify_host_key_callback(c->c_ssh, key_print_wrapper);
}

static int
key_print_wrapper(struct sshkey *hostkey, struct ssh *ssh)
{
	con *c;

	if ((c = ssh_get_app_data(ssh)) != NULL)
		keyprint(c, hostkey);
	/* always abort key exchange */
	return -1;
}