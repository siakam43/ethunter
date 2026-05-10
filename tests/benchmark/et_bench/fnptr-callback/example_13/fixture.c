/* ET-Bench fixture: fnptr-callback/example_13 */
/* Based on GCC's gimple_fold_stmt_to_constant_1 pattern */
/* fnptr: valueize, targets: pta_valueize, threadedge_valueize, vn_valueize, dom_valueize, valueize_val, valueize_op, do_valueize */

#include <stddef.h>
#include <stdlib.h>

enum tree_code {
    SSA_NAME,
    TREE_CODE_BASE,
    TREE_CODE_SWITCH,
    TREE_CODE_COND,
    TREE_CODE_ASSIGN,
    TREE_CODE_CALL
};

typedef struct tree_node *tree;
#define NULL_TREE ((tree)0)

static int is_gimple_min_invariant(tree t) { return t == NULL_TREE; }

typedef struct gimple {
    int code;
    int internal_p;
    tree args[4];
    tree lhs;
    enum tree_code rhs_code;
} gimple;

static int gimple_call_internal_p(gimple *stmt) { return stmt->internal_p; }
static tree gimple_call_arg(gimple *stmt, int i) { return stmt->args[i]; }
static int gimple_code(gimple *stmt) { return stmt->code; }
static tree gimple_assign_lhs(gimple *stmt) { return stmt->lhs; }
static enum tree_code gimple_assign_rhs_code(gimple *stmt) { return stmt->rhs_code; }

typedef struct gimple_match_op {
    int code;
    tree ops[3];
} gimple_match_op;

tree
gimple_fold_stmt_to_constant_1 (gimple *stmt, tree (*valueize) (tree),
                                tree (*gvalueize) (tree))
{
    gimple_match_op res_op;
    if (gimple_call_internal_p (stmt))
    {
        tree arg0 = gimple_call_arg (stmt, 0);
        tree arg1 = gimple_call_arg (stmt, 1);
        tree op0 = (*valueize) (arg0);
        tree op1 = (*valueize) (arg1);
        (void)op0;
        (void)op1;
    }
    return NULL_TREE;
}

tree
gimple_fold_stmt_to_constant (gimple *stmt, tree (*valueize) (tree))
{
    tree res = gimple_fold_stmt_to_constant_1 (stmt, valueize, valueize);
    if (res && is_gimple_min_invariant (res))
        return res;
    return NULL_TREE;
}

static unsigned int
object_sizes_execute (int fun, int early)
{
    int result;

    result = (int)(intptr_t)gimple_fold_stmt_to_constant (NULL, do_valueize);
    return (unsigned int)result;
}

static tree
ccp_fold (gimple *stmt)
{
    switch (gimple_code (stmt))
    {
    case TREE_CODE_SWITCH:
        return valueize_op (gimple_call_arg (stmt, 0));

    case TREE_CODE_COND:
    case TREE_CODE_ASSIGN:
    case TREE_CODE_CALL:
        return gimple_fold_stmt_to_constant_1 (stmt,
                            valueize_op, valueize_op);

    default:
        return NULL_TREE;
    }
}

static int may_propagate_copy(tree lhs, tree rhs) { return 1; }

static int
copy_prop_visit_assignment (gimple *stmt)
{
    tree lhs = gimple_assign_lhs (stmt);
    tree rhs = gimple_fold_stmt_to_constant_1 (stmt, valueize_val, valueize_val);
    if (rhs
        && (rhs == NULL_TREE
            || is_gimple_min_invariant (rhs)))
    {
        if (!may_propagate_copy (lhs, rhs))
            rhs = lhs;
    }
    else
        rhs = lhs;
    return 0;
}

static void record_equality(tree lhs, tree res, void *copies) {}

static void
back_propagate_equivalences (tree lhs, int e,
                             void *const_and_copies,
                             int domby)
{
    tree no_follow_ssa_edges = NULL_TREE;
    tree res = gimple_fold_stmt_to_constant_1 (NULL, dom_valueize,
                        no_follow_ssa_edges);
    if (res && (res == NULL_TREE || is_gimple_min_invariant (res)))
        record_equality (lhs, res, const_and_copies);
}

tree vn_lookup_simplify_result(void) { return NULL_TREE; }
static void *mprts_hook = NULL;

static tree
try_to_simplify (gimple *stmt)
{
    enum tree_code code = gimple_assign_rhs_code (stmt);
    tree tem;

    if (code == SSA_NAME)
        return NULL_TREE;

    mprts_hook = vn_lookup_simplify_result;
    tem = gimple_fold_stmt_to_constant_1 (stmt, vn_valueize, vn_valueize);
    mprts_hook = NULL;
    if (tem
        && (tem == NULL_TREE
            || is_gimple_min_invariant (tem)))
        return tem;

    return NULL_TREE;
}

static tree dump_file;
static void print_generic_expr(tree t) {}

static int
visit_stmt (gimple *stmt)
{
    int changed = 0;
    tree simplified = gimple_fold_stmt_to_constant_1 (stmt,
                                vn_valueize, vn_valueize);
    if (simplified)
    {
        if (dump_file)
        {
            print_generic_expr (simplified);
        }
    }
    return changed;
}

void
jt_state_register_equivs_stmt (gimple *stmt, int bb,
                void *simplifier)
{
    tree cached_lhs = NULL_TREE;
    if (gimple_assign_lhs (stmt) != NULL_TREE
        && gimple_assign_rhs_code (stmt) == SSA_NAME)
        cached_lhs = gimple_assign_lhs (stmt);
    else
    {
        cached_lhs = gimple_fold_stmt_to_constant_1 (stmt, threadedge_valueize, threadedge_valueize);
    }
    (void)cached_lhs;
}

int supported_pointer_equiv_p(tree t) { return 0; }
tree get_equiv_expr(enum tree_code code, tree rhs) { return NULL_TREE; }
void set_global_equiv(tree lhs, tree rhs) {}

void
pointer_equiv_analyzer_visit_stmt (gimple *stmt)
{
    if (gimple_code (stmt) != TREE_CODE_ASSIGN)
        return;

    tree lhs = gimple_assign_lhs (stmt);
    if (!supported_pointer_equiv_p (lhs))
        return;

    tree rhs = gimple_assign_lhs (stmt);
    rhs = get_equiv_expr (gimple_assign_rhs_code (stmt), rhs);
    if (rhs)
    {
        set_global_equiv (lhs, rhs);
        return;
    }

    rhs = gimple_fold_stmt_to_constant_1 (stmt, pta_valueize, pta_valueize);
    if (rhs)
    {
        rhs = get_equiv_expr (TREE_CODE_BASE, rhs);
        if (rhs)
        {
            set_global_equiv (lhs, rhs);
            return;
        }
    }
}

tree pta_valueize(tree t) { return t; }
tree threadedge_valueize(tree t) { return t; }
tree vn_valueize(tree t) { return t; }
tree dom_valueize(tree t) { return t; }
tree valueize_val(tree t) { return t; }
tree valueize_op(tree t) { return t; }
tree do_valueize(tree t) { return t; }
