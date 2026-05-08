/* CG-Bench fixture: fnptr-library/example_20 */
/* fnptr: c->open_confirm, targets: mux_session_confirm, mux_stdio_confirm, ssh_stdio_confirm, ssh_session2_setup, ssh_tun_confirm */

int
channel_input_open_failure(int type, u_int32_t seq, struct ssh *ssh)
{
	Channel *c = channel_from_packet_id(ssh, __func__, "open failure");
	u_int32_t reason;
	char *msg = NULL;
	int r;

	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;
	if (c->type != SSH_CHANNEL_OPENING)
		ssh_packet_disconnect(ssh, "Received open failure for "
		    "non-opening channel %d.", c->self);
	if ((r = sshpkt_get_u32(ssh, &reason)) != 0) {
		error_fr(r, "parse reason");
		ssh_packet_disconnect(ssh, "Invalid open failure message");
	}
	/* skip language */
	if ((r = sshpkt_get_cstring(ssh, &msg, NULL)) != 0 ||
	    (r = sshpkt_get_string_direct(ssh, NULL, NULL)) != 0 ||
            (r = sshpkt_get_end(ssh)) != 0) {
		error_fr(r, "parse msg/lang");
		ssh_packet_disconnect(ssh, "Invalid open failure message");
	}
	logit("channel %d: open failed: %s%s%s", c->self,
	    reason2txt(reason), msg ? ": ": "", msg ? msg : "");
	free(msg);
	if (c->open_confirm) {
		debug2_f("channel %d: callback start", c->self);
		c->open_confirm(ssh, c->self, 0, c->open_confirm_ctx);
		debug2_f("channel %d: callback done", c->self);
	}
	/* Schedule the channel for cleanup/deletion. */
	chan_mark_dead(ssh, c);
	return 0;
}

void
channel_register_open_confirm(struct ssh *ssh, int id,
    channel_open_fn *fn, void *ctx)
{
	Channel *c = channel_lookup(ssh, id);

	if (c == NULL) {
		logit_f("%d: bad id", id);
		return;
	}
	c->open_confirm = fn;
	c->open_confirm_ctx = ctx;
}

char *
client_request_tun_fwd(struct ssh *ssh, int tun_mode,
    int local_tun, int remote_tun, channel_open_fn *cb, void *cbctx)
{
	if (cb != NULL)
		channel_register_open_confirm(ssh, c->self, cb, cbctx);
}

static int
mux_master_process_new_session(struct ssh *ssh, u_int rid,
    Channel *c, struct sshbuf *m, struct sshbuf *reply)
{
	channel_register_open_confirm(ssh, nc->self, mux_session_confirm, cctx);
}

static int
mux_master_process_stdio_fwd(struct ssh *ssh, u_int rid,
    Channel *c, struct sshbuf *m, struct sshbuf *reply)
{
	channel_register_open_confirm(ssh, nc->self, mux_stdio_confirm, cctx);
}

static void
ssh_init_stdio_forwarding(struct ssh *ssh)
{
	Channel *c;
	int in, out;

	if (options.stdio_forward_host == NULL)
		return;

	debug3_f("%s:%d", options.stdio_forward_host,
	    options.stdio_forward_port);

	if ((in = dup(STDIN_FILENO)) == -1 ||
	    (out = dup(STDOUT_FILENO)) == -1)
		fatal_f("dup() in/out failed");
	if ((c = channel_connect_stdio_fwd(ssh, options.stdio_forward_host,
	    options.stdio_forward_port, in, out,
	    CHANNEL_NONBLOCK_STDIO)) == NULL)
		fatal_f("channel_connect_stdio_fwd failed");
	channel_register_cleanup(ssh, c->self, client_cleanup_stdio_fwd, 0);
	channel_register_open_confirm(ssh, c->self, ssh_stdio_confirm, NULL);
}

static int
ssh_session2_open(struct ssh *ssh)
{
	Channel *c;
	int window, packetmax, in, out, err;

	if (options.stdin_null) {
		in = open(_PATH_DEVNULL, O_RDONLY);
	} else {
		in = dup(STDIN_FILENO);
	}
	out = dup(STDOUT_FILENO);
	err = dup(STDERR_FILENO);

	if (in == -1 || out == -1 || err == -1)
		fatal("dup() in/out/err failed");

	window = CHAN_SES_WINDOW_DEFAULT;
	packetmax = CHAN_SES_PACKET_DEFAULT;
	if (tty_flag) {
		window >>= 1;
		packetmax >>= 1;
	}
	c = channel_new(ssh,
	    "session", SSH_CHANNEL_OPENING, in, out, err,
	    window, packetmax, CHAN_EXTENDED_WRITE,
	    "client-session", CHANNEL_NONBLOCK_STDIO);

	debug3_f("channel_new: %d", c->self);

	channel_send_open(ssh, c->self);
	if (options.session_type != SESSION_TYPE_NONE)
		channel_register_open_confirm(ssh, c->self,
		    ssh_session2_setup, NULL);

	return c->self;
}

static void
ssh_init_forwarding(struct ssh *ssh, char **ifname)
{
	/* Initiate tunnel forwarding. */
	if (options.tun_open != SSH_TUNMODE_NO) {
		if ((*ifname = client_request_tun_fwd(ssh,
		    options.tun_open, options.tun_local,
		    options.tun_remote, ssh_tun_confirm, NULL)) != NULL)
			forward_confirms_pending++;
	}
}


/* Stub implementation for mux_session_confirm */
void mux_session_confirm(void) {}



/* Stub implementation for mux_stdio_confirm */
void mux_stdio_confirm(void) {}



/* Stub implementation for ssh_stdio_confirm */
void ssh_stdio_confirm(void) {}



/* Stub implementation for ssh_session2_setup */
void ssh_session2_setup(void) {}



/* Stub implementation for ssh_tun_confirm */
void ssh_tun_confirm(void) {}
