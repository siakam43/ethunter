/* et_bench fixture: fnptr-cast/example_2 */
/* fnptr: ops->fmdo_close, targets: zfs_fm_close */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* --- Forward declarations and type stubs --- */
typedef struct fmd_hdl fmd_hdl_t;
typedef struct fmd_case fmd_case_t;
typedef struct fmd_module fmd_module_t;
typedef struct zfs_case zfs_case_t;
typedef struct libzfs_handle libzfs_handle_t;

typedef struct {
    unsigned long long ui64;
} fmd_stat_value_t;

typedef struct {
    char fmds_name[64];
    fmd_stat_value_t fmds_value;
} fmd_stat_t;

typedef struct {
    fmd_stat_t ms_accepted;
    fmd_stat_t ms_caseopen;
    fmd_stat_t ms_casesolved;
    fmd_stat_t ms_caseclosed;
} fmd_mod_stats_t;

typedef struct {
    const char *fmdi_desc;
} fmd_mod_info_t;

/* Module operations struct — defines the fnptr-cast target slots */
typedef struct fmd_hdl_ops {
    void (*fmdo_recv)(fmd_hdl_t *, void *, void *, const char *);
    void (*fmdo_timeout)(fmd_hdl_t *, int, void *);
    void (*fmdo_close)(fmd_hdl_t *, fmd_case_t *);
    void (*fmdo_stats)(fmd_hdl_t *);
    void (*fmdo_gc)(fmd_hdl_t *);
} fmd_hdl_ops_t;

typedef struct {
    const char *fmdi_desc;
    const char *fmdi_ver;
    const fmd_hdl_ops_t *fmdi_ops;
    void *fmdi_props;
} fmd_hdl_info_t;

/* Module and handle structures */
struct fmd_module {
    const fmd_hdl_info_t *mod_info;
    const char *mod_name;
    void *mod_spec;
    fmd_mod_stats_t mod_stats;
    void *mod_serds;
};

struct fmd_hdl {
    fmd_module_t *impl;
};

struct fmd_case {
    char ci_uuid[64];
    void *ci_bufptr;
    size_t ci_bufsiz;
};

struct zfs_case {
    struct {
        char zc_serd_checksum[64];
        char zc_serd_io[64];
        int zc_has_remove_timer;
    } zc_data;
    int zc_remove_timer;
    void *zc_node;
};

/* Typedef for the ops struct used in fmd_hdl_register */
static void *fmd_serd_hash_create(void);
static void umem_free(void *, size_t);
static void zed_log_msg(int level, const char *fmt, ...);

static zfs_case_t *fmd_case_getspecific(fmd_hdl_t *hdl, fmd_case_t *cs);
static void fmd_serd_destroy(fmd_hdl_t *hdl, const char *name);
static void fmd_timer_remove(fmd_hdl_t *hdl, int timer_id);
static void *uu_list_remove(void *list, void *node);
static void uu_list_node_fini(void *node, void *nodep, void *pool);
static void zfs_purge_cases(fmd_hdl_t *hdl);

/* --- The target function: zfs_fm_close --- */
static void
zfs_fm_close(fmd_hdl_t *hdl, fmd_case_t *cs)
{
    zfs_case_t *zcp = fmd_case_getspecific(hdl, cs);

    if (zcp->zc_data.zc_serd_checksum[0] != '\0')
        fmd_serd_destroy(hdl, zcp->zc_data.zc_serd_checksum);
    if (zcp->zc_data.zc_serd_io[0] != '\0')
        fmd_serd_destroy(hdl, zcp->zc_data.zc_serd_io);
    if (zcp->zc_data.zc_has_remove_timer)
        fmd_timer_remove(hdl, zcp->zc_remove_timer);

    uu_list_remove((void *)0, zcp);
    uu_list_node_fini(zcp, &zcp->zc_node, (void *)0);
    fmd_hdl_free(hdl, zcp, sizeof(zfs_case_t));
}

static void
zfs_fm_gc(fmd_hdl_t *hdl)
{
    zfs_purge_cases(hdl);
}

static void
zfs_fm_timeout(fmd_hdl_t *hdl, int id, void *data)
{
    zfs_case_t *zcp = data;
    (void)zcp;
    (void)id;
}

static void
zfs_fm_recv(fmd_hdl_t *hdl, void *ep, void *nvl, const char *class)
{
    (void)hdl; (void)ep; (void)nvl; (void)class;
}

