/* ET-Bench fixture: fnptr-virtual/example_1 */
/* fnptr: get_state_map_by_name, targets: noop_region_model_context::get_state_map_by_name, region_model_context_decorator::get_state_map_by_name */
/* Pattern: C++ virtual method dispatch — base class pure virtual, derived classes override, called through base pointer.
   Rewritten in C with explicit vtable to be tree-sitter-c parseable while preserving the same caller/callee/target names. */

#include <stdlib.h>
#include <string.h>

/* Opaque forward declarations (stand-ins for GCC-internal types) */
struct pending_diagnostic;
struct pending_note;
struct svalue;
struct svalue_set;
struct region_model;
struct logger;
struct gphi;
struct tree_node;
struct bounded_ranges;
struct frame_region;
struct dump_location;
struct sm_state_map;
struct state_machine;
struct sm_context;
struct custom_edge_info;
struct gimple;
struct extrinsic_state;
struct uncertainty;

/* === Base class vtable (simulates C++ virtual method table) === */
typedef struct region_model_context region_model_context;

typedef int (*vfn_warn)(region_model_context *self, struct pending_diagnostic *d);
typedef void (*vfn_add_note)(region_model_context *self, struct pending_note *pn);
typedef void (*vfn_on_svalue_leak)(region_model_context *self, struct svalue *sval);
typedef void (*vfn_on_liveness_change)(region_model_context *self, struct svalue_set *live, const struct region_model *model);
typedef struct logger *(*vfn_get_logger)(region_model_context *self);
typedef void (*vfn_on_condition)(region_model_context *self, struct svalue *lhs, int op, struct svalue *rhs);
typedef void (*vfn_on_bounded_ranges)(region_model_context *self, struct svalue *sval, struct bounded_ranges *ranges);
typedef void (*vfn_on_pop_frame)(region_model_context *self, struct frame_region *fr);
typedef void (*vfn_on_unknown_change)(region_model_context *self, struct svalue *sval, int is_mutable);
typedef void (*vfn_on_phi)(region_model_context *self, struct gphi *phi, struct tree_node *rhs);
typedef void (*vfn_on_unexpected_tree_code)(region_model_context *self, struct tree_node *t, struct dump_location *loc);
typedef void (*vfn_on_escaped_function)(region_model_context *self, struct tree_node *fndecl);
typedef struct uncertainty *(*vfn_get_uncertainty)(region_model_context *self);
typedef void (*vfn_purge_state_involving)(region_model_context *self, struct svalue *sval);
typedef void (*vfn_bifurcate)(region_model_context *self, struct custom_edge_info *info);
typedef void (*vfn_terminate_path)(region_model_context *self);
typedef struct extrinsic_state *(*vfn_get_ext_state)(region_model_context *self);
typedef int (*vfn_get_state_map_by_name)(region_model_context *self,
                                          const char *name,
                                          struct sm_state_map **out_smap,
                                          const struct state_machine **out_sm,
                                          unsigned *out_sm_idx,
                                          struct sm_context **out_sm_context);
typedef const struct gimple *(*vfn_get_stmt)(region_model_context *self);

struct region_model_context_vtable {
    vfn_warn warn;
    vfn_add_note add_note;
    vfn_on_svalue_leak on_svalue_leak;
    vfn_on_liveness_change on_liveness_change;
    vfn_get_logger get_logger;
    vfn_on_condition on_condition;
    vfn_on_bounded_ranges on_bounded_ranges;
    vfn_on_pop_frame on_pop_frame;
    vfn_on_unknown_change on_unknown_change;
    vfn_on_phi on_phi;
    vfn_on_unexpected_tree_code on_unexpected_tree_code;
    vfn_on_escaped_function on_escaped_function;
    vfn_get_uncertainty get_uncertainty;
    vfn_purge_state_involving purge_state_involving;
    vfn_bifurcate bifurcate;
    vfn_terminate_path terminate_path;
    vfn_get_ext_state get_ext_state;
    /* The virtual method we track */
    vfn_get_state_map_by_name get_state_map_by_name;
    vfn_get_stmt get_stmt;
};

