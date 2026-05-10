/* ET-Bench fixture: fnptr-library/example_18 */
/* fnptr: kex->verify_host_key, targets: key_print_wrapper, _ssh_verify_host_key */
/* Pattern: library KEX context with verify function pointer set via callback API */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SSH_ERR_INVALID_ARGUMENT (-1)
#define SSH_ERR_KEY_TYPE_MISMATCH (-2)
#define SSH_ERR_SIGNATURE_INVALID (-3)
#define SSH_ERR_ALLOC_FAIL (-4)
#define KEY_ECDSA 5

typedef struct sshkey {
    int type;
    int ecdsa_nid;
} sshkey;

typedef struct kex {
    int server;
    int hostkey_type;
    int hostkey_nid;
    int (*verify_host_key)(struct sshkey *, struct ssh *);
    void (*kex)(void);
} kex;

typedef struct ssh {
    kex *kex;
} ssh;

static int key_print_wrapper(sshkey *hostkey, ssh *ssh_ctx);
static int _ssh_verify_host_key(sshkey *hostkey, ssh *ssh_ctx);

int kex_verify_host_key(ssh *ssh_ctx, sshkey *server_host_key)
{
    kex *k = ssh_ctx->kex;

    if (k->verify_host_key == NULL) {
        fprintf(stderr, "missing hostkey verifier\n");
        return SSH_ERR_INVALID_ARGUMENT;
    }
    if (server_host_key->type != k->hostkey_type ||
        (k->hostkey_type == KEY_ECDSA &&
         server_host_key->ecdsa_nid != k->hostkey_nid))
        return SSH_ERR_KEY_TYPE_MISMATCH;

    if (k->verify_host_key(server_host_key, ssh_ctx) == -1)
        return SSH_ERR_SIGNATURE_INVALID;
    return 0;
}

int ssh_init(ssh **sshp, int is_server, void *kex_params)
{
    ssh *ssh_ctx;

    ssh_ctx = (ssh *)calloc(1, sizeof(*ssh_ctx));
    if (ssh_ctx == NULL)
        return SSH_ERR_ALLOC_FAIL;

    ssh_ctx->kex = (kex *)calloc(1, sizeof(kex));
    if (ssh_ctx->kex == NULL) {
        free(ssh_ctx);
        return SSH_ERR_ALLOC_FAIL;
    }

    ssh_ctx->kex->server = is_server;
    if (is_server) {
        ssh_ctx->kex->verify_host_key = _ssh_verify_host_key;
    }

    *sshp = ssh_ctx;
    (void)kex_params;
    return 0;
}

int _ssh_verify_host_key(sshkey *hostkey, ssh *ssh_ctx)
{
    (void)hostkey; (void)ssh_ctx;
    return 0;
}

int ssh_set_verify_host_key_callback(ssh *ssh_ctx,
    int (*cb)(sshkey *, ssh *))
{
    if (cb == NULL || ssh_ctx->kex == NULL)
        return SSH_ERR_INVALID_ARGUMENT;
    ssh_ctx->kex->verify_host_key = cb;
    return 0;
}

static int key_print_wrapper(sshkey *hostkey, ssh *ssh_ctx)
{
    (void)hostkey; (void)ssh_ctx;
    return -1;
}
