/* CG-Bench fixture: fnptr-dynamic-call/example_5 */
/* fnptr: skp->sk_enroll, targets: sk_enroll, ssh_sk_enroll */

int
sshsk_enroll(int type, const char *provider_path, const char *device,
    const char *application, const char *userid, uint8_t flags,
    const char *pin, struct sshbuf *challenge_buf,
    struct sshkey **keyp, struct sshbuf *attest)
{
	if ((skp = sshsk_open(provider_path)) == NULL) {
		r = SSH_ERR_INVALID_FORMAT; /* XXX sshsk_open return code? */
		goto out;
	}
	/* XXX validate flags? */
	/* enroll key */
	if ((r = skp->sk_enroll(alg, challenge, challenge_len, application,
	    flags, pin, opts, &resp)) != 0) {
		debug_f("provider \"%s\" failure %d", provider_path, r);
		r = skerr_to_ssherr(r);
		goto out;
	}
 out:
	return r;
}

static struct sshsk_provider *
sshsk_open(const char *path)
{
	struct sshsk_provider *ret = NULL;
	uint32_t version;

	if (path == NULL || *path == '\0') {
		error("No FIDO SecurityKeyProvider specified");
		return NULL;
	}
	if ((ret = calloc(1, sizeof(*ret))) == NULL) {
		error_f("calloc failed");
		return NULL;
	}
	if ((ret->path = strdup(path)) == NULL) {
		error_f("strdup failed");
		goto fail;
	}
	/* Skip the rest if we're using the linked in middleware */
	if (strcasecmp(ret->path, "internal") == 0) {
#ifdef ENABLE_SK_INTERNAL
		ret->sk_enroll = ssh_sk_enroll;
		ret->sk_sign = ssh_sk_sign;
		ret->sk_load_resident_keys = ssh_sk_load_resident_keys;
		return ret;
#else
		error("internal security key support not enabled");
		goto fail;
#endif
	}
	if ((ret->dlhandle = dlopen(path, RTLD_NOW)) == NULL)
		fatal("Provider \"%s\" dlopen failed: %s", path, dlerror());
	if ((ret->sk_enroll = dlsym(ret->dlhandle, "sk_enroll")) == NULL) {
		error("Provider %s dlsym(sk_enroll) failed: %s",
		    path, dlerror());
		goto fail;
	}
	/* success */
	return ret;
fail:
	sshsk_free(ret);
	return NULL;
}

int ssh_sk_enroll(int alg, const uint8_t *challenge,
    size_t challenge_len, const char *application, uint8_t flags,
    const char *pin, struct sk_option **opts,
    struct sk_enroll_response **enroll_response);


/* Stub implementation for sk_enroll */
void sk_enroll(void) {}



/* Stub implementation for ssh_sk_enroll */
void ssh_sk_enroll(int alg, const uint8_t *challenge, size_t challenge_len, const char *application, uint8_t flags, const char *pin, struct sk_option **opts, struct sk_enroll_response **enroll_response) {}
