/* CG-Bench fixture: fnptr-global-struct-array/example_8 */
/* fnptr: ddt_ops[type]->ddt_op_lookup, targets: ddt_zap_lookup */

static int
ddt_object_lookup(ddt_t *ddt, enum ddt_type type, enum ddt_class class,
    ddt_entry_t *dde)
{
	if (!ddt_object_exists(ddt, type, class))
		return (SET_ERROR(ENOENT));

	return (ddt_ops[type]->ddt_op_lookup(ddt->ddt_os,
	    ddt->ddt_object[type][class], dde));
}

static const ddt_ops_t *const ddt_ops[DDT_TYPES] = {
	&ddt_zap_ops,
};

const ddt_ops_t ddt_zap_ops = {
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

typedef struct ddt_ops {
	char ddt_op_name[32];
	int (*ddt_op_create)(objset_t *os, uint64_t *object, dmu_tx_t *tx,
	    boolean_t prehash);
	int (*ddt_op_destroy)(objset_t *os, uint64_t object, dmu_tx_t *tx);
	int (*ddt_op_lookup)(objset_t *os, uint64_t object, ddt_entry_t *dde);
	void (*ddt_op_prefetch)(objset_t *os, uint64_t object,
	    ddt_entry_t *dde);
	int (*ddt_op_update)(objset_t *os, uint64_t object, ddt_entry_t *dde,
	    dmu_tx_t *tx);
	int (*ddt_op_remove)(objset_t *os, uint64_t object, ddt_entry_t *dde,
	    dmu_tx_t *tx);
	int (*ddt_op_walk)(objset_t *os, uint64_t object, ddt_entry_t *dde,
	    uint64_t *walk);
	int (*ddt_op_count)(objset_t *os, uint64_t object, uint64_t *count);
} ddt_ops_t;


/* Stub implementation for ddt_zap_lookup */
void ddt_zap_lookup(void) {}
