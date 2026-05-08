/* CG-Bench fixture: fnptr-only/example_5 */
/* fnptr: tmp_handler, targets: mm_log_handler */

static void
do_log(LogLevel level, int force, const char *suffix, const char *fmt,
    va_list args)
{
        ...
	log_handler_fn *tmp_handler;
	const char *progname = argv0 != NULL ? argv0 : __progname;

	if (!force && level > log_level)
		return;

	...
	if (log_handler != NULL) {
		/* Avoid recursion */
		tmp_handler = log_handler;
		log_handler = NULL;
		tmp_handler(level, force, fmtbuf, log_handler_ctx);
		log_handler = tmp_handler;
	} else if (log_on_stderr) {
		snprintf(msgbuf, sizeof msgbuf, "%s%s%.*s\r\n",
		    (log_on_stderr > 1) ? progname : "",
		    (log_on_stderr > 1) ? ": " : "",
		    (int)sizeof msgbuf - 3, fmtbuf);
		(void)write(log_stderr_fd, msgbuf, strlen(msgbuf));
	} else {
        ...
    }
}

static log_handler_fn *log_handler;

void set_log_handler(log_handler_fn *handler, void *ctx)
{
	log_handler = handler;
	log_handler_ctx = ctx;
}

static int privsep_preauth(struct ssh *ssh)
{
	...

	if (use_privsep == PRIVSEP_ON)
		box = ssh_sandbox_init(pmonitor);
	pid = fork();
	if (pid == -1) {
		fatal("fork of unprivileged child failed");
	} else if (pid != 0) {
		...
	} else {
		...
		/* Arrange for logging to be sent to the monitor */
		set_log_handler(mm_log_handler, pmonitor);
		...
	}
}


/* Stub implementation for mm_log_handler */
void mm_log_handler(void) {}
