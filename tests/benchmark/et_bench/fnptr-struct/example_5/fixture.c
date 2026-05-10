/* ET-Bench fixture: fnptr-struct/example_5 */
/* Scenario: ZFS spacemap log iteration callback dispatch.
   fnptr: uic->uic_cb
   targets: spacemap_check_sm_log_cb, load_unflushed_cb,
            load_unflushed_svr_segs_cb, count_unflushed_space_cb,
            log_spacemap_obsolete_stats_cb
   caller: iterate_through_spacemap_logs_cb */

#include <stddef.h>
#include <stdint.h>

#define VERIFY0(x) do { int _err = (x); (void)_err; } while (0)

typedef struct spa spa_t;
typedef uint64_t zdb_log_sm_cb_t;
typedef struct space_map space_map_t;
typedef uint64_t zdb_cb_t;
typedef struct metaslab_verify metaslab_verify_t;

enum { SCL_CONFIG, RW_READER };

typedef struct {
    uint64_t block_type;
} space_map_entry_t;

typedef int (sm_cb_t)(space_map_entry_t *sme, void *arg);

typedef int (unflushed_iter_fn_t)(uint64_t spa, space_map_entry_t *sme,
                                   uint64_t txg, void *arg);

typedef struct unflushed_iter_cb_arg {
    uint64_t uic_spa;
    uint64_t uic_txg;
    void *uic_arg;
    unflushed_iter_fn_t *uic_cb;
} unflushed_iter_cb_arg_t;

static int spa_feature_is_active(spa_t *spa, int feature) {
    (void)spa; (void)feature;
    return 0;
}

static void spa_config_enter(spa_t *spa, int lock, void *tag, int mode) {
    (void)spa; (void)lock; (void)tag; (void)mode;
}

static void spa_config_exit(spa_t *spa, int lock, void *tag) {
    (void)spa; (void)lock; (void)tag;
}

static space_map_t *avl_first(void *tree) { (void)tree; return NULL; }
#define AVL_NEXT(tree, node) ((void)tree, (void)node, NULL)

static void space_map_close(space_map_t *sm) { (void)sm; }
static uint64_t space_map_length(space_map_t *sm) { (void)sm; return 0; }

/* Caller: invokes uic->uic_cb through the struct */
static int iterate_through_spacemap_logs_cb(space_map_entry_t *sme, void *arg)
{
    unflushed_iter_cb_arg_t *uic = arg;
    return (uic->uic_cb(uic->uic_spa, sme, uic->uic_txg, uic->uic_arg));
}

/* Target callbacks */
static int spacemap_check_sm_log_cb(uint64_t spa, space_map_entry_t *sme,
                                     uint64_t txg, void *arg)
{
    (void)spa; (void)sme; (void)txg; (void)arg;
    return 0;
}

static int load_unflushed_cb(uint64_t spa, space_map_entry_t *sme,
                              uint64_t txg, void *arg)
{
    (void)spa; (void)sme; (void)txg; (void)arg;
    return 0;
}

static int load_unflushed_svr_segs_cb(uint64_t spa, space_map_entry_t *sme,
                                       uint64_t txg, void *arg)
{
    (void)spa; (void)sme; (void)txg; (void)arg;
    return 0;
}

static int count_unflushed_space_cb(uint64_t spa, space_map_entry_t *sme,
                                     uint64_t txg, void *arg)
{
    (void)spa; (void)sme; (void)txg; (void)arg;
    return 0;
}

static int log_spacemap_obsolete_stats_cb(uint64_t spa, space_map_entry_t *sme,
                                           uint64_t txg, void *arg)
{
    (void)spa; (void)sme; (void)txg; (void)arg;
    return 0;
}

static void iterate_through_spacemap_logs(spa_t *spa, zdb_log_sm_cb_t cb, void *arg)
{
    if (!spa_feature_is_active(spa, 0))
        return;

    spa_config_enter(spa, SCL_CONFIG, NULL, RW_READER);
    for (int i = 0; i < 1; i++) {
        space_map_t *sm = NULL;

        unflushed_iter_cb_arg_t uic = {
            .uic_spa = (uint64_t)(uintptr_t)spa,
            .uic_txg = (uint64_t)i,
            .uic_arg = arg,
            .uic_cb = (unflushed_iter_fn_t *)(uintptr_t)cb
        };
        VERIFY0(space_map_iterate(sm, 0,
            iterate_through_spacemap_logs_cb, &uic));
        space_map_close(sm);
    }
    spa_config_exit(spa, SCL_CONFIG, NULL);
}

int space_map_iterate(space_map_t *sm, uint64_t end, sm_cb_t callback, void *arg)
{
    int error = 0;
    (void)sm; (void)end;
    for (uint64_t block_base = 0; block_base < end && error == 0;
        block_base += 4096) {
        space_map_entry_t sme;
        error = callback(&sme, arg);
    }
    return (error);
}

static void spacemap_check_sm_log(spa_t *spa, metaslab_verify_t *mv)
{
    (void)mv;
    iterate_through_spacemap_logs(spa, (zdb_log_sm_cb_t)(uintptr_t)spacemap_check_sm_log_cb, mv);
}

static void load_unflushed_to_ms_allocatables(spa_t *spa, int maptype)
{
    iterate_through_spacemap_logs(spa, (zdb_log_sm_cb_t)(uintptr_t)load_unflushed_cb, &maptype);
}

static void zdb_claim_removing(spa_t *spa, zdb_cb_t *zcb)
{
    (void)zcb;
    iterate_through_spacemap_logs(spa, (zdb_log_sm_cb_t)(uintptr_t)load_unflushed_svr_segs_cb, NULL);
}

static int64_t get_unflushed_alloc_space(spa_t *spa)
{
    int64_t ualloc_space = 0;
    iterate_through_spacemap_logs(spa, (zdb_log_sm_cb_t)(uintptr_t)count_unflushed_space_cb,
        &ualloc_space);
    return (ualloc_space);
}

static void dump_log_spacemap_obsolete_stats(spa_t *spa)
{
    int lsos = 0;
    iterate_through_spacemap_logs(spa,
        (zdb_log_sm_cb_t)(uintptr_t)log_spacemap_obsolete_stats_cb, &lsos);
}
