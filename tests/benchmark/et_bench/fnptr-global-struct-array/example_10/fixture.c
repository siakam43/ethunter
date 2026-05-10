/* ET-Bench fixture: fnptr-global-struct-array/example_10 */
/* fnptr: impl->funcs->size, targets: ssh_rsa_size */
/* Pattern: SSH key impl table with per-key-type size function pointer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_RSA 1
#define KEY_UNSPEC 0

#define KEY_ED25519 10
#define KEY_ED25519_CERT 11
#define KEY_ECDSA_NISTP256 20
#define KEY_ECDSA_NISTP256_CERT 21
#define KEY_DSS 30
#define KEY_DSA_CERT 31
#define KEY_RSA_CERT 2
#define KEY_RSA_SHA256 3
#define KEY_RSA_SHA256_CERT 4
#define KEY_RSA_SHA512 5
#define KEY_RSA_SHA512_CERT 6

typedef unsigned int u_int;

typedef struct sshkey {
    int type;
    int ecdsa_nid;
    void *cert;
} sshkey;

typedef struct sshkey_impl_funcs {
    u_int (*size)(const sshkey *k);
    int (*alloc)(sshkey *k);
    void (*cleanup)(sshkey *k);
    int (*equal)(const sshkey *a, const sshkey *b);
    int (*ssh_serialize_public)(const sshkey *b, void *blob, size_t *len);
    int (*ssh_deserialize_public)(const sshkey *b, const void *blob, size_t *len);
    int (*ssh_serialize_private)(const sshkey *b, void *blob, size_t *len);
    int (*ssh_deserialize_private)(const sshkey *b, const void *blob, size_t *len);
    int (*generate)(sshkey *k, int bits);
    int (*copy_public)(sshkey *dst, const sshkey *src);
    int (*sign)(sshkey *key, void **sigp, size_t *lenp, const void *data, size_t datalen);
    int (*verify)(const sshkey *key, const void *sig, size_t siglen, const void *data, size_t datalen);
} sshkey_impl_funcs;

typedef struct sshkey_impl {
    const char *name;
    const char *shortname;
    const char *sigalg;
    int type;
    int nid;
    int cert;
    int sigonly;
    u_int keybits;
    const sshkey_impl_funcs *funcs;
} sshkey_impl;

/* Target function */
static u_int ssh_rsa_size(const sshkey *k) {
    (void)k;
    return 2048;
}

static int ssh_rsa_alloc(sshkey *k) { (void)k; return 0; }
static void ssh_rsa_cleanup(sshkey *k) { (void)k; }
static int ssh_rsa_equal(const sshkey *a, const sshkey *b) { (void)a; (void)b; return 0; }
static int ssh_rsa_serialize_public(const sshkey *b, void *blob, size_t *len) { (void)b; (void)blob; (void)len; return 0; }
static int ssh_rsa_deserialize_public(const sshkey *b, const void *blob, size_t *len) { (void)b; (void)blob; (void)len; return 0; }
static int ssh_rsa_serialize_private(const sshkey *b, void *blob, size_t *len) { (void)b; (void)blob; (void)len; return 0; }
static int ssh_rsa_deserialize_private(const sshkey *b, const void *blob, size_t *len) { (void)b; (void)blob; (void)len; return 0; }
static int ssh_rsa_generate(sshkey *k, int bits) { (void)k; (void)bits; return 0; }
static int ssh_rsa_copy_public(sshkey *dst, const sshkey *src) { (void)dst; (void)src; return 0; }
static int ssh_rsa_sign(sshkey *key, void **sigp, size_t *lenp, const void *data, size_t datalen) { (void)key; (void)sigp; (void)lenp; (void)data; (void)datalen; return 0; }
static int ssh_rsa_verify(const sshkey *key, const void *sig, size_t siglen, const void *data, size_t datalen) { (void)key; (void)sig; (void)siglen; (void)data; (void)datalen; return 0; }

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
static const sshkey_impl *sshkey_impl_from_type_nid(int type, int nid) {
    int i;
    for (i = 0; keyimpls[i] != NULL; i++) {
        if (keyimpls[i]->type == type &&
            (keyimpls[i]->nid == 0 || keyimpls[i]->nid == nid))
            return keyimpls[i];
    }
    return NULL;
}

static const sshkey_impl *sshkey_impl_from_key(const sshkey *k) {
    if (k == NULL)
        return NULL;
    return sshkey_impl_from_type_nid(k->type, k->ecdsa_nid);
}

/* Caller: impl->funcs->size */
u_int sshkey_size(const sshkey *k) {
    const sshkey_impl *impl;

    if ((impl = sshkey_impl_from_key(k)) == NULL)
        return 0;
    if (impl->funcs->size != NULL)
        return impl->funcs->size(k);
    return impl->keybits;
}