struct region_model_context {
    const struct region_model_context_vtable *vtable;
};

/* === Derived class: noop_region_model_context === */
/* Overrides all virtual methods with no-op implementations */

struct noop_region_model_context {
    struct region_model_context base;
};

static int noop_warn(region_model_context *self, struct pending_diagnostic *d) { return 0; }
static void noop_add_note(region_model_context *self, struct pending_note *pn) {}
static void noop_on_svalue_leak(region_model_context *self, struct svalue *sval) {}
static void noop_on_liveness_change(region_model_context *self, struct svalue_set *live, const struct region_model *model) {}
static struct logger *noop_get_logger(region_model_context *self) { return NULL; }
static void noop_on_condition(region_model_context *self, struct svalue *lhs, int op, struct svalue *rhs) {}
static void noop_on_bounded_ranges(region_model_context *self, struct svalue *sval, struct bounded_ranges *ranges) {}
static void noop_on_pop_frame(region_model_context *self, struct frame_region *fr) {}
static void noop_on_unknown_change(region_model_context *self, struct svalue *sval, int is_mutable) {}
static void noop_on_phi(region_model_context *self, struct gphi *phi, struct tree_node *rhs) {}
static void noop_on_unexpected_tree_code(region_model_context *self, struct tree_node *t, struct dump_location *loc) {}
static void noop_on_escaped_function(region_model_context *self, struct tree_node *fndecl) {}
static struct uncertainty *noop_get_uncertainty(region_model_context *self) { return NULL; }
static void noop_purge_state_involving(region_model_context *self, struct svalue *sval) {}
static void noop_bifurcate(region_model_context *self, struct custom_edge_info *info) {}
static void noop_terminate_path(region_model_context *self) {}
static struct extrinsic_state *noop_get_ext_state(region_model_context *self) { return NULL; }
static const struct gimple *noop_get_stmt(region_model_context *self) { return NULL; }

/* noop_region_model_context::get_state_map_by_name — always returns false */
static int noop_region_model_context_get_state_map_by_name(
    region_model_context *self,
    const char *name,
    struct sm_state_map **out_smap,
    const struct state_machine **out_sm,
    unsigned *out_sm_idx,
    struct sm_context **out_sm_context)
{
    return 0;
}

static const struct region_model_context_vtable noop_vtable = {
    noop_warn,
    noop_add_note,
    noop_on_svalue_leak,
    noop_on_liveness_change,
    noop_get_logger,
    noop_on_condition,
    noop_on_bounded_ranges,
    noop_on_pop_frame,
    noop_on_unknown_change,
    noop_on_phi,
    noop_on_unexpected_tree_code,
    noop_on_escaped_function,
    noop_get_uncertainty,
    noop_purge_state_involving,
    noop_bifurcate,
    noop_terminate_path,
    noop_get_ext_state,
    noop_region_model_context_get_state_map_by_name,
    noop_get_stmt,
};

void noop_region_model_context_init(struct noop_region_model_context *ctx)
{
    ctx->base.vtable = &noop_vtable;
}

/* === Derived class: region_model_context_decorator === */
/* Decorator: forwards all virtual calls to an inner context */

struct region_model_context_decorator {
    struct region_model_context base;
    region_model_context *inner;
};

