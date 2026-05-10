/* ET-Bench fixture: fnptr-callback/example_15 */
/* Based on OpenSSH's atomicio6 / source / get_msg_extended pattern */
/* fnptr: cb, targets: scpio, sftpio */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifndef BROKEN_READ_COMPARISON
#define BROKEN_READ_COMPARISON 0
#endif

#define EAGAIN 11
#define EWOULDBLOCK 11
#define EINTR 4
#define EPIPE 32

struct pollfd {
    int fd;
    short events;
};

#define POLLIN  0x001
#define POLLOUT 0x004

static int poll(struct pollfd *pfd, int nfds, int timeout) {
    return 0;
}

static ssize_t read(int fd, void *buf, size_t count) {
    return 0;
}

static ssize_t vwrite(int fd, void *buf, size_t count) {
    return (ssize_t)count;
}

typedef unsigned int u_int;

struct sshbuf {
    unsigned char *buf;
    size_t len;
    size_t off;
    size_t max_len;
};

static int sshbuf_reset(struct sshbuf *b) { b->off = 0; b->len = 0; return 0; }
static int sshbuf_reserve(struct sshbuf *b, size_t len, unsigned char **p) {
    return 0;
}
static int sshbuf_get_u32(struct sshbuf *b, u_int *val) {
    return 0;
}

#define SFTP_MAX_MSG_LENGTH (256 * 1024)

struct bwlimit {
    size_t kbps;
    size_t transferred;
};

struct sftp_conn {
    int fd_in;
    int limit_kbps;
    struct bwlimit bwlimit_in;
};

void refresh_progress_meter(int force) {}
void bandwidth_limit(struct bwlimit *bl, size_t amount) {
    bl->transferred += amount;
}

static void do_log2(int level, const char *fmt, ...) {}
static void fatal(const char *fmt, ...) {}
#define SYSLOG_LEVEL_ERROR 3
#define SYSLOG_LEVEL_FATAL 7

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

int
scpio(void *_cnt, size_t s)
{
    off_t *cnt = (off_t *)_cnt;

    *cnt += s;
    refresh_progress_meter(0);
    return 0;
}

void
source(int argc, char **argv)
{
    int remout = 1;
    char buf[4096];
    size_t amt = 4096;
    off_t statbytes = 0;
    int haderr = 0;

    if (atomicio6(vwrite, remout, buf, amt, scpio,
        &statbytes) != amt)
        haderr = errno;
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

static void
get_msg_extended(struct sftp_conn *conn, struct sshbuf *m, int initial)
{
    u_int msg_len;
    unsigned char *p;
    int r;

    sshbuf_reset(m);
    if ((r = sshbuf_reserve(m, 4, &p)) != 0)
        fatal("reserve failed");
    if (atomicio6(read, conn->fd_in, p, 4, sftpio,
        conn->limit_kbps > 0 ? &conn->bwlimit_in : NULL) != 4) {
        if (errno == EPIPE || errno == ECONNRESET)
            fatal("Connection closed");
        else
            fatal("Couldn't read packet: %s", strerror(errno));
    }

    if ((r = sshbuf_get_u32(m, &msg_len)) != 0)
        fatal("sshbuf_get_u32 failed");
    if (msg_len > SFTP_MAX_MSG_LENGTH) {
        do_log2(initial ? SYSLOG_LEVEL_ERROR : SYSLOG_LEVEL_FATAL,
            "Received message too long %u", msg_len);
        fatal("Ensure the remote shell produces no output "
            "for non-interactive sessions.");
    }

    if ((r = sshbuf_reserve(m, msg_len, &p)) != 0)
        fatal("reserve failed");
    if (atomicio6(read, conn->fd_in, p, msg_len, sftpio,
        conn->limit_kbps > 0 ? &conn->bwlimit_in : NULL)
        != msg_len) {
        if (errno == EPIPE)
            fatal("Connection closed");
        else
            fatal("Read packet: %s", strerror(errno));
    }
}
