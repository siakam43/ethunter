/* et_bench fixture: fnptr-cast/example_5 */
/* fnptr: funs->memory, targets: __gmp_asprintf_memory */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#define ASSERT(x) assert(x)

/* --- Function pointer types for doprnt callbacks --- */
typedef int (*doprnt_format_t) (void *, const char *, va_list);
typedef int (*doprnt_memory_t) (void *, const char *, size_t);
typedef int (*doprnt_reps_t)   (void *, int, int);
typedef int (*doprnt_final_t)  (void *);

/* Function table — fnptr slots including "memory" */
struct doprnt_funs_t {
    doprnt_format_t  format;
    doprnt_memory_t  memory;
    doprnt_reps_t    reps;
    doprnt_final_t   final;
};

/* Parameters for formatted output */
struct doprnt_params_t {
    int width;
    int precision;
    int base;
};

/* Internal buffer state for asprintf */
struct gmp_asprintf_t {
    char *buf;
    size_t size;
    size_t alloc;
};

#define GMP_ASPRINTF_T_INIT(d, res) \
    do { (d).buf = malloc(256); (d).size = 0; (d).alloc = 256; } while (0)

#define GMP_ASPRINTF_T_NEED(d, len) \
    do { \
        while ((d)->size + (len) > (d)->alloc) { \
            (d)->alloc *= 2; \
            (d)->buf = realloc((d)->buf, (d)->alloc); \
        } \
    } while (0)

/* --- Target functions: __gmp_asprintf_memory et al --- */
int
__gmp_asprintf_memory(struct gmp_asprintf_t *d, const char *str, size_t len)
{
    GMP_ASPRINTF_T_NEED(d, len);
    memcpy(d->buf + d->size, str, len);
    d->size += len;
    return (int)len;
}

int
__gmp_asprintf_reps(void *d, int ch, int reps)
{
    (void)d;
    return reps;
}

int
__gmp_asprintf_format(void *d, const char *fmt, va_list ap)
{
    (void)d; (void)fmt; (void)ap;
    return 0;
}

/* --- The cast assignment: function pointers cast to typedef types --- */
const struct doprnt_funs_t __gmp_asprintf_funs_noformat = {
    (doprnt_format_t) __gmp_asprintf_format,
    (doprnt_memory_t) __gmp_asprintf_memory,
    (doprnt_reps_t)   __gmp_asprintf_reps,
    NULL
};

/* --- Caller: __gmp_doprnt_integer — calls through funs->memory --- */
int
__gmp_doprnt_integer(const struct doprnt_funs_t *funs,
                     void *data,
                     const struct doprnt_params_t *p,
                     const char *s)
{
    int retval = 0;
    int slen = 0;
    int slashlen = 0;
    const char *slash = NULL;
    const char *showbase = NULL;
    int den_showbaselen = 0;

    (void)p;

    if (den_showbaselen != 0) {
        ASSERT(slash != NULL);
        slashlen = (int)(slash + 1 - s);
        /* DOPRNT_MEMORY expands to funs->memory call */
        retval += funs->memory(data, s, slashlen);
        slen -= slashlen;
        s += slashlen;
        retval += funs->memory(data, showbase, den_showbaselen);
    }
    return retval;
}

/* Higher-level caller wrapping the integer formatter */
char result[1024];

int
__gmp_doprnt_integer_ostream(void *o, struct doprnt_params_t *p, char *s)
{
    struct gmp_asprintf_t d;
    int ret;

    GMP_ASPRINTF_T_INIT(d, &result);
    ret = __gmp_doprnt_integer(&__gmp_asprintf_funs_noformat, &d, p, s);
    free(d.buf);
    return ret;
}
