/* ET-Bench fixture: fnptr-library/example_20 */
/* fnptr: c->open_confirm, targets: mux_session_confirm, mux_stdio_confirm, ssh_stdio_confirm, ssh_session2_setup, ssh_tun_confirm */
/* Pattern: library channel context with open-confirm callback set via registration API */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int u_int;
typedef unsigned int u_int32_t;

struct ssh { int dummy; };

typedef struct Channel {
    int self;
    int type;
    void (*open_confirm)(struct ssh *, int, u_int32_t, void *);
    void *open_confirm_ctx;
} Channel;

#define SSH_CHANNEL_OPENING 1

int sshpkt_get_u32(struct ssh *ssh, u_int32_t *val) { (void)ssh; *val = 0; return 0; }
int sshpkt_get_cstring(struct ssh *ssh, char **str, void *p) { (void)ssh; *str = NULL; (void)p; return 0; }
int sshpkt_get_string_direct(struct ssh *ssh, void *p, void *p2) { (void)ssh; (void)p; (void)p2; return 0; }
int sshpkt_get_end(struct ssh *ssh) { (void)ssh; return 0; }
int sshpkt_fatal(struct ssh *ssh, int r, const char *fmt, ...) { (void)ssh; (void)r; (void)fmt; return 0; }
int sshpkt_start(struct ssh *ssh, int type) { (void)ssh; (void)type; return 0; }
int sshpkt_put_cstring(struct ssh *ssh, const char *str) { (void)ssh; (void)str; return 0; }
int sshpkt_put_u32(struct ssh *ssh, u_int32_t val) { (void)ssh; (void)val; return 0; }
int sshpkt_send(struct ssh *ssh) { (void)ssh; return 0; }
void logit(const char *fmt, ...) { (void)fmt; }
void debug2(const char *fmt, ...) { (void)fmt; }
void chan_mark_dead(struct ssh *ssh, Channel *c) { (void)ssh; (void)c; }
void free(void *p) { (void)p; }

static Channel *channel_from_packet_id(struct ssh *ssh, const char *func, const char *msg)
{
    (void)ssh; (void)func; (void)msg;
    return NULL;
}

int channel_input_open_failure(int type, u_int32_t seq, struct ssh *ssh)
{
    Channel *c = channel_from_packet_id(ssh, __func__, "open failure");
    u_int32_t reason;
    char *msg = NULL;
    int r;

    if (c == NULL) return 0;
    if (c->type != SSH_CHANNEL_OPENING)
        return 0;
    if ((r = sshpkt_get_u32(ssh, &reason)) != 0)
        return 0;
    if ((r = sshpkt_get_cstring(ssh, &msg, NULL)) != 0 ||
        (r = sshpkt_get_string_direct(ssh, NULL, NULL)) != 0 ||
        (r = sshpkt_get_end(ssh)) != 0)
        return 0;

    logit("channel %d: open failed: %s", c->self, "unknown");
    if (msg) free(msg);

    if (c->open_confirm) {
        debug2("channel %d: callback start\n", c->self);
        c->open_confirm(ssh, c->self, 0, c->open_confirm_ctx);
        debug2("channel %d: callback done\n", c->self);
    }
    chan_mark_dead(ssh, c);
    return 0;
}

void channel_register_open_confirm(struct ssh *ssh, int id,
    void (*fn)(struct ssh *, int, u_int32_t, void *), void *ctx)
{
    (void)ssh; (void)id;
    Channel *c = NULL;
    if (c == NULL) return;
    c->open_confirm = fn;
    c->open_confirm_ctx = ctx;
}

/* Target 1: mux session confirm */
static void mux_session_confirm(struct ssh *ssh, int id, u_int32_t seq, void *ctx) {
    (void)ssh; (void)id; (void)seq; (void)ctx;
}

/* Target 2: mux stdio forward confirm */
static void mux_stdio_confirm(struct ssh *ssh, int id, u_int32_t seq, void *ctx) {
    (void)ssh; (void)id; (void)seq; (void)ctx;
}

/* Target 3: ssh stdio forward confirm */
static void ssh_stdio_confirm(struct ssh *ssh, int id, u_int32_t seq, void *ctx) {
    (void)ssh; (void)id; (void)seq; (void)ctx;
}

/* Target 4: ssh session setup */
static void ssh_session2_setup(struct ssh *ssh, int id, u_int32_t seq, void *ctx) {
    (void)ssh; (void)id; (void)seq; (void)ctx;
}

/* Target 5: ssh tunnel confirm */
static void ssh_tun_confirm(struct ssh *ssh, int id, u_int32_t seq, void *ctx) {
    (void)ssh; (void)id; (void)seq; (void)ctx;
}
