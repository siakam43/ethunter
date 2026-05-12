/* ET-Bench fixture: fnptr-library/example_4 */
/* fnptr: c->input_filter, targets: client_simple_escape_filter, sys_tun_infilter */
/* Pattern: library context with filter function pointer set via registration API */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ssh { int dummy; };
typedef unsigned int u_int;

typedef struct Channel {
    int self;
    int (*input_filter)(struct ssh *, struct Channel *, const char *, size_t);
    int (*output_filter)(struct ssh *, struct Channel *, char **, size_t *);
    void *filter_ctx;
    void (*filter_cleanup)(void *);
} Channel;

static int channel_handle_rfd(struct ssh *ssh, Channel *c, const char *buf, size_t len)
{
    if (c->input_filter != NULL) {
        if (c->input_filter(ssh, c, buf, len) == -1) {
            fprintf(stderr, "channel %d: filter stops\n", c->self);
            return -1;
        }
    }
    return 0;
}

void channel_register_filter(struct ssh *ssh, int id,
    int (*ifn)(struct ssh *, Channel *, const char *, size_t),
    int (*ofn)(struct ssh *, Channel *, char **, size_t *),
    void (*cfn)(void *), void *ctx)
{
    (void)ssh; (void)id;
    Channel c_storage; Channel *c = &c_storage;
    c->input_filter = ifn;
    c->output_filter = ofn;
    c->filter_ctx = ctx;
    c->filter_cleanup = cfn;
}

/* Target 1: client escape filter */
static int client_simple_escape_filter(struct ssh *ssh, Channel *c,
                                       const char *buf, size_t len) {
    (void)ssh; (void)c; (void)buf; (void)len;
    return 0;
}

/* Target 2: system tunnel input filter */
static int sys_tun_infilter(struct ssh *ssh, Channel *c,
                            const char *buf, size_t len) {
    (void)ssh; (void)c; (void)buf; (void)len;
    return 0;
}

void client_filter_cleanup(void *ctx) { (void)ctx; }
void *client_new_escape_filter_ctx(int escape_char) {
    (void)escape_char;
    return NULL;
}

void register_all_filters(void) {
    channel_register_filter(((void *)0), 0, client_simple_escape_filter, ((void *)0), ((void *)0), ((void *)0));
    channel_register_filter(((void *)0), 1, sys_tun_infilter, ((void *)0), ((void *)0), ((void *)0));
}
