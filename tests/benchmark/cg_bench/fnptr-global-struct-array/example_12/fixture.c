/* CG-Bench fixture: fnptr-global-struct-array/example_12 */
/* fnptr: mux_master_handlers[i].handler, targets: mux_master_process_hello, mux_master_process_new_session, mux_master_process_alive_check, mux_master_process_terminate, mux_master_process_open_fwd, mux_master_process_close_fwd, mux_master_process_stdio_fwd, mux_master_process_stop_listening, mux_master_process_proxy */

/* Channel callbacks fired on read/write from mux client fd */
static int
mux_master_read_cb(struct ssh *ssh, Channel *c)
{
	struct sshbuf *in = NULL, *out = NULL;
	u_int type, rid, i;
	int r, ret = -1;

	for (i = 0; mux_master_handlers[i].handler != NULL; i++) {
		if (type == mux_master_handlers[i].type) {
			ret = mux_master_handlers[i].handler(ssh, rid,
			    c, in, out);
			break;
		}
	}
	if (mux_master_handlers[i].handler == NULL) {
		error_f("unsupported mux message 0x%08x", type);
		reply_error(out, MUX_S_FAILURE, rid, "unsupported request");
		ret = 0;
	}

 out:
	sshbuf_free(in);
	sshbuf_free(out);
	return ret;
}

static const struct {
	u_int type;
	int (*handler)(struct ssh *, u_int, Channel *,
	    struct sshbuf *, struct sshbuf *);
} mux_master_handlers[] = {
	{ MUX_MSG_HELLO, mux_master_process_hello },
	{ MUX_C_NEW_SESSION, mux_master_process_new_session },
	{ MUX_C_ALIVE_CHECK, mux_master_process_alive_check },
	{ MUX_C_TERMINATE, mux_master_process_terminate },
	{ MUX_C_OPEN_FWD, mux_master_process_open_fwd },
	{ MUX_C_CLOSE_FWD, mux_master_process_close_fwd },
	{ MUX_C_NEW_STDIO_FWD, mux_master_process_stdio_fwd },
	{ MUX_C_STOP_LISTENING, mux_master_process_stop_listening },
	{ MUX_C_PROXY, mux_master_process_proxy },
	{ 0, NULL }
};


/* Stub implementation for mux_master_process_hello */
void mux_master_process_hello(void) {}



/* Stub implementation for mux_master_process_new_session */
void mux_master_process_new_session(void) {}



/* Stub implementation for mux_master_process_alive_check */
void mux_master_process_alive_check(void) {}



/* Stub implementation for mux_master_process_terminate */
void mux_master_process_terminate(void) {}



/* Stub implementation for mux_master_process_open_fwd */
void mux_master_process_open_fwd(void) {}



/* Stub implementation for mux_master_process_close_fwd */
void mux_master_process_close_fwd(void) {}



/* Stub implementation for mux_master_process_stdio_fwd */
void mux_master_process_stdio_fwd(void) {}



/* Stub implementation for mux_master_process_stop_listening */
void mux_master_process_stop_listening(void) {}



/* Stub implementation for mux_master_process_proxy */
void mux_master_process_proxy(void) {}
