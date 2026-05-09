/* CG-Bench fixture: fnptr-callback/example_13 */
/* fnptr: valueize, targets: pta_valueize, threadedge_valueize, vn_valueize, dom_valueize, valueize_val, valueize_op, do_valueize */

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
      }
   return NULL_TREE;
}

tree
gimple_fold_stmt_to_constant (gimple *stmt, tree (*valueize) (tree))
{
  tree res = gimple_fold_stmt_to_constant_1 (stmt, valueize);
  if (res && is_gimple_min_invariant (res))
    return res;
  return NULL_TREE;
}

static unsigned int
object_sizes_execute (int fun, int early)
{
    int result;

    result = gimple_fold_stmt_to_constant (NULL, do_valueize);
    return result;
}

static tree
ccp_fold (gimple *stmt)
{
  switch (gimple_code (stmt))
    {
    case GIMPLE_SWITCH:
      return valueize_op (gimple_switch_index (stmt));

    case GIMPLE_COND:
    case GIMPLE_ASSIGN:
    case GIMPLE_CALL:
      return gimple_fold_stmt_to_constant_1 (stmt,
					     valueize_op, valueize_op_1);

    default:
      return NULL_TREE;
    }
}

static int
copy_prop_visit_assignment (gimple *stmt)
{
  tree lhs = gimple_assign_lhs (stmt);
  tree rhs = gimple_fold_stmt_to_constant_1 (stmt, valueize_val);
  if (rhs
      && (TREE_CODE (rhs) == SSA_NAME
	  || is_gimple_min_invariant (rhs)))
    {
      if (!may_propagate_copy (lhs, rhs))
	rhs = lhs;
    }
  else
    rhs = lhs;
  return 0;
}

static void
back_propagate_equivalences (tree lhs, int e,
				     void *const_and_copies,
				     int domby)
{
        tree res = gimple_fold_stmt_to_constant_1 (NULL, dom_valueize,
                            no_follow_ssa_edges);
        if (res && (TREE_CODE (res) == SSA_NAME || is_gimple_min_invariant (res)))
    record_equality (lhs, res, const_and_copies);
}

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
      && (TREE_CODE (tem) == SSA_NAME
	  || is_gimple_min_invariant (tem)))
    return tem;

  return NULL_TREE;
}

static int
visit_stmt (gimple *stmt)
{
  int changed = 0;
 	  tree simplified = gimple_fold_stmt_to_constant_1 (stmt,
							    vn_valueize);
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
     tree cached_lhs = NULL;
  if (gimple_assign_single_p (stmt)
      && TREE_CODE (gimple_assign_rhs1 (stmt)) == SSA_NAME)
    cached_lhs = gimple_assign_rhs1 (stmt);
  else
    {
      cached_lhs = gimple_fold_stmt_to_constant_1 (stmt, threadedge_valueize);
    }
}

void
pointer_equiv_analyzer_visit_stmt (gimple *stmt)
{
  if (gimple_code (stmt) != GIMPLE_ASSIGN)
    return;

  tree lhs = gimple_assign_lhs (stmt);
  if (!supported_pointer_equiv_p (lhs))
    return;

  tree rhs = gimple_assign_rhs1 (stmt);
  rhs = get_equiv_expr (gimple_assign_rhs_code (stmt), rhs);
  if (rhs)
    {
      set_global_equiv (lhs, rhs);
      return;
    }

  rhs = gimple_fold_stmt_to_constant_1 (stmt, pta_valueize, pta_valueize);
  if (rhs)
    {
      rhs = get_equiv_expr (TREE_CODE (rhs), rhs);
      if (rhs)
	{
	  set_global_equiv (lhs, rhs);
	  return;
	}
    }
}

/* Stub implementation for pta_valueize */
void pta_valueize(void) {}

/* Stub implementation for threadedge_valueize */
void threadedge_valueize(void) {}

/* Stub implementation for vn_valueize */
void vn_valueize(void) {}

/* Stub implementation for dom_valueize */
void dom_valueize(void) {}

/* Stub implementation for valueize_val */
void valueize_val(void) {}

/* Stub implementation for valueize_op */
void valueize_op(void) {}

/* Stub implementation for do_valueize */
void do_valueize(void) {}
