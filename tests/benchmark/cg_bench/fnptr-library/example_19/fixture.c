/* CG-Bench fixture: fnptr-library/example_19 */
/* fnptr: c->output_filter, targets: sys_tun_outfilter */

static int
channel_handle_wfd(struct ssh *ssh, Channel *c)
{
	u_char *data = NULL, *buf; /* XXX const; need filter API change */
	size_t dlen, olen = 0;
	int r, len;

	/* Send buffered output data to the socket. */
	olen = sshbuf_len(c->output);
	if (c->output_filter != NULL) {
		if ((buf = c->output_filter(ssh, c, &data, &dlen)) == NULL) {
			debug2("channel %d: filter stops", c->self);
			if (c->type != SSH_CHANNEL_OPEN)
				chan_mark_dead(ssh, c);
			else
				chan_write_failed(ssh, c);
			return -1;
		}
	}

 out:
	c->local_consumed += olen - sshbuf_len(c->output);
	return 1;
}

void
channel_register_filter(struct ssh *ssh, int id, channel_infilter_fn *ifn,
    channel_outfilter_fn *ofn, channel_filter_cleanup_fn *cfn, void *ctx)
{
	Channel *c = channel_lookup(ssh, id);

	if (c == NULL) {
		logit_f("%d: bad id", id);
		return;
	}
	c->input_filter = ifn;
	c->output_filter = ofn;
	c->filter_ctx = ctx;
	c->filter_cleanup = cfn;
}

int
client_loop(struct ssh *ssh, int have_pty, int escape_char_arg,
    int ssh2_chan_id)
{
	if (session_ident != -1) {
		if (escape_char_arg != SSH_ESCAPECHAR_NONE) {
			channel_register_filter(ssh, session_ident,
			    client_simple_escape_filter, NULL,
			    client_filter_cleanup,
			    client_new_escape_filter_ctx(
			    escape_char_arg));
		}
	}

	return exit_status;
}

char *
client_request_tun_fwd(struct ssh *ssh, int tun_mode,
    int local_tun, int remote_tun, channel_open_fn *cb, void *cbctx)
{
	Channel *c;
	int r, fd;
	char *ifname = NULL;

	if (tun_mode == SSH_TUNMODE_NO)
		return 0;

	debug("Requesting tun unit %d in mode %d", local_tun, tun_mode);

	/* Open local tunnel device */
	if ((fd = tun_open(local_tun, tun_mode, &ifname)) == -1) {
		error("Tunnel device open failed.");
		return NULL;
	}
	debug("Tunnel forwarding using interface %s", ifname);

	c = channel_new(ssh, "tun-connection", SSH_CHANNEL_OPENING, fd, fd, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0, "tun", 1);
	c->datagram = 1;

#if defined(SSH_TUN_FILTER)
	if (options.tun_open == SSH_TUNMODE_POINTOPOINT)
		channel_register_filter(ssh, c->self, sys_tun_infilter,
		    sys_tun_outfilter, NULL, NULL);
#endif

	if (cb != NULL)
		channel_register_open_confirm(ssh, c->self, cb, cbctx);

	if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_OPEN)) != 0 ||
	    (r = sshpkt_put_cstring(ssh, "tun@openssh.com")) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->self)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->local_window_max)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->local_maxpacket)) != 0 ||
	    (r = sshpkt_put_u32(ssh, tun_mode)) != 0 ||
	    (r = sshpkt_put_u32(ssh, remote_tun)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		sshpkt_fatal(ssh, r, "%s: send reply", __func__);

	return ifname;
}


/* Stub implementation for sys_tun_outfilter */
void sys_tun_outfilter(void) {}
