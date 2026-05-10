/* ET-Bench fixture: fnptr-global-struct-array/example_12 */
/* fnptr: mux_master_handlers[i].handler, targets: mux_master_process_hello, mux_master_process_new_session, mux_master_process_alive_check, mux_master_process_terminate, mux_master_process_open_fwd, mux_master_process_close_fwd, mux_master_process_stdio_fwd, mux_master_process_stop_listening, mux_master_process_proxy */
/* Pattern: SSH mux handler dispatch table with per-message-type handler function pointer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MUX_MSG_HELLO           0x10000000
#define MUX_C_NEW_SESSION       0x10000001
#define MUX_C_ALIVE_CHECK       0x10000002
#define MUX_C_TERMINATE         0x10000003
#define MUX_C_OPEN_FWD          0x10000004
#define MUX_C_CLOSE_FWD         0x10000005
#define MUX_C_NEW_STDIO_FWD     0x10000006
#define MUX_C_STOP_LISTENING    0x10000007
#define MUX_C_PROXY             0x10000008

#define MUX_S_FAILURE           1

struct sshbuf;
typedef struct sshbuf sshbuf;

struct ssh;
typedef struct ssh ssh;

typedef struct Channel {
    int type;
    int self;
} Channel;

/* Handler function pointer type */
typedef int (*mux_handler_fn)(struct ssh *, unsigned int, Channel *,
    struct sshbuf *, struct sshbuf *);

/* Dispatch table entry */
typedef struct {
    unsigned int type;
    mux_handler_fn handler;
} mux_handler_entry;

/* Target handler functions */
static int mux_master_process_hello(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

static int mux_master_process_new_session(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

static int mux_master_process_alive_check(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

static int mux_master_process_terminate(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

static int mux_master_process_open_fwd(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

static int mux_master_process_close_fwd(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

static int mux_master_process_stdio_fwd(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

static int mux_master_process_stop_listening(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

static int mux_master_process_proxy(ssh *ssh, unsigned int rid, Channel *c,
    sshbuf *in, sshbuf *out) {
    (void)ssh; (void)rid; (void)c; (void)in; (void)out;
    return 0;
}

/* Global handler dispatch table */
static const mux_handler_entry mux_master_handlers[] = {
    { MUX_MSG_HELLO,        mux_master_process_hello },
    { MUX_C_NEW_SESSION,    mux_master_process_new_session },
    { MUX_C_ALIVE_CHECK,    mux_master_process_alive_check },
    { MUX_C_TERMINATE,      mux_master_process_terminate },
    { MUX_C_OPEN_FWD,       mux_master_process_open_fwd },
    { MUX_C_CLOSE_FWD,      mux_master_process_close_fwd },
    { MUX_C_NEW_STDIO_FWD,  mux_master_process_stdio_fwd },
    { MUX_C_STOP_LISTENING, mux_master_process_stop_listening },
    { MUX_C_PROXY,          mux_master_process_proxy },
    { 0, NULL }
};

static void error_f(const char *fmt, ...) {
    (void)fmt;
}

static void reply_error(sshbuf *out, int code, unsigned int rid, const char *msg) {
    (void)out; (void)code; (void)rid; (void)msg;
}

/* Caller: mux_master_handlers[i].handler */
static int mux_master_read_cb(ssh *ssh, Channel *c) {
    sshbuf *in = NULL, *out = NULL;
    unsigned int type, rid, i;
    int ret = -1;

    /* In real code, type and rid are parsed from the buffer */
    type = MUX_C_ALIVE_CHECK;
    rid = 0;

    for (i = 0; mux_master_handlers[i].handler != NULL; i++) {
        if (type == mux_master_handlers[i].type) {
            ret = mux_master_handlers[i].handler(ssh, rid, c, in, out);
            break;
        }
    }
    if (mux_master_handlers[i].handler == NULL) {
        error_f("unsupported mux message 0x%08x", type);
        reply_error(out, MUX_S_FAILURE, rid, "unsupported request");
        ret = 0;
    }

    return ret;
}
