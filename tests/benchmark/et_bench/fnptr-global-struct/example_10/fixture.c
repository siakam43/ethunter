/* et_bench fixture: fnptr-global-struct/example_10 */
/* fnptr: sshkey_ecdsa_funcs.equal, targets: ssh_ecdsa_equal */

#include <stddef.h>
#include <stdint.h>

typedef uint32_t u_int;
typedef uint8_t u_char;

struct sshbuf { char *buf; size_t len; size_t cap; };
struct sshkey {
    int type;
    void *ecdsa;
    void *sk_application;
    void *sk_key_handle;
    void *sk_reserved;
    u_char *sk_ed25519_pk;
};

enum sshkey_serialize_rep { SSHKEY_SERIALIZE_DEFAULT = 0 };

struct sshkey_impl_funcs {
    u_int (*size)(const struct sshkey *);
    int (*alloc)(struct sshkey *);
    void (*cleanup)(struct sshkey *);
    int (*equal)(const struct sshkey *, const struct sshkey *);
    int (*serialize_public)(const struct sshkey *, struct sshbuf *,
                            enum sshkey_serialize_rep);
    int (*deserialize_public)(const char *, struct sshbuf *, struct sshkey *);
    int (*serialize_private)(const struct sshkey *, struct sshbuf *,
                             enum sshkey_serialize_rep);
    int (*deserialize_private)(const char *, struct sshbuf *, struct sshkey *);
    int (*generate)(struct sshkey *, int);
    int (*copy_public)(const struct sshkey *, struct sshkey *);
    int (*sign)(struct sshkey *, u_char **, size_t *, const u_char *, size_t,
                const char *, const char *, const char *, u_int);
    int (*verify)(const struct sshkey *, const u_char *, size_t,
                  const u_char *, size_t, const char *, u_int, void **);
};

/* Target: ssh_ecdsa_equal */
int ssh_ecdsa_equal(const struct sshkey *a, const struct sshkey *b)
{
    return 1;
}

/* Other ECDSA ops */
u_int ssh_ecdsa_size(const struct sshkey *k) { return 0; }
void ssh_ecdsa_cleanup(struct sshkey *k) {}
int ssh_ecdsa_serialize_public(const struct sshkey *k, struct sshbuf *b,
                                enum sshkey_serialize_rep r) { return 0; }
int ssh_ecdsa_deserialize_public(const char *s, struct sshbuf *b,
                                  struct sshkey *k) { return 0; }
int ssh_ecdsa_serialize_private(const struct sshkey *k, struct sshbuf *b,
                                 enum sshkey_serialize_rep r) { return 0; }
int ssh_ecdsa_deserialize_private(const char *s, struct sshbuf *b,
                                   struct sshkey *k) { return 0; }
int ssh_ecdsa_generate(struct sshkey *k, int bits) { return 0; }
int ssh_ecdsa_copy_public(const struct sshkey *src, struct sshkey *dst) { return 0; }
int ssh_ecdsa_sign(struct sshkey *k, u_char **sig, size_t *slen,
                    const u_char *data, size_t dlen, const char *alg,
                    const char *sk, const char *hk, u_int compat) { return 0; }
int ssh_ecdsa_verify(const struct sshkey *k, const u_char *sig, size_t slen,
                      const u_char *data, size_t dlen, const char *alg,
                      u_int compat, void **detailsp) { return 0; }

/* Global const struct with function pointer members */
const struct sshkey_impl_funcs sshkey_ecdsa_funcs = {
    .size = ssh_ecdsa_size,
    .alloc = NULL,
    .cleanup = ssh_ecdsa_cleanup,
    .equal = ssh_ecdsa_equal,
    .serialize_public = ssh_ecdsa_serialize_public,
    .deserialize_public = ssh_ecdsa_deserialize_public,
    .serialize_private = ssh_ecdsa_serialize_private,
    .deserialize_private = ssh_ecdsa_deserialize_private,
    .generate = ssh_ecdsa_generate,
    .copy_public = ssh_ecdsa_copy_public,
    .sign = ssh_ecdsa_sign,
    .verify = ssh_ecdsa_verify,
};

/* Caller: uses sshkey_ecdsa_funcs.equal */
int sshkey_sk_fields_equal(const struct sshkey *a, const struct sshkey *b) { return 1; }

int ssh_ecdsa_sk_equal(const struct sshkey *a, const struct sshkey *b)
{
    if (!sshkey_sk_fields_equal(a, b))
        return 0;
    if (!sshkey_ecdsa_funcs.equal(a, b))
        return 0;
    return 1;
}
