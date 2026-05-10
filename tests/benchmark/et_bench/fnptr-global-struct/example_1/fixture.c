/* et_bench fixture: fnptr-global-struct/example_1 */
/* fnptr: vdev_indirect_ops.vdev_op_remap, targets: vdev_indirect_remap */

#include <stdint.h>

typedef struct vdev vdev_t;
typedef void vdev_remap_cb_t(uint64_t, vdev_t *, uint64_t, uint64_t, void *);

struct vdev_ops {
    int (*vdev_op_init)(void);
    void (*vdev_op_fini)(void);
    int (*vdev_op_open)(vdev_t *);
    void (*vdev_op_close)(vdev_t *);
    uint64_t (*vdev_op_asize)(vdev_t *);
    uint64_t (*vdev_op_min_asize)(vdev_t *);
    uint64_t (*vdev_op_min_alloc)(void);
    void (*vdev_op_io_start)(vdev_t *);
    void (*vdev_op_io_done)(vdev_t *);
    void (*vdev_op_state_change)(vdev_t *, int);
    int (*vdev_op_need_resilver)(vdev_t *);
    void (*vdev_op_hold)(vdev_t *);
    void (*vdev_op_rele)(vdev_t *);
    void (*vdev_op_remap)(vdev_t *, uint64_t, uint64_t,
                          vdev_remap_cb_t *, void *);
    int (*vdev_op_xlate)(vdev_t *, uint64_t);
    void (*vdev_op_rebuild_asize)(vdev_t *);
    void (*vdev_op_metaslab_init)(vdev_t *);
    void (*vdev_op_config_generate)(vdev_t *);
    int (*vdev_op_nparity)(vdev_t *);
    int (*vdev_op_ndisks)(vdev_t *);
    const char *vdev_op_type;
    int vdev_op_leaf;
};

struct vdev {
    vdev_ops_t *v_ops;
    uint64_t v_offset;
};

static void claim_segment_impl(uint64_t off, vdev_t *vd,
                                uint64_t size, uint64_t asize, void *arg);

static void
claim_segment_cb(void *arg, uint64_t offset, uint64_t size)
{
    vdev_t *vd = (vdev_t *)arg;
    vdev_indirect_ops.vdev_op_remap(vd, offset, size,
        claim_segment_impl, NULL);
}

void
vdev_indirect_remap(vdev_t *vd, uint64_t offset, uint64_t asize,
    void (*func)(uint64_t, vdev_t *, uint64_t, uint64_t, void *), void *arg)
{
    func(offset, vd, asize, offset, arg);
}

vdev_ops_t vdev_indirect_ops = {
    .vdev_op_init = NULL,
    .vdev_op_fini = NULL,
    .vdev_op_open = NULL,
    .vdev_op_close = NULL,
    .vdev_op_asize = NULL,
    .vdev_op_min_asize = NULL,
    .vdev_op_min_alloc = NULL,
    .vdev_op_io_start = NULL,
    .vdev_op_io_done = NULL,
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
    .vdev_op_type = "indirect",
    .vdev_op_leaf = 0
};
