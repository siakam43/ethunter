/* CG-Bench fixture: fnptr-cast/example_5 */
/* fnptr: funs->memory, targets: __gmp_asprintf_memory */

int __gmp_doprnt_integer (const struct doprnt_funs_t *funs,
		      void *data,
		      const struct doprnt_params_t *p,
		      const char *s)
{
  ...
  if (den_showbaselen != 0)
  {
    ASSERT (slash != NULL);
    slashlen = slash+1 - s;
    DOPRNT_MEMORY (s, slashlen);                 /* numerator and slash */
    slen -= slashlen;
    s += slashlen;
    DOPRNT_MEMORY (showbase, den_showbaselen);
  }
  ...
}

#define DOPRNT_ACCUMULATE(call)						\
  do {									\
    int  __ret;								\
    __ret = call;							\
    if (__ret == -1)							\
      goto error;							\
    retval += __ret;							\
  } while (0)
#define DOPRNT_ACCUMULATE_FUN(fun, params)				\
  do {									\
    ASSERT ((fun) != NULL);						\
    DOPRNT_ACCUMULATE ((*(fun)) params);				\
  } while (0)

#define DOPRNT_MEMORY(ptr, len)						\
  DOPRNT_ACCUMULATE_FUN (funs->memory, (data, ptr, len))

ostream& __gmp_doprnt_integer_ostream (ostream &o, struct doprnt_params_t *p,
                              char *s)
{
  struct gmp_asprintf_t   d;
  ...

  GMP_ASPRINTF_T_INIT (d, &result);
  ret = __gmp_doprnt_integer (&__gmp_asprintf_funs_noformat, &d, p, s);
  ...
  return o.write (t.str, t.len);
}

typedef int (*doprnt_format_t) (void *, const char *, va_list);
typedef int (*doprnt_memory_t) (void *, const char *, size_t);
typedef int (*doprnt_reps_t)   (void *, int, int);
typedef int (*doprnt_final_t)  (void *);

struct doprnt_funs_t {
  doprnt_format_t  format;
  doprnt_memory_t  memory;
  doprnt_reps_t    reps;
  doprnt_final_t   final;   /* NULL if not required */
};

const struct doprnt_funs_t  __gmp_asprintf_funs_noformat = {
  NULL,
  (doprnt_memory_t) __gmp_asprintf_memory,
  (doprnt_reps_t)   __gmp_asprintf_reps,
  NULL
};

int
__gmp_asprintf_memory (struct gmp_asprintf_t *d, const char *str, size_t len)
{
  GMP_ASPRINTF_T_NEED (d, len);
  memcpy (d->buf + d->size, str, len);
  d->size += len;
  return len;
}


/* Wrapper: calls through funs->memory */
void memory_caller(void) {
    funs->memory();
}
