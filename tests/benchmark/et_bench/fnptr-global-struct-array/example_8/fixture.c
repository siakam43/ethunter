/* ET-Bench fixture: fnptr-global-struct-array/example_8 */
/* fnptr: ddt_ops[type]->ddt_op_lookup, targets: ddt_zap_lookup */
/* Pattern: DDT ops table with per-type lookup function pointer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DDT_TYPES 1
#define SET_ERROR(x) (x)
#define ENOENT 2

typedef uint64_t objset_t;
typedef struct ddt_entry ddt_entry_t;
typedef struct dmu_tx dmu_tx_t;

/* DDT ops struct with function pointer array */
typedef struct ddt_ops {
    char ddt_op_name[32];
    int (*ddt_op_create)(objset_t *os, uint64_t *object, dmu_tx_t *tx);
    int (*ddt_op_destroy)(objset_t *os, uint64_t object, dmu_tx_t *tx);
    int (*ddt_op_lookup)(objset_t *os, uint64_t object, ddt_entry_t *dde);
    void (*ddt_op_prefetch)(objset_t *os, uint64_t object, ddt_entry_t *dde);
    int (*ddt_op_update)(objset_t *os, uint64_t object, ddt_entry_t *dde, dmu_tx_t *tx);
    int (*ddt_op_remove)(objset_t *os, uint64_t object, ddt_entry_t *dde, dmu_tx_t *tx);
    int (*ddt_op_walk)(objset_t *os, uint64_t object, ddt_entry_t *dde, uint64_t *walk);
    int (*ddt_op_count)(objset_t *os, uint64_t object, uint64_t *count);
} ddt_ops_t;

enum ddt_type { DDT_TYPE_ZAP = 0 };
enum ddt_class { DDT_CLASS_DITTO = 0, DDT_CLASS_UNDITTO, DDT_NUM_CLASSES };

typedef struct ddt {
    objset_t ddt_os;
    uint64_t ddt_object[DDT_TYPES][DDT_NUM_CLASSES];
} ddt_t;

/* Stub types */
struct ddt_entry {};
struct dmu_tx {};

/* Target function */
static int ddt_zap_lookup(objset_t *os, uint64_t object, ddt_entry_t *dde) {
    (void)os; (void)object; (void)dde;
    return 0;
}

static int ddt_zap_create(objset_t *os, uint64_t *object, dmu_tx_t *tx) { (void)os; (void)object; (void)tx; return 0; }
static int ddt_zap_destroy(objset_t *os, uint64_t object, dmu_tx_t *tx) { (void)os; (void)object; (void)tx; return 0; }
static void ddt_zap_prefetch(objset_t *os, uint64_t object, ddt_entry_t *dde) { (void)os; (void)object; (void)dde; }
static int ddt_zap_update(objset_t *os, uint64_t object, ddt_entry_t *dde, dmu_tx_t *tx) { (void)os; (void)object; (void)dde; (void)tx; return 0; }
static int ddt_zap_remove(objset_t *os, uint64_t object, ddt_entry_t *dde, dmu_tx_t *tx) { (void)os; (void)object; (void)dde; (void)tx; return 0; }
static int ddt_zap_walk(objset_t *os, uint64_t object, ddt_entry_t *dde, uint64_t *walk) { (void)os; (void)object; (void)dde; (void)walk; return 0; }
static int ddt_zap_count(objset_t *os, uint64_t object, uint64_t *count) { (void)os; (void)object; (void)count; return 0; }

/* Global ops table */
static const ddt_ops_t ddt_zap_ops = {
    "zap",
    ddt_zap_create,
    ddt_zap_destroy,
    ddt_zap_lookup,
    ddt_zap_prefetch,
    ddt_zap_update,
    ddt_zap_remove,
    ddt_zap_walk,
    ddt_zap_count,
};

static const ddt_ops_t *const ddt_ops[DDT_TYPES] = {
    &ddt_zap_ops,
};

/* Helper */
static int ddt_object_exists(ddt_t *ddt, enum ddt_type type, enum ddt_class class) {
    (void)ddt; (void)type; (void)class;
    return 1;
}

/* Caller: ddt_ops[type]->ddt_op_lookup */
static int ddt_object_lookup(ddt_t *ddt, enum ddt_type type, enum ddt_class class,
    ddt_entry_t *dde) {
    if (!ddt_object_exists(ddt, type, class))
        return SET_ERROR(ENOENT);

    return ddt_ops[type]->ddt_op_lookup(ddt->ddt_os,
        ddt->ddt_object[type][class], dde);
}