/* The ops struct — assigns zfs_fm_close to fmdo_close slot */
static const fmd_hdl_ops_t fmd_ops = {
    zfs_fm_recv,      /* fmdo_recv */
    zfs_fm_timeout,   /* fmdo_timeout */
    zfs_fm_close,     /* fmdo_close */
    NULL,             /* fmdo_stats */
    zfs_fm_gc,        /* fmdo_gc */
};

static const fmd_hdl_info_t fmd_info = {
    "ZFS Diagnosis Engine", "1.0", &fmd_ops, NULL
};

#define FMD_API_VERSION 1
#define FMD_SLEEP 1
#define FMD_STR_BUCKETS 16

/* --- Caller function: fmd_case_close --- */
void
fmd_case_close(fmd_hdl_t *hdl, fmd_case_t *cp)
{
    fmd_module_t *mp = (fmd_module_t *)hdl;
    const fmd_hdl_ops_t *ops = mp->mod_info->fmdi_ops;

    fmd_hdl_debug(hdl, "case closed (%s)", cp->ci_uuid);

    if (ops->fmdo_close != NULL)
        ops->fmdo_close(hdl, cp);

    mp->mod_stats.ms_caseopen.fmds_value.ui64--;
    mp->mod_stats.ms_caseclosed.fmds_value.ui64++;

    if (cp->ci_bufptr != NULL && cp->ci_bufsiz > 0)
        fmd_hdl_free(hdl, cp->ci_bufptr, cp->ci_bufsiz);

    fmd_hdl_free(hdl, cp, sizeof(fmd_case_t));
}

void
fmd_hdl_debug(fmd_hdl_t *hdl, const char *format, ...)
{
    char message[256];
    va_list vargs;
    fmd_module_t *mp = (fmd_module_t *)hdl;

    va_start(vargs, format);
    vsnprintf(message, sizeof(message), format, vargs);
    va_end(vargs);

    zed_log_msg(1, "%s: %s", mp->mod_name, message);
}

void
fmd_hdl_free(fmd_hdl_t *hdl, void *data, size_t size)
{
    (void)hdl;
    umem_free(data, size);
}

void *
fmd_hdl_zalloc(fmd_hdl_t *hdl, size_t size, int flags)
{
    (void)flags;
    return calloc(1, size);
}

void
fmd_hdl_setspecific(fmd_hdl_t *hdl, void *data)
{
    (void)hdl; (void)data;
}

void *
fmd_serd_hash_create(void)
{
    return calloc(FMD_STR_BUCKETS, sizeof(void *));
}

int
fmd_hdl_register(fmd_hdl_t *hdl, int version, const fmd_hdl_info_t *mip)
{
    (void)version;
    fmd_module_t *mp = (fmd_module_t *)hdl;

    mp->mod_info = mip;
    mp->mod_name = mip->fmdi_desc + 4;
    mp->mod_spec = NULL;

    strcpy(mp->mod_stats.ms_accepted.fmds_name, "fmd.accepted");
    strcpy(mp->mod_stats.ms_caseopen.fmds_name, "fmd.caseopen");
    strcpy(mp->mod_stats.ms_casesolved.fmds_name, "fmd.casesolved");
    strcpy(mp->mod_stats.ms_caseclosed.fmds_name, "fmd.caseclosed");

    mp->mod_serds = fmd_serd_hash_create();

    fmd_hdl_debug(hdl, "register module");

    return 0;
}

void
_zfs_retire_init(fmd_hdl_t *hdl)
{
    libzfs_handle_t *zhdl;

    if ((zhdl = (libzfs_handle_t *)0) == NULL)
        return;

    if (fmd_hdl_register(hdl, FMD_API_VERSION, &fmd_info) != 0)
        return;

    void *zdp = fmd_hdl_zalloc(hdl, 64, FMD_SLEEP);
    fmd_hdl_setspecific(hdl, zdp);
}

/* --- Stub helpers --- */
static void umem_free(void *data, size_t size) { (void)data; (void)size; }
static void zed_log_msg(int level, const char *fmt, ...) { (void)level; (void)fmt; }
static zfs_case_t *fmd_case_getspecific(fmd_hdl_t *hdl, fmd_case_t *cs) { (void)hdl; (void)cs; static zfs_case_t zc; return &zc; }
static void fmd_serd_destroy(fmd_hdl_t *hdl, const char *name) { (void)hdl; (void)name; }
static void fmd_timer_remove(fmd_hdl_t *hdl, int timer_id) { (void)hdl; (void)timer_id; }
static void *uu_list_remove(void *list, void *node) { (void)list; (void)node; return NULL; }
static void uu_list_node_fini(void *node, void *nodep, void *pool) { (void)node; (void)nodep; (void)pool; }
static void zfs_purge_cases(fmd_hdl_t *hdl) { (void)hdl; }
