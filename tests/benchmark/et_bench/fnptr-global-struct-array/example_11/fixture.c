/* ET-Bench fixture: fnptr-global-struct-array/example_11 */
/* fnptr: impl->funcs->alloc, targets: ssh_rsa_alloc */
/* Pattern: SSH key impl table with per-key-type alloc function pointer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_RSA 1
#define KEY_UNSPEC 0

typedef struct sshkey {
    int type;
    int ecdsa_nid;
    void *cert;
} sshkey;

typedef struct sshcert {
    int version;
} sshcert;

static sshcert *cert_new(void) {
    return calloc(1, sizeof(sshcert));
}

typedef struct sshkey_impl_funcs {
    int (*size)(const sshkey *k);
    int (*alloc)(sshkey *k);
    void (*cleanup)(sshkey *k);
    int (*equal)(const sshkey *a, const sshkey *b);
    int (*ssh_serialize_public)(const sshkey *b, void *blob, int len);
    int (*ssh_deserialize_public)(const sshkey *b, const void *blob, int len);
    int (*ssh_serialize_private)(const sshkey *b, void *blob, int len);
    int (*ssh_deserialize_private)(const sshkey *b, const void *blob, int len);
    int (*generate)(sshkey *k, int bits);
    int (*copy_public)(sshkey *dst, const sshkey *src);
    int (*sign)(sshkey *key, void **sigp, int *lenp, const void *data, int datalen);
    int (*verify)(const sshkey *key, const void *sig, int siglen, const void *data, int datalen);
} sshkey_impl_funcs;

typedef struct sshkey_impl {
    const char *name;
    const char *shortname;
    const char *sigalg;
    int type;
    int nid;
    int cert;
    int sigonly;
    int keybits;
    const sshkey_impl_funcs *funcs;
} sshkey_impl;

/* Target function */
static int ssh_rsa_alloc(sshkey *k) {
    (void)k;
    return 0;
}

static int ssh_rsa_size(const sshkey *k) { (void)k; return 2048; }
static void ssh_rsa_cleanup(sshkey *k) { (void)k; }
static int ssh_rsa_equal(const sshkey *a, const sshkey *b) { (void)a; (void)b; return 0; }
static int ssh_rsa_serialize_public(const sshkey *b, void *blob, int len) { (void)b; (void)blob; (void)len; return 0; }
static int ssh_rsa_deserialize_public(const sshkey *b, const void *blob, int len) { (void)b; (void)blob; (void)len; return 0; }
static int ssh_rsa_serialize_private(const sshkey *b, void *blob, int len) { (void)b; (void)blob; (void)len; return 0; }
static int ssh_rsa_deserialize_private(const sshkey *b, const void *blob, int len) { (void)b; (void)blob; (void)len; return 0; }
static int ssh_rsa_generate(sshkey *k, int bits) { (void)k; (void)bits; return 0; }
static int ssh_rsa_copy_public(sshkey *dst, const sshkey *src) { (void)dst; (void)src; return 0; }
static int ssh_rsa_sign(sshkey *key, void **sigp, int *lenp, const void *data, int datalen) { (void)key; (void)sigp; (void)lenp; (void)data; (void)datalen; return 0; }
static int ssh_rsa_verify(const sshkey *key, const void *sig, int siglen, const void *data, int datalen) { (void)key; (void)sig; (void)siglen; (void)data; (void)datalen; return 0; }

static const sshkey_impl_funcs sshkey_rsa_funcs = {
    ssh_rsa_size,
    ssh_rsa_alloc,
    ssh_rsa_cleanup,
    ssh_rsa_equal,
    ssh_rsa_serialize_public,
    ssh_rsa_deserialize_public,
    ssh_rsa_serialize_private,
    ssh_rsa_deserialize_private,
    ssh_rsa_generate,
    ssh_rsa_copy_public,
    ssh_rsa_sign,
    ssh_rsa_verify,
};

static const sshkey_impl sshkey_rsa_impl = {
    "ssh-rsa", "RSA", NULL,
    KEY_RSA, 0, 0, 0, 0,
    &sshkey_rsa_funcs,
};

/* Global impl table */
static const sshkey_impl *const keyimpls[] = {
    &sshkey_rsa_impl,
    NULL
};

/* Helpers */
static int sshkey_impl_from_type(int type) {
    int i;
    for (i = 0; keyimpls[i] != NULL; i++) {
        if (keyimpls[i]->type == type)
            return i;
    }
    return -1;
}

static int sshkey_is_cert(const sshkey *k) {
    return keyimpls[0]->cert;
}

/* Caller: impl->funcs->alloc */
int sshkey_new(int type) {
    sshkey k;
    int impl_idx;

    if (type != KEY_UNSPEC &&
        (impl_idx = sshkey_impl_from_type(type)) < 0)
        return -1;

    memset(&k, 0, sizeof(k));
    k.type = type;
    k.ecdsa_nid = -1;
    if (keyimpls[impl_idx] != NULL && keyimpls[impl_idx]->funcs->alloc != NULL) {
        if (keyimpls[impl_idx]->funcs->alloc(&k) != 0) {
            return -1;
        }
    }
    if (sshkey_is_cert(&k)) {
        void *cert = cert_new();
        if (cert == NULL) {
            return -1;
        }
        k.cert = cert;
    }

    return 0;
}
