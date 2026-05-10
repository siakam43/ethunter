/* ET-Bench fixture: fnptr-library/example_19 */
/* fnptr: c->output_filter, targets: sys_tun_outfilter */
/* Pattern: library channel context with output filter function pointer set via registration API */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ssh { int dummy; };

typedef struct Channel {
    int self;
    int type;
    void *output;
    size_t local_consumed;
    int (*output_filter)(struct ssh *, struct Channel *, char **, size_t *);
    int (*input_filter)(struct ssh *, struct Channel *, const char *, size_t);
    void *filter_ctx;
    void (*filter_cleanup)(void *);
} Channel;

size_t sshbuf_len(void *buf) { (void)buf; return 0; }

static int channel_handle_wfd(struct ssh *ssh, Channel *c)
{
    char *data = NULL;
    size_t dlen, olen = 0;

    olen = sshbuf_len(c->output);
    if (c->output_filter != NULL) {
        if ((data = c->output_filter(ssh, c, &data, &dlen)) == NULL) {
            fprintf(stderr, "channel %d: filter stops\n", c->self);
            return -1;
        }
    }
    c->local_consumed += olen - sshbuf_len(c->output);
    return 1;
}

void channel_register_filter(struct ssh *ssh, int id,
    int (*ifn)(struct ssh *, Channel *, const char *, size_t),
    int (*ofn)(struct ssh *, Channel *, char **, size_t *),
    void (*cfn)(void *), void *ctx)
{
    (void)ssh; (void)id;
    Channel *c = NULL;
    if (c == NULL) return;
    c->input_filter = ifn;
    c->output_filter = ofn;
    c->filter_ctx = ctx;
    c->filter_cleanup = cfn;
}

/* Target: used as output filter for tunnel forwarding */
static int sys_tun_outfilter(struct ssh *ssh, Channel *c,
                             char **data, size_t *dlen) {
    (void)ssh; (void)c; (void)data; (void)dlen;
    return 0;
}

int client_loop(struct ssh *ssh, int have_pty, int escape_char_arg,
    int ssh2_chan_id)
{
    (void)ssh; (void)have_pty; (void)escape_char_arg; (void)ssh2_chan_id;
    return 0;
}
