/* et_bench fixture: fnptr-cast/example_6 */
/* fnptr: holdfunc, targets: dsl_dataset_hold, dsl_dataset_hold_obj_string */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* --- Type stubs for ZFS DSL --- */
typedef struct dmu_tx dmu_tx_t;
typedef struct dsl_pool dsl_pool_t;
typedef struct dsl_dataset dsl_dataset_t;
typedef struct nvpair nvpair_t;
typedef struct nvlist nvlist_t;

#define FTAG ((void *)0)
#define B_TRUE 1
#define B_FALSE 0
#define KM_SLEEP 1
#define ZFS_SPACE_CHECK_EXTRA_RESERVED 3

/* Hold function type — typedef used for fnptr-cast */
typedef int (*dsl_holdfunc_t)(dsl_pool_t *, const char *, void *, dsl_dataset_t **);

/* Release argument structure carrying the holdfunc pointer */
typedef struct {
    dsl_holdfunc_t       ddura_holdfunc;
    nvlist_t            *ddura_holds;
    nvlist_t            *ddura_errlist;
    nvlist_t            *ddura_todelete;
    nvlist_t            *ddura_chkholds;
} dsl_dataset_user_release_arg_t;

/* --- Stub implementations --- */
static dmu_tx_t *dmu_tx_pool(dmu_tx_t *tx) { (void)tx; return NULL; }
static void *dsl_dataset_user_release_sync_one(dsl_dataset_t *ds, nvlist_t *nvl, dmu_tx_t *tx) { (void)ds; (void)nvl; (void)tx; return NULL; }
static void dsl_dataset_rele(dsl_dataset_t *ds, void *tag) { (void)ds; (void)tag; }
static void dsl_destroy_snapshot_sync_impl(dsl_dataset_t *ds, int flag, dmu_tx_t *tx) { (void)ds; (void)flag; (void)tx; }
static nvpair_t *nvlist_next_nvpair(nvlist_t *nvl, nvpair_t *pair) { (void)nvl; (void)pair; return NULL; }
static const char *nvpair_name(nvpair_t *pair) { (void)pair; return ""; }
static nvlist_t *fnvpair_value_nvlist(nvpair_t *pair) { (void)pair; return NULL; }
static int nvlist_exists(nvlist_t *nvl, const char *name) { (void)nvl; (void)name; return 0; }
static int nvlist_alloc(nvlist_t **out, int flags, int sleep) { (void)flags; (void)sleep; *out = calloc(1, sizeof(nvlist_t)); return 0; }
static void fnvlist_free(nvlist_t *nvl) { free(nvl); }

/* DSL sync function types */
typedef int (*dsl_checkfunc_t)(const char *, nvlist_t *, nvlist_t *, nvlist_t *, void *, int);
typedef void (*dsl_syncfunc_t)(void *, dmu_tx_t *);

static int dsl_sync_task_common(const char *pool, dsl_checkfunc_t *checkfunc,
    dsl_syncfunc_t *syncfunc, void *sigfunc, void *arg,
    int blocks_modified, int space_check, int early)
{
    (void)pool; (void)checkfunc; (void)syncfunc; (void)sigfunc;
    (void)arg; (void)blocks_modified; (void)space_check; (void)early;
    return 0;
}

int
dsl_sync_task(const char *pool, dsl_checkfunc_t *checkfunc,
    dsl_syncfunc_t *syncfunc, void *arg,
    int blocks_modified, int space_check)
{
    return dsl_sync_task_common(pool, checkfunc, syncfunc, NULL, arg,
        blocks_modified, space_check, B_FALSE);
}

static int dsl_dataset_user_release_check(const char *pool, nvlist_t *a, nvlist_t *b, nvlist_t *c, void *arg, int d) {
    (void)pool; (void)a; (void)b; (void)c; (void)arg; (void)d; return 0;
}

/* --- Target functions: dsl_dataset_hold and dsl_dataset_hold_obj_string --- */
int
dsl_dataset_hold(dsl_pool_t *dp, const char *name, void *tag, dsl_dataset_t **dsp)
{
    (void)dp; (void)name; (void)tag; (void)dsp;
    *dsp = calloc(1, sizeof(dsl_dataset_t));
    return 0;
}

int
dsl_dataset_hold_obj_string(dsl_pool_t *dp, const char *name, void *tag, dsl_dataset_t **dsp)
{
    (void)dp; (void)name; (void)tag; (void)dsp;
    *dsp = calloc(1, sizeof(dsl_dataset_t));
    return 0;
}

/* --- Caller: dsl_dataset_user_release_sync — calls through holdfunc --- */
static void
dsl_dataset_user_release_sync(void *arg, dmu_tx_t *tx)
{
    dsl_dataset_user_release_arg_t *ddura = arg;
    dsl_holdfunc_t *holdfunc = ddura->ddura_holdfunc;
    dsl_pool_t *dp = dmu_tx_pool(tx);

    for (nvpair_t *pair = nvlist_next_nvpair(ddura->ddura_chkholds, NULL);
        pair != NULL; pair = nvlist_next_nvpair(ddura->ddura_chkholds, pair)) {
        dsl_dataset_t *ds;
        const char *name = nvpair_name(pair);

        holdfunc(dp, name, FTAG, &ds);

        dsl_dataset_user_release_sync_one(ds,
            fnvpair_value_nvlist(pair), tx);
        if (nvlist_exists(ddura->ddura_todelete, name)) {
            dsl_destroy_snapshot_sync_impl(ds, B_FALSE, tx);
        }
        dsl_dataset_rele(ds, FTAG);
    }
}

/* --- Higher-level caller that sets holdfunc to different targets --- */
static int
dsl_dataset_user_release_impl(nvlist_t *holds, nvlist_t *errlist,
    dsl_pool_t *tmpdp)
{
    dsl_dataset_user_release_arg_t ddura;
    nvpair_t *pair;
    const char *pool;
    int error;

    pair = nvlist_next_nvpair(holds, NULL);
    if (pair == NULL)
        return 0;

    /* Key branching: holdfunc assigned to one of two targets via cast */
    if (tmpdp != NULL) {
        ddura.ddura_holdfunc = (dsl_holdfunc_t)dsl_dataset_hold_obj_string;
        pool = "tmp_pool";
    } else {
        ddura.ddura_holdfunc = (dsl_holdfunc_t)dsl_dataset_hold;
        pool = nvpair_name(pair);
    }

    ddura.ddura_holds = holds;
    ddura.ddura_errlist = errlist;
    nvlist_alloc(&ddura.ddura_todelete, 0, KM_SLEEP);
    nvlist_alloc(&ddura.ddura_chkholds, 0, KM_SLEEP);

    error = dsl_sync_task(pool, dsl_dataset_user_release_check,
        dsl_dataset_user_release_sync, &ddura, 0,
        ZFS_SPACE_CHECK_EXTRA_RESERVED);
    fnvlist_free(ddura.ddura_todelete);
    fnvlist_free(ddura.ddura_chkholds);

    return error;
}