static int decorator_warn(region_model_context *self, struct pending_diagnostic *d) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    return ctx->inner->vtable->warn(ctx->inner, d);
}
static void decorator_add_note(region_model_context *self, struct pending_note *pn) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->add_note(ctx->inner, pn);
}
static void decorator_on_svalue_leak(region_model_context *self, struct svalue *sval) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_svalue_leak(ctx->inner, sval);
}
static void decorator_on_liveness_change(region_model_context *self, struct svalue_set *live, const struct region_model *model) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_liveness_change(ctx->inner, live, model);
}
static struct logger *decorator_get_logger(region_model_context *self) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    return ctx->inner->vtable->get_logger(ctx->inner);
}
static void decorator_on_condition(region_model_context *self, struct svalue *lhs, int op, struct svalue *rhs) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_condition(ctx->inner, lhs, op, rhs);
}
static void decorator_on_bounded_ranges(region_model_context *self, struct svalue *sval, struct bounded_ranges *ranges) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_bounded_ranges(ctx->inner, sval, ranges);
}
static void decorator_on_pop_frame(region_model_context *self, struct frame_region *fr) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_pop_frame(ctx->inner, fr);
}
static void decorator_on_unknown_change(region_model_context *self, struct svalue *sval, int is_mutable) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_unknown_change(ctx->inner, sval, is_mutable);
}
static void decorator_on_phi(region_model_context *self, struct gphi *phi, struct tree_node *rhs) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_phi(ctx->inner, phi, rhs);
}
static void decorator_on_unexpected_tree_code(region_model_context *self, struct tree_node *t, struct dump_location *loc) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_unexpected_tree_code(ctx->inner, t, loc);
}
static void decorator_on_escaped_function(region_model_context *self, struct tree_node *fndecl) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->on_escaped_function(ctx->inner, fndecl);
}
static struct uncertainty *decorator_get_uncertainty(region_model_context *self) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    return ctx->inner->vtable->get_uncertainty(ctx->inner);
}
static void decorator_purge_state_involving(region_model_context *self, struct svalue *sval) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->purge_state_involving(ctx->inner, sval);
}
static void decorator_bifurcate(region_model_context *self, struct custom_edge_info *info) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->bifurcate(ctx->inner, info);
}
static void decorator_terminate_path(region_model_context *self) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    ctx->inner->vtable->terminate_path(ctx->inner);
}
static struct extrinsic_state *decorator_get_ext_state(region_model_context *self) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    return ctx->inner->vtable->get_ext_state(ctx->inner);
}

/* region_model_context_decorator::get_state_map_by_name — forwards to inner */
static int region_model_context_decorator_get_state_map_by_name(
    region_model_context *self,
    const char *name,
    struct sm_state_map **out_smap,
    const struct state_machine **out_sm,
    unsigned *out_sm_idx,
    struct sm_context **out_sm_context)
{
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    return ctx->inner->vtable->get_state_map_by_name(ctx->inner, name, out_smap, out_sm, out_sm_idx, out_sm_context);
}

static const struct gimple *decorator_get_stmt(region_model_context *self) {
    struct region_model_context_decorator *ctx = (struct region_model_context_decorator *)self;
    return ctx->inner->vtable->get_stmt(ctx->inner);
}

static const struct region_model_context_vtable decorator_vtable = {
    decorator_warn,
    decorator_add_note,
    decorator_on_svalue_leak,
    decorator_on_liveness_change,
    decorator_get_logger,
    decorator_on_condition,
    decorator_on_bounded_ranges,
    decorator_on_pop_frame,
    decorator_on_unknown_change,
    decorator_on_phi,
    decorator_on_unexpected_tree_code,
    decorator_on_escaped_function,
    decorator_get_uncertainty,
    decorator_purge_state_involving,
    decorator_bifurcate,
    decorator_terminate_path,
    decorator_get_ext_state,
    region_model_context_decorator_get_state_map_by_name,
    decorator_get_stmt,
};

void region_model_context_decorator_init(struct region_model_context_decorator *ctx,
                                          region_model_context *inner)
{
    ctx->base.vtable = &decorator_vtable;
    ctx->inner = inner;
}

/* === get_fd_map: the caller function === */
/* Equivalent to region_model_context::get_fd_map in the original C++ code.
   Invokes the virtual method get_state_map_by_name through the base pointer.
   At runtime this dispatches to either:
     - noop_region_model_context_get_state_map_by_name
     - region_model_context_decorator_get_state_map_by_name
*/
int get_fd_map(region_model_context *ctx,
               struct sm_state_map **out_smap,
               const struct state_machine **out_sm,
               unsigned *out_sm_idx,
               struct sm_context **out_sm_context)
{
    return ctx->vtable->get_state_map_by_name(ctx, "file-descriptor", out_smap, out_sm, out_sm_idx, out_sm_context);
}
