/* CG-Bench fixture: fnptr-callback/example_13 */
/* fnptr: valueize, targets: pta_valueize, threadedge_valueize, vn_valueize, dom_valueize, valueize_val, valueize_op, do_valueize */

tree
gimple_fold_stmt_to_constant_1 (gimple *stmt, tree (*valueize) (tree),
				tree (*gvalueize) (tree))
{
  gimple_match_op res_op;
  ...
	if (gimple_call_internal_p (stmt))
	  {
	    tree arg0 = gimple_call_arg (stmt, 0);
	    tree arg1 = gimple_call_arg (stmt, 1);
	    tree op0 = (*valueize) (arg0);
	    tree op1 = (*valueize) (arg1);
      }
   ...
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
object_sizes_execute (function *fun, bool early)
{
    basic_block bb;
    ...

    result = gimple_fold_stmt_to_constant (call, do_valueize);
    ...
}

static tree
ccp_fold (gimple *stmt)
{
  switch (gimple_code (stmt))
    {
    case GIMPLE_SWITCH:
      {
	/* Return the constant switch index.  */
        return valueize_op (gimple_switch_index (as_a <gswitch *> (stmt)));
      }

    case GIMPLE_COND:
    case GIMPLE_ASSIGN:
    case GIMPLE_CALL:
      return gimple_fold_stmt_to_constant_1 (stmt,
					     valueize_op, valueize_op_1);

    default:
      gcc_unreachable ();
    }
}

static enum ssa_prop_result
copy_prop_visit_assignment (gimple *stmt, tree *result_p)
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
}

static void
back_propagate_equivalences (tree lhs, edge e,
			     class const_and_copies *const_and_copies,
			     bitmap domby)
{
  ...

        tree res = gimple_fold_stmt_to_constant_1 (use_stmt, dom_valueize,
                            no_follow_ssa_edges);
        if (res && (TREE_CODE (res) == SSA_NAME || is_gimple_min_invariant (res)))
    record_equality (lhs2, res, const_and_copies);
  ...
}

static tree
try_to_simplify (gassign *stmt)
{
  enum tree_code code = gimple_assign_rhs_code (stmt);
  tree tem;

  /* For stores we can end up simplifying a SSA_NAME rhs.  Just return
     in this case, there is no point in doing extra work.  */
  if (code == SSA_NAME)
    return NULL_TREE;

  /* First try constant folding based on our current lattice.  */
  mprts_hook = vn_lookup_simplify_result;
  tem = gimple_fold_stmt_to_constant_1 (stmt, vn_valueize, vn_valueize);
  mprts_hook = NULL;
  if (tem
      && (TREE_CODE (tem) == SSA_NAME
	  || is_gimple_min_invariant (tem)))
    return tem;

  return NULL_TREE;
}

static bool
visit_stmt (gimple *stmt, bool backedges_varying_p = false)
{
  bool changed = false;
  ...
  	  tree simplified = gimple_fold_stmt_to_constant_1 (call_stmt,
							    vn_valueize);
	  if (simplified)
	    {
	      if (dump_file && (dump_flags & TDF_DETAILS))
		{
		  fprintf (dump_file, "call ");
		  print_gimple_expr (dump_file, call_stmt, 0);
		  fprintf (dump_file, " simplified to ");
		  print_generic_expr (dump_file, simplified);
		  fprintf (dump_file, "\n");
		}
	    }
  ...
}

void
jt_state::register_equivs_stmt (gimple *stmt, basic_block bb,
				jt_simplifier *simplifier)
{
     tree cached_lhs = NULL;
     ...
  if (gimple_assign_single_p (stmt)
      && TREE_CODE (gimple_assign_rhs1 (stmt)) == SSA_NAME)
    cached_lhs = gimple_assign_rhs1 (stmt);
  else
    {
        ...
      cached_lhs = gimple_fold_stmt_to_constant_1 (stmt, threadedge_valueize);
    }
    ...
}

void
pointer_equiv_analyzer::visit_stmt (gimple *stmt)
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

  // If we couldn't find anything, try fold.
  x_fold_context = { stmt, m_ranger, this};
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


/* Wrapper: calls through valueize */
void valueize_caller(tree) {
    valueize(tree);
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
void valueize_op(gimple_switch_index (as_a <gswitch *> (stmt))) {}



/* Stub implementation for do_valueize */
void do_valueize(void) {}
