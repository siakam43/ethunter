/* CG-Bench fixture: fnptr-callback/example_15 */
/* fnptr: cb, targets: scpio, sftpio */

size_t
atomicio6(ssize_t (*f) (int, void *, size_t), int fd, void *_s, size_t n,
    int (*cb)(void *, size_t), void *cb_arg)
{
	char *s = _s;
	size_t pos = 0;
	ssize_t res;
	struct pollfd pfd;

	pfd.fd = fd;
#ifndef BROKEN_READ_COMPARISON
	pfd.events = f == read ? POLLIN : POLLOUT;
#else
	pfd.events = POLLIN|POLLOUT;
#endif
	while (n > pos) {
		res = (f) (fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR) {
				/* possible SIGALARM, update callback */
				if (cb != NULL && cb(cb_arg, 0) == -1) {
					errno = EINTR;
					return pos;
				}
				continue;
			} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
				(void)poll(&pfd, 1, -1);
				continue;
			}
			return 0;
		case 0:
			errno = EPIPE;
			return pos;
		default:
			pos += (size_t)res;
			if (cb != NULL && cb(cb_arg, (size_t)res) == -1) {
				errno = EINTR;
				return pos;
			}
		}
	}
	return pos;
}

void
source(int argc, char **argv)
{
  if (atomicio6(vwrite, remout, bp->buf, amt, scpio,
      &statbytes) != amt)
    haderr = errno;
}

static int
scpio(void *_cnt, size_t s)
{
	off_t *cnt = (off_t *)_cnt;

	*cnt += s;
	refresh_progress_meter(0);
	if (limit_kbps > 0)
		bandwidth_limit(&bwlimit, s);
	return 0;
}

static void
get_msg_extended(struct sftp_conn *conn, struct sshbuf *m, int initial)
{
	u_int msg_len;
	u_char *p;
	int r;

	sshbuf_reset(m);
	if ((r = sshbuf_reserve(m, 4, &p)) != 0)
		fatal_fr(r, "reserve");
	if (atomicio6(read, conn->fd_in, p, 4, sftpio,
	    conn->limit_kbps > 0 ? &conn->bwlimit_in : NULL) != 4) {
		if (errno == EPIPE || errno == ECONNRESET)
			fatal("Connection closed");
		else
			fatal("Couldn't read packet: %s", strerror(errno));
	}

	if ((r = sshbuf_get_u32(m, &msg_len)) != 0)
		fatal_fr(r, "sshbuf_get_u32");
	if (msg_len > SFTP_MAX_MSG_LENGTH) {
		do_log2(initial ? SYSLOG_LEVEL_ERROR : SYSLOG_LEVEL_FATAL,
		    "Received message too long %u", msg_len);
		fatal("Ensure the remote shell produces no output "
		    "for non-interactive sessions.");
	}

	if ((r = sshbuf_reserve(m, msg_len, &p)) != 0)
		fatal_fr(r, "reserve");
	if (atomicio6(read, conn->fd_in, p, msg_len, sftpio,
	    conn->limit_kbps > 0 ? &conn->bwlimit_in : NULL)
	    != msg_len) {
		if (errno == EPIPE)
			fatal("Connection closed");
		else
			fatal("Read packet: %s", strerror(errno));
	}
}

static int
sftpio(void *_bwlimit, size_t amount)
{
	struct bwlimit *bwlimit = (struct bwlimit *)_bwlimit;

	refresh_progress_meter(0);
	if (bwlimit != NULL)
		bandwidth_limit(bwlimit, amount);
	return 0;
}


/* Wrapper: calls through cb */
void cb_caller(void *, size_t) {
    cb(size_t);
}
