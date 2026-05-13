# ET-Bench Ground Truth Fix Report

**Fix principle**: Caller should be the function that directly calls through the function pointer (immediate fnptr-calling function), not the outer function that passes the fnptr as an argument.

**Total**: 10 examples fixed, 26 GT entries corrected, 3 duplicate entries removed after fix.
4 additional examples reverted (7 entries) — ethunter's Pass 3 cannot currently detect the immediate caller; marked as PASS 3 COVERAGE GAP.

## Fixed Examples

### fnptr-callback/example_1
- `(Curl_auth_create_digest_http_message, auth_digest_md5_to_ascii)` → `(auth_create_digest_http_message, auth_digest_md5_to_ascii)`
- `(Curl_auth_create_digest_http_message, auth_digest_sha256_to_ascii)` → `(auth_create_digest_http_message, auth_digest_sha256_to_ascii)`

### fnptr-callback/example_3
- `(nfs_is_shared_impl, nfs_is_shared_cb)` → `(nfs_process_exports, nfs_is_shared_cb)`
- `(nfs_copy_entries, nfs_copy_entries_cb)` → `(nfs_process_exports, nfs_copy_entries_cb)`

### fnptr-callback/example_4
- `(avcodec_find_decoder, av_codec_is_decoder)` → `(find_codec, av_codec_is_decoder)`
- `(avcodec_find_encoder, av_codec_is_encoder)` → `(find_codec, av_codec_is_encoder)`

### fnptr-callback/example_5
- `(quicklistPop, _quicklistSaver)` → `(quicklistPopCustom, _quicklistSaver)`
- `(listTypePop, listPopSaver)` → `(quicklistPopCustom, listPopSaver)`

### fnptr-callback/example_8
- `(georadiusGeneric, sort_gp_asc)` — kept (immediate caller _pqsort not detected)
- `(georadiusGeneric, sort_gp_desc)` — kept (immediate caller _pqsort not detected)
- `(sortCommandGeneric, sortCompare)` → `(_pqsort, sortCompare)`

### fnptr-callback/example_9
- `(addReplyCommandInfo, addReplyCommandInfo)` → `(addReplyCommandSubCommands, addReplyCommandInfo)`
- `(addReplyCommandDocs, addReplyCommandDocs)` → `(addReplyCommandSubCommands, addReplyCommandDocs)`

### fnptr-callback/example_10
- `(aes_encrypt_contiguous_blocks, aes_encrypt_block)` → `(ccm_mode_encrypt_contiguous_blocks, aes_encrypt_block)`

### fnptr-callback/example_11
- `(aes_encrypt_atomic, aes_xor_block)`, `(aes_encrypt_final, aes_xor_block)`, `(aes_encrypt, aes_xor_block)`
  → Deduplicated to single `(ccm_encrypt_final, aes_xor_block)` (3→1)

### fnptr-callback/example_12
- `(Curl_auth_create_digest_http_message, Curl_md5it)` → `(auth_create_digest_http_message, Curl_md5it)`
- `(Curl_auth_create_digest_http_message, Curl_sha256it)` → `(auth_create_digest_http_message, Curl_sha256it)`

### fnptr-callback/example_13
- `(ccp_fold, valueize_op)` → `(gimple_fold_stmt_to_constant_1, valueize_op)`
- `(copy_prop_visit_assignment, valueize_val)` → `(gimple_fold_stmt_to_constant_1, valueize_val)`
- `(back_propagate_equivalences, dom_valueize)` → `(gimple_fold_stmt_to_constant_1, dom_valueize)`
- `(try_to_simplify, vn_valueize)` → `(gimple_fold_stmt_to_constant_1, vn_valueize)`
- `(visit_stmt, vn_valueize)` → `(gimple_fold_stmt_to_constant_1, vn_valueize)` (deduped)
- `(jt_state_register_equivs_stmt, threadedge_valueize)` → `(gimple_fold_stmt_to_constant_1, threadedge_valueize)`
- `(pointer_equiv_analyzer_visit_stmt, pta_valueize)` → `(gimple_fold_stmt_to_constant_1, pta_valueize)`
- `(object_sizes_execute, do_valueize)` → `(gimple_fold_stmt_to_constant_1, do_valueize)`

### fnptr-callback/example_15
- `(source, scpio)` → `(atomicio6, scpio)`
- `(get_msg_extended, sftpio)` → `(atomicio6, sftpio)`

## Reverted (Pass 3 Coverage Gap)

以下 example 的 immediate caller ethunter 当前无法检测（Pass 3 不覆盖），保持原 caller：

| Example | Original Caller | Callee | Immediate Caller | 原因 |
|---------|----------------|--------|-----------------|------|
| fnptr-callback/example_14 | `gt_pch_save` | `relocate_ptrs` | `gt_pch_p_14lang_tree_node` | Pass 3 未检测到 inner fnptr call |
| fnptr-cast/example_7 | `abd_fletcher_4_iter` | `fletcher_4_*` (3) | `abd_iterate_func` | 由 field_call 覆盖，inner caller 不产出 |
| fnptr-global-struct/example_1 | `claim_segment_cb` | `vdev_indirect_remap` | `vdev_indirect_remap` | inner=callee（自引用 dispatch） |
| fnptr-callback/example_8 | `georadiusGeneric` | `sort_gp_*` (2) | `_pqsort` | Pass 3 未检测 typedef fnptr param |

## 验证结果

- 145 tests pass, 0 failures
- 9 target scenarios all 100% recall
- 3 GT duplicates correctly removed (example_11 ×2, example_13 ×1)
