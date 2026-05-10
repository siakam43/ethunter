/* ET-Bench fixture: fnptr-only/example_5 */
/* fnptr: tmp_handler, targets: mm_log_handler */
/* Source: OpenSSH-style privilege separation logging */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_FATAL = 3
} LogLevel;

typedef void (*log_handler_fn)(LogLevel level, int force,
                                const char *msg, void *ctx);

static log_handler_fn *log_handler;
static void *log_handler_ctx;
static int log_level = LOG_LEVEL_INFO;
static int log_on_stderr = 1;
static int log_stderr_fd = 2;
static const char *argv0;

static void mm_log_handler(LogLevel level, int force,
                            const char *msg, void *ctx)
{
    FILE *fp = stderr;
    fprintf(fp, "[monitor] %s\n", msg);
    fflush(fp);
}

static void
do_log(LogLevel level, int force, const char *suffix, const char *fmt,
       const char *arg)
{
    log_handler_fn *tmp_handler;
    char msgbuf[256];
    char fmtbuf[256];

    const char *progname = argv0 ? argv0 : "sshd";

    if (!force && level > log_level)
        return;

    snprintf(fmtbuf, sizeof(fmtbuf), fmt, arg);

    if (log_handler != NULL) {
        tmp_handler = log_handler;
        log_handler = NULL;
        tmp_handler(level, force, fmtbuf, log_handler_ctx);
        log_handler = tmp_handler;
    } else if (log_on_stderr) {
        snprintf(msgbuf, sizeof(msgbuf), "%s%s%.*s\r\n",
                 (log_on_stderr > 1) ? progname : "",
                 (log_on_stderr > 1) ? ": " : "",
                 (int)sizeof(msgbuf) - 3, fmtbuf);
        write(log_stderr_fd, msgbuf, strlen(msgbuf));
    }
}

void set_log_handler(log_handler_fn *handler, void *ctx)
{
    log_handler = handler;
    log_handler_ctx = ctx;
}

static void do_log_error(const char *msg) {
    do_log(LOG_LEVEL_ERROR, 0, NULL, "%s", msg);
}

static void setup_privsep_logging(void) {
    set_log_handler(mm_log_handler, NULL);
    do_log(LOG_LEVEL_INFO, 1, NULL, "privilege separation logging active", "");
}
