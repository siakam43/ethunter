/* CG-Bench fixture: fnptr-global-struct/example_1 */
/* fnptr: vdev_indirect_ops.vdev_op_remap, targets: vdev_indirect_remap */

static void
claim_segment_cb(void *arg, uint64_t offset, uint64_t size)
{
	vdev_t *vd = arg;

	vdev_indirect_ops.vdev_op_remap(vd, offset, size,
	    claim_segment_impl_cb, NULL);
}

vdev_ops_t vdev_indirect_ops = {
	.vdev_op_init = NULL,
	.vdev_op_fini = NULL,
	.vdev_op_open = vdev_indirect_open,
	.vdev_op_close = vdev_indirect_close,
	.vdev_op_asize = vdev_default_asize,
	.vdev_op_min_asize = vdev_default_min_asize,
	.vdev_op_min_alloc = NULL,
	.vdev_op_io_start = vdev_indirect_io_start,
	.vdev_op_io_done = vdev_indirect_io_done,
	.vdev_op_state_change = NULL,
	.vdev_op_need_resilver = NULL,
	.vdev_op_hold = NULL,
	.vdev_op_rele = NULL,
	.vdev_op_remap = vdev_indirect_remap,
	.vdev_op_xlate = NULL,
	.vdev_op_rebuild_asize = NULL,
	.vdev_op_metaslab_init = NULL,
	.vdev_op_config_generate = NULL,
	.vdev_op_nparity = NULL,
	.vdev_op_ndisks = NULL,
	.vdev_op_type = VDEV_TYPE_INDIRECT,	/* name of this vdev type */
	.vdev_op_leaf = B_FALSE			/* leaf vdev */
};

static void
vdev_indirect_remap(vdev_t *vd, uint64_t offset, uint64_t asize,
    void (*func)(uint64_t, vdev_t *, uint64_t, uint64_t, void *), void *arg)


/* Stub implementation for vdev_indirect_remap */
void vdev_indirect_remap(vdev_t *vd, uint64_t offset, uint64_t asize, void (*func)(uint64_t, vdev_t *, uint64_t, uint64_t, void *), void *arg) {}
