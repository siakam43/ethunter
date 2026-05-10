/* ET-Bench fixture: fnptr-dynamic-call/example_5 */
/* fnptr: skp->sk_enroll, targets: sk_enroll, ssh_sk_enroll */
/* Pattern: dlopen/dlsym with internal fallback, function pointer in provider struct */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>

struct sshbuf {
    unsigned char *buf;
    size_t len;
};

struct sshkey {
    int type;
};

struct sk_enroll_response {
    uint8_t *key_handle;
    size_t key_handle_len;
};

struct sk_option {
    char *name;
    char *value;
};

struct sshsk_provider {
    char *path;
    void *dlhandle;
    int (*sk_enroll)(int, const uint8_t *, size_t, const char *, uint8_t,
                     const char *, struct sk_option **, struct sk_enroll_response **);
    int (*sk_sign)(int);
    int (*sk_load_resident_keys)(void);
};

int sshsk_enroll(int type, const char *provider_path, const char *device,
    const char *application, const char *userid, uint8_t flags,
    const char *pin, struct sshbuf *challenge_buf,
    struct sshkey **keyp, struct sshbuf *attest);

static struct sshsk_provider *
sshsk_open(const char *path)
{
    struct sshsk_provider *ret = NULL;

    if (path == NULL || *path == '\0') {
        fprintf(stderr, "No FIDO SecurityKeyProvider specified\n");
        return NULL;
    }

    ret = (struct sshsk_provider *)calloc(1, sizeof(*ret));
    if (ret == NULL) {
        fprintf(stderr, "calloc failed\n");
        return NULL;
    }

    ret->path = strdup(path);
    if (ret->path == NULL) {
        fprintf(stderr, "strdup failed\n");
        goto fail;
    }

    /* Internal provider path */
    if (strcmp(ret->path, "internal") == 0) {
        ret->sk_enroll = ssh_sk_enroll;
        ret->sk_sign = NULL;
        ret->sk_load_resident_keys = NULL;
        return ret;
    }

    /* External provider via dlopen */
    if ((ret->dlhandle = dlopen(path, RTLD_NOW)) == NULL) {
        fprintf(stderr, "Provider \"%s\" dlopen failed: %s\n", path, dlerror());
        goto fail;
    }

    if ((ret->sk_enroll = dlsym(ret->dlhandle, "sk_enroll")) == NULL) {
        fprintf(stderr, "Provider %s dlsym(sk_enroll) failed: %s\n",
            path, dlerror());
        goto fail;
    }

    return ret;

fail:
    free(ret->path);
    free(ret);
    return NULL;
}

int
sshsk_enroll(int type, const char *provider_path, const char *device,
    const char *application, const char *userid, uint8_t flags,
    const char *pin, struct sshbuf *challenge_buf,
    struct sshkey **keyp, struct sshbuf *attest)
{
    struct sshsk_provider *skp;
    int r = -1;
    int alg = type;
    const uint8_t *challenge = challenge_buf ? challenge_buf->buf : NULL;
    size_t challenge_len = challenge_buf ? challenge_buf->len : 0;
    struct sk_option **opts = NULL;
    struct sk_enroll_response *resp = NULL;

    (void)device; (void)userid; (void)attest;

    if ((skp = sshsk_open(provider_path)) == NULL) {
        r = -1;
        goto out;
    }

    if ((r = skp->sk_enroll(alg, challenge, challenge_len, application,
        flags, pin, opts, &resp)) != 0) {
        fprintf(stderr, "provider \"%s\" failure %d\n", provider_path, r);
        goto out;
    }

out:
    return r;
}

/* Target 1: used by internal provider */
int ssh_sk_enroll(int alg, const uint8_t *challenge,
    size_t challenge_len, const char *application, uint8_t flags,
    const char *pin, struct sk_option **opts,
    struct sk_enroll_response **enroll_response)
{
    (void)alg; (void)challenge; (void)challenge_len;
    (void)application; (void)flags; (void)pin;
    (void)opts; (void)enroll_response;
    return 0;
}

/* Target 2: resolved via dlsym */
int sk_enroll(int alg, const uint8_t *challenge,
    size_t challenge_len, const char *application, uint8_t flags,
    const char *pin, struct sk_option **opts,
    struct sk_enroll_response **enroll_response)
{
    (void)alg; (void)challenge; (void)challenge_len;
    (void)application; (void)flags; (void)pin;
    (void)opts; (void)enroll_response;
    return 0;
}
