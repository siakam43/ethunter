/* CG-Bench fixture: fnptr-library/example_4 */
/* fnptr: c->input_filter, targets: client_simple_escape_filter, sys_tun_infilter */

static int channel_handle_rfd(struct ssh *ssh, Channel *c)
{
	...
	if (c->input_filter != NULL) {
		if (c->input_filter(ssh, c, buf, len) == -1) {
			debug2("channel %d: filter stops", c->self);
			chan_read_failed(ssh, c);
		}
        ...
    }
    ...
}

void channel_register_filter(struct ssh *ssh, int id, channel_infilter_fn *ifn,
    channel_outfilter_fn *ofn, channel_filter_cleanup_fn *cfn, void *ctx)
{
	...
	c->input_filter = ifn;
	c->output_filter = ofn;
	c->filter_ctx = ctx;
	c->filter_cleanup = cfn;
}

static int mux_master_process_new_session(struct ssh *ssh, u_int rid,
    Channel *c, struct sshbuf *m, struct sshbuf *reply)
{
	...
	if (cctx->want_tty && escape_char != 0xffffffff) {
		channel_register_filter(ssh, nc->self,
		    client_simple_escape_filter, NULL,
		    client_filter_cleanup,
		    client_new_escape_filter_ctx((int)escape_char));
	}
    ...
}

char *
client_request_tun_fwd(struct ssh *ssh, int tun_mode,
    int local_tun, int remote_tun, channel_open_fn *cb, void *cbctx)
{
    ...

#if defined(SSH_TUN_FILTER)
	if (options.tun_open == SSH_TUNMODE_POINTOPOINT)
		channel_register_filter(ssh, c->self, sys_tun_infilter,
		    sys_tun_outfilter, NULL, NULL);
#endif
    ...
}


/* Stub implementation for client_simple_escape_filter */
void client_simple_escape_filter(void) {}



/* Stub implementation for sys_tun_infilter */
void sys_tun_infilter(void) {}
