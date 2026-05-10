/* et_bench fixture: fnptr-global-struct/example_9 */
/* fnptr: stream_read_tree (macro -> streamer_hooks.read_tree), targets: lto_input_tree */

#include <stddef.h>

/* Minimal forward declarations */
typedef struct tree_node *tree;
typedef struct function function;
#define NULL_TREE ((tree)0)

struct lto_input_block {
    const char *data;
    size_t pos;
    size_t len;
};

struct data_in {
    void *reader_data;
};

function *cfun;

/* Streamer hooks struct with function pointer members */
struct streamer_hooks {
    void (*write_tree)(struct lto_input_block *, tree);
    tree (*read_tree)(struct lto_input_block *, struct data_in *);
    void (*input_location)(struct lto_input_block *, struct data_in *, void *loc);
    void (*output_location)(struct lto_input_block *, void *loc);
    void (*output_location_and_block)(struct lto_input_block *, void *loc, void *block);
} streamer_hooks;

/* Macro that delegates to the hooks struct */
#define stream_read_tree(IB, DATA_IN) \
    streamer_hooks.read_tree(IB, DATA_IN)

unsigned int streamer_read_uhwi(struct lto_input_block *ib) { return 0; }
unsigned char streamer_read_uchar(struct lto_input_block *ib) { return 0; }
void streamer_hooks_init(void) {}

/* Target: lto_input_tree — assigned to streamer_hooks.read_tree */
tree lto_input_tree(struct lto_input_block *ib, struct data_in *data_in)
{
    return NULL_TREE;
}

void lto_output_tree(struct lto_input_block *ib, tree t) {}
void lto_input_location(struct lto_input_block *ib, struct data_in *di, void *loc) {}
void lto_output_location(struct lto_input_block *ib, void *loc) {}
void lto_output_location_and_block(struct lto_input_block *ib, void *loc, void *blk) {}

/* SSA name list */
tree *SSANAMES(function *fn) { return NULL; }
void init_tree_ssa(function *fn, unsigned int size) {}
void init_ssa_operands(function *fn) {}
tree make_ssa_name_fn(function *fn, tree var, void *stmt) { return NULL_TREE; }
tree SSA_NAME_VAR(tree ssa) { return NULL_TREE; }
void set_ssa_default_def(function *fn, tree var, tree ssa) {}
void *gimple_build_nop(void) { return NULL; }
#define SSA_NAME_DEF_STMT(ssa) (*(void **)0)

void
input_ssa_names(struct lto_input_block *ib, struct data_in *data_in,
                function *fn)
{
    unsigned int i, size;

    size = streamer_read_uhwi(ib);
    init_tree_ssa(fn, size);
    cfun->flags |= 1;  /* in_ssa_p */
    init_ssa_operands(fn);

    i = streamer_read_uhwi(ib);
    while (i) {
        tree ssa_name, name;
        int is_default_def;

        /* Skip over the elements that had been freed. */
        is_default_def = (streamer_read_uchar(ib) != 0);
        name = stream_read_tree(ib, data_in);
        ssa_name = make_ssa_name_fn(fn, name, NULL);

        if (is_default_def) {
            set_ssa_default_def(cfun, SSA_NAME_VAR(ssa_name), ssa_name);
            SSA_NAME_DEF_STMT(ssa_name) = gimple_build_nop();
        }

        i = streamer_read_uhwi(ib);
    }
}

void
lto_streamer_hooks_init(void)
{
    streamer_hooks_init();
    streamer_hooks.write_tree = lto_output_tree;
    streamer_hooks.read_tree = lto_input_tree;
    streamer_hooks.input_location = lto_input_location;
    streamer_hooks.output_location = lto_output_location;
    streamer_hooks.output_location_and_block = lto_output_location_and_block;
}
