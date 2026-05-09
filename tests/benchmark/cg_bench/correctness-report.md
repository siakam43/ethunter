# CG-Bench Fixture Correctness Report

## Summary

| Metric | Count |
|--------|-------|
| **Total fixtures** | 104 |
| **Syntax PASS** | 32 (30.8%) |
| **Syntax FAIL** | 72 (69.2%) |
| **Overall PASS** (syntax + semantic) | 17 (16.3%) |
| **FAIL-SYNTAX** | 60 (57.7%) |
| **FAIL-SEMANTIC** | 10 (9.6%) |
| **FAIL-BOTH** | 17 (16.3%) |

### Per-Category Breakdown

| Category | Examples | Syntax PASS | Overall PASS | FAIL-SYNTAX | FAIL-SEMANTIC | FAIL-BOTH |
|----------|----------|-------------|--------------|-------------|---------------|-----------|
| fnptr-callback | 15 | 3 | 0 | 0 | 3 | 12 |
| fnptr-cast | 7 | 0 | 0 | 7 | 0 | 0 |
| fnptr-dynamic-call | 5 | 4 | 4 | 1 | 0 | 0 |
| fnptr-global-array | 6 | 2 | 2 | 4 | 0 | 0 |
| fnptr-global-struct | 11 | 3 | 3 | 7 | 0 | 2 |
| fnptr-global-struct-array | 12 | 4 | 3 | 8 | 0 | 1 |
| fnptr-library | 20 | 20 | 17 | 0 | 3 | 0 |
| fnptr-only | 12 | 2 | 2 | 6 | 0 | 4 |
| fnptr-struct | 14 | 7 | 3 | 7 | 4 | 0 |
| fnptr-varargs | 1 | 1 | 1 | 0 | 0 | 0 |
| fnptr-virtual | 1 | 0 | 0 | 0 | 0 | 1 |

### Systemic Issues Found

1. **Wrong caller in ground truth** (fnptr-callback: all 15 examples) -- Ground truth lists `*_caller` wrapper stub functions as callers, but these wrappers have broken signatures and cannot invoke callbacks. Real callers are the functions that pass the callback as an argument.

2. **Swapped ground_truth.json files** (fnptr-global-struct-array: example_1 ↔ example_2) -- The two fixtures have each other's ground truth files.

3. **Invalid "NULL" callee** (fnptr-struct: examples 8, 10; fnptr-library: example 6) -- Ground truth lists `"callee": "NULL"` which is not a callable function; NULL is a null pointer guard.

4. **Missing edges** (fnptr-struct: examples 1, 6, 9; fnptr-only: examples 8, 9, 11; fnptr-virtual: example_1) -- Ground truth is incomplete; real indirect call sites exist in code but are not recorded.

5. **Broken fixture code** (fnptr-only: example_10) -- Comment header references "Curl_cfree" but no such variable exists in code; ground truth edge is unconnectable.

6. **Wrong ground truth content** (fnptr-global-struct: example_2) -- Ground truth lists `connUnixRead->connTLSRead` but the actual code has `Curl_pin_peer_pubkey->ossl_sha256sum`.

7. **Self-referential edge** (fnptr-global-struct: example_8) -- Ground truth lists `multissl_send_plain->multissl_send_plain`, a degenerate self-call.

8. **Tree-sitter parse errors** (72 of 104 fixtures) -- Primarily caused by: `...` ellipsis placeholders, truncated code fragments, bare preprocessor directives, C++ syntax in C files, and malformed macro usage.

---

## Category: fnptr-callback (15 examples)

### example_1
- **Syntax**: PASS
- **Comment header**: `/* fnptr: convert_to_ascii, targets: auth_digest_md5_to_ascii, auth_digest_sha256_to_ascii */`
- **Comment matches code**: YES
- **Forward check**:
  - convert_to_ascii_caller -> auth_digest_md5_to_ascii: NOT FOUND
  - convert_to_ascii_caller -> auth_digest_sha256_to_ascii: NOT FOUND
- **Backward check**: indirect call via convert_to_ascii param: MISSING from ground truth
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Ground truth lists wrong caller (wrapper stub `convert_to_ascii_caller` instead of real caller `Curl_auth_create_digest_http_message`)

### example_2
- **Syntax**: FAIL (parse errors at lines 49, 53)
- **Comment header**: `/* fnptr: fmt, targets: format_time_us, format_metric */`
- **Comment matches code**: YES
- **Forward check**: fmt_caller -> format_time_us: NOT FOUND; fmt_caller -> format_metric: NOT FOUND
- **Backward check**: indirect call via fmt: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller in ground truth; wrapper has broken signature

### example_3
- **Syntax**: FAIL (parse errors at lines 4, 5)
- **Comment header**: `/* fnptr: cbk, targets: nfs_is_shared_cb, nfs_copy_entries_cb */`
- **Comment matches code**: YES
- **Forward check**: cbk_caller -> nfs_is_shared_cb: NOT FOUND; cbk_caller -> nfs_copy_entries_cb: NOT FOUND
- **Backward check**: indirect call via cbk: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller in ground truth

### example_4
- **Syntax**: PASS
- **Comment header**: `/* fnptr: x, targets: av_codec_is_decoder, av_codec_is_encoder */`
- **Comment matches code**: YES
- **Forward check**: x_caller -> av_codec_is_decoder: NOT FOUND; x_caller -> av_codec_is_encoder: NOT FOUND
- **Backward check**: indirect call via x: MISSING
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Wrong caller (x_caller instead of avcodec_find_encoder/avcodec_find_decoder)

### example_5
- **Syntax**: FAIL (parse errors at lines 13, 23, 37)
- **Comment header**: `/* fnptr: cbk, targets: nfs_is_shared_cb, update_shared_entries_cb */`
- **Comment matches code**: YES
- **Forward check**: cbk_caller -> nfs_is_shared_cb: NOT FOUND; cbk_caller -> update_shared_entries_cb: NOT FOUND
- **Backward check**: indirect call via cbk: MISSING; nfs_is_shared_cb has no call site passing it
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller; phantom target in comment

### example_6
- **Syntax**: FAIL (parse errors at lines 4, 9, 19)
- **Comment header**: `/* fnptr: parse_response, targets: auth_parse_www_authenticate */`
- **Comment matches code**: YES
- **Forward check**: parse_response_caller -> auth_parse_www_authenticate: NOT FOUND
- **Backward check**: indirect call via parse_response: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller

### example_7
- **Syntax**: FAIL (parse error at line 7)
- **Comment header**: `/* fnptr: x, targets: av_codec_is_encoder_with_data */`
- **Comment matches code**: YES
- **Forward check**: x_caller -> av_codec_is_encoder_with_data: NOT FOUND
- **Backward check**: indirect call via x: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller

### example_8
- **Syntax**: FAIL (parse errors at lines 53, 74, 78)
- **Comment header**: `/* fnptr: fmt, targets: format_time_us, format_metric, format_time_ns */`
- **Comment matches code**: YES
- **Forward check**: fmt_caller -> all targets: NOT FOUND
- **Backward check**: indirect call via fmt: MISSING edges from print_stats_latency, print_stats_summary, print_stats_header
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller; incomplete edges

### example_9
- **Syntax**: FAIL (parse error at line 55)
- **Comment header**: `/* fnptr: cbk, targets: nfs_is_shared_cb */`
- **Comment matches code**: YES
- **Forward check**: cbk_caller -> nfs_is_shared_cb: NOT FOUND
- **Backward check**: indirect call via cbk: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller

### example_10
- **Syntax**: FAIL (parse errors at lines 11, 29, 38)
- **Comment header**: `/* fnptr: cbk, targets: nfs_is_shared_cb, nfs_copy_entries_cb */`
- **Comment matches code**: YES
- **Forward check**: cbk_caller -> all targets: NOT FOUND
- **Backward check**: indirect call via cbk: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller

### example_11
- **Syntax**: FAIL (parse errors at lines 8, 12, 35)
- **Comment header**: `/* fnptr: cbk, targets: update_shared_entries_cb, nfs_is_shared_cb */`
- **Comment matches code**: YES
- **Forward check**: cbk_caller -> all targets: NOT FOUND
- **Backward check**: indirect call via cbk: MISSING; nfs_is_shared_cb has no matching call site
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller; phantom target

### example_12
- **Syntax**: FAIL (parse errors at lines 16, 21)
- **Comment header**: `/* fnptr: fmt, targets: format_time_us, format_metric */`
- **Comment matches code**: YES
- **Forward check**: fmt_caller -> all targets: NOT FOUND
- **Backward check**: indirect call via fmt: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller

### example_13
- **Syntax**: FAIL (parse errors at lines 9, 17, 33)
- **Comment header**: `/* fnptr: cbk, targets: nfs_is_shared_cb */`
- **Comment matches code**: YES
- **Forward check**: cbk_caller -> nfs_is_shared_cb: NOT FOUND
- **Backward check**: indirect call via cbk: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller

### example_14
- **Syntax**: FAIL (parse errors at lines 5, 7, 8)
- **Comment header**: `/* fnptr: convert_to_ascii, targets: auth_digest_md5_to_ascii */`
- **Comment matches code**: YES
- **Forward check**: convert_to_ascii_caller -> auth_digest_md5_to_ascii: NOT FOUND
- **Backward check**: indirect call via convert_to_ascii: MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Wrong caller

### example_15
- **Syntax**: PASS
- **Comment header**: `/* fnptr: cbk, targets: update_shared_entries_cb */`
- **Comment matches code**: YES
- **Forward check**: cbk_caller -> update_shared_entries_cb: NOT FOUND
- **Backward check**: indirect call via cbk: MISSING
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Wrong caller (cbk_caller instead of nfs_update_shared_entries)

---

## Category: fnptr-cast (7 examples)

### example_1
- **Syntax**: FAIL (parse errors at lines 13, 14 -- JET_MUTABLE attribute)
- **Comment header**: `/* fnptr: nstime_update, targets: nstime_update_impl */`
- **Comment matches code**: YES
- **Forward check**: hpa_hooks_curtime -> nstime_update_impl: FOUND-INDIRECT
- **Backward check**: indirect call via nstime_update: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: tree-sitter cannot parse JET_MUTABLE attribute

### example_2
- **Syntax**: FAIL (parse errors at lines 112, 113, 152)
- **Comment header**: `/* fnptr: ops->fmdo_close, targets: zfs_fm_close */`
- **Comment matches code**: YES
- **Forward check**: fmd_case_close -> zfs_fm_close: FOUND-INDIRECT
- **Backward check**: indirect call via ops->fmdo_close: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code fragments

### example_3
- **Syntax**: FAIL (parse errors at lines 8, 26, 29)
- **Comment header**: `/* fnptr: *md5params->md5_init_func, targets: my_md5_init */`
- **Comment matches code**: YES
- **Forward check**: md5_init_func_caller -> my_md5_init: FOUND-INDIRECT
- **Backward check**: indirect call via *md5params->md5_init_func: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_4
- **Syntax**: FAIL (parse errors at lines 17, 24, 33)
- **Comment header**: `/* fnptr: *context->md5_hash->md5_update_func, targets: my_md5_update */`
- **Comment matches code**: YES
- **Forward check**: md5_update_func_caller -> my_md5_update: FOUND-INDIRECT
- **Backward check**: indirect call via *context->md5_hash->md5_update_func: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_5
- **Syntax**: FAIL (parse errors at lines 9, 19, 39)
- **Comment header**: `/* fnptr: funs->memory, targets: __gmp_asprintf_memory */`
- **Comment matches code**: YES
- **Forward check**: memory_caller -> __gmp_asprintf_memory: FOUND-INDIRECT
- **Backward check**: indirect call via funs->memory: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_6
- **Syntax**: FAIL (parse errors at lines 54, 59, 92)
- **Comment header**: `/* fnptr: holdfunc, targets: dsl_dataset_hold, dsl_dataset_hold_obj_string */`
- **Comment matches code**: YES
- **Forward check**: dsl_dataset_user_release_sync -> dsl_dataset_hold: FOUND-INDIRECT; -> dsl_dataset_hold_obj_string: FOUND-INDIRECT
- **Backward check**: indirect call via holdfunc: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_7
- **Syntax**: FAIL (parse errors at lines 26, 89, 95)
- **Comment header**: `/* fnptr: ops->compute_native, targets: fletcher_4_scalar_native, fletcher_4_superscalar_native, fletcher_4_superscalar4_native */`
- **Comment matches code**: YES
- **Forward check**: abd_fletcher_4_iter -> all 3 targets: FOUND-INDIRECT
- **Backward check**: indirect call via ops->compute_native: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

---

## Category: fnptr-dynamic-call (5 examples)

### example_1
- **Syntax**: PASS
- **Comment header**: `/* fnptr: onload, targets: RedisModule_OnLoad */`
- **Comment matches code**: YES
- **Forward check**: onload_caller -> RedisModule_OnLoad: FOUND-INDIRECT
- **Backward check**: indirect call via onload (dlsym-resolved): COVERED
- **Verdict**: PASS
- **Issues**: none

### example_2
- **Syntax**: PASS
- **Comment header**: `/* fnptr: ret->sk_api_version, targets: sk_api_version */`
- **Comment matches code**: YES
- **Forward check**: sk_api_version_caller -> sk_api_version: FOUND-INDIRECT
- **Backward check**: indirect call via ret->sk_api_version: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_3
- **Syntax**: FAIL (parse errors at lines 4, 11, 15)
- **Comment header**: `/* fnptr: omx_context->ptr_Init, targets: OMX_Init */`
- **Comment matches code**: YES
- **Forward check**: omx_init -> OMX_Init: FOUND-INDIRECT
- **Backward check**: indirect call via omx_context->ptr_Init: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_4
- **Syntax**: PASS
- **Comment header**: `/* fnptr: ctx->bind_engine, targets: bind_engine */`
- **Comment matches code**: YES
- **Forward check**: dynamic_load -> bind_engine: FOUND-INDIRECT
- **Backward check**: indirect call via ctx->bind_engine: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_5
- **Syntax**: PASS
- **Comment header**: `/* fnptr: skp->sk_enroll, targets: sk_enroll, ssh_sk_enroll */`
- **Comment matches code**: YES
- **Forward check**: sshsk_enroll -> sk_enroll: FOUND-INDIRECT; -> ssh_sk_enroll: FOUND-INDIRECT
- **Backward check**: indirect call via skp->sk_enroll: COVERED
- **Verdict**: PASS
- **Issues**: none

---

## Category: fnptr-global-array (6 examples)

### example_1
- **Syntax**: PASS
- **Comment header**: `/* fnptr: object_viewer[], targets: 19 dump_* functions */`
- **Comment matches code**: YES
- **Forward check**: dump_object -> all 19 dump_* functions: FOUND-INDIRECT
- **Backward check**: indirect call via object_viewer array: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_2
- **Syntax**: PASS
- **Comment header**: `/* fnptr: quantize_and_encode_band_cost_*_arr[], targets: 10 quantize_and_encode_band_cost_* functions */`
- **Comment matches code**: YES
- **Forward check**: quantize_and_encode_band -> all 10 targets: FOUND-INDIRECT
- **Backward check**: indirect call via array dispatch: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_3
- **Syntax**: FAIL (parse errors at lines 12, 28)
- **Comment header**: `/* fnptr: convert_func[type], targets: convert_str, convert_int, convert_flt, convert_color, convert_timestamp, convert_alignment */`
- **Comment matches code**: YES
- **Forward check**: ass_split_section -> all 6 convert_* functions: FOUND-INDIRECT
- **Backward check**: indirect call via convert_func array: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_4
- **Syntax**: FAIL (parse errors at lines 4, 7, 15)
- **Comment header**: `/* fnptr: trc_func, targets: 13 trc_* functions */`
- **Comment matches code**: YES
- **Forward check**: decode_init -> all 13 trc_* functions: FOUND-INDIRECT
- **Backward check**: indirect call via trc_func: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_5
- **Syntax**: FAIL (parse errors at lines 6, 8, 31)
- **Comment header**: `/* fnptr: finit[state], targets: Curl_init_CONNECT, before_perform, init_completed */`
- **Comment matches code**: YES
- **Forward check**: mstate -> all 3 targets: FOUND-INDIRECT
- **Backward check**: indirect call via finit array: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_6
- **Syntax**: FAIL (parse errors at lines 8, 28, 30)
- **Comment header**: `/* fnptr: gf_x1_mul_fns[c], targets: mul_x1_0 through mul_x1_255 */`
- **Comment matches code**: YES
- **Forward check**: gf_x1_mul_fns[c]_caller -> all 256 mul_x1_* functions: FOUND-INDIRECT
- **Backward check**: indirect call via gf_x1_mul_fns array: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code; non-standard wrapper function name

---

## Category: fnptr-global-struct (11 examples)

### example_1
- **Syntax**: FAIL (duplicate function definition at line 38/44)
- **Comment header**: `/* fnptr: vdev_indirect_ops.vdev_op_remap, targets: vdev_indirect_remap */`
- **Comment matches code**: YES
- **Forward check**: claim_segment_cb -> vdev_indirect_remap: FOUND-INDIRECT
- **Backward check**: indirect call via vdev_indirect_ops.vdev_op_remap: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Duplicate function definition

### example_2
- **Syntax**: FAIL (truncated code at lines 8, 15, 18)
- **Comment header**: `/* fnptr: Curl_ssl->sha256sum, targets: ossl_sha256sum */`
- **Comment matches code**: YES
- **Forward check**: connUnixRead -> connTLSRead: NOT FOUND -- ground truth appears to be from a different fixture
- **Backward check**: indirect call via Curl_ssl->sha256sum: MISSING (should be Curl_pin_peer_pubkey -> ossl_sha256sum)
- **Verdict**: FAIL-BOTH
- **Issues**: Ground truth completely wrong -- lists connUnixRead->connTLSRead but code has Curl_pin_peer_pubkey->ossl_sha256sum

### example_3
- **Syntax**: PASS
- **Comment header**: `/* fnptr: type.clientPubSubChannels, targets: getClientPubSubChannels */`
- **Comment matches code**: YES
- **Forward check**: pubsubSubscribeChannel -> getClientPubSubChannels: FOUND-INDIRECT
- **Backward check**: indirect call via type.clientPubSubChannels: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_4
- **Syntax**: FAIL (truncated code at lines 6, 8, 14)
- **Comment header**: `/* fnptr: vec->zvec_legacy_func, targets: 57 zfs_ioc_* functions */`
- **Comment matches code**: YES
- **Forward check**: zfsdev_ioctl_common -> all 57 targets: FOUND-INDIRECT
- **Backward check**: indirect call via vec->zvec_legacy_func: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_5
- **Syntax**: FAIL (truncated code at lines 41, 44, 58)
- **Comment header**: `/* fnptr: connectionTypeTcp()->read, targets: connTLSRead */`
- **Comment matches code**: YES
- **Forward check**: connUnixRead -> connTLSRead: FOUND-INDIRECT
- **Backward check**: indirect call via connectionTypeTcp()->read: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_6
- **Syntax**: FAIL (truncated code at lines 12, 14, 24)
- **Comment header**: `/* fnptr: r->fn->createDouble, targets: createDoubleObject */`
- **Comment matches code**: YES
- **Forward check**: processLineItem -> createDoubleObject: FOUND-INDIRECT
- **Backward check**: indirect call via r->fn->createDouble: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_7
- **Syntax**: FAIL (truncated code at lines 8, 11, 47)
- **Comment header**: `/* fnptr: aclp->z_ops->ace_size, targets: zfs_ace_v0_size, zfs_ace_fuid_size */`
- **Comment matches code**: YES
- **Forward check**: zfs_acl_chmod -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via aclp->z_ops->ace_size: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code

### example_8
- **Syntax**: FAIL (#error directive at line 46)
- **Comment header**: `/* fnptr: Curl_ssl->send_plain, targets: multissl_send_plain */`
- **Comment matches code**: YES
- **Forward check**: multissl_send_plain -> multissl_send_plain: FOUND-BUT-DIRECT (self-referential edge)
- **Backward check**: indirect call via Curl_ssl->send_plain: COVERED but degenerate
- **Verdict**: FAIL-BOTH
- **Issues**: #error directive; ground truth is a self-call (degenerate)

### example_9
- **Syntax**: FAIL (C++ `class` keyword at line 5)
- **Comment header**: `/* fnptr: stream_read_tree, targets: lto_input_tree */`
- **Comment matches code**: YES
- **Forward check**: input_ssa_names -> lto_input_tree: FOUND-INDIRECT
- **Backward check**: indirect call via stream_read_tree: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: C++ syntax in C file

### example_10
- **Syntax**: PASS
- **Comment header**: `/* fnptr: sshkey_ecdsa_funcs.equal, targets: ssh_ecdsa_equal */`
- **Comment matches code**: YES
- **Forward check**: ssh_ecdsa_sk_equal -> ssh_ecdsa_equal: FOUND-INDIRECT
- **Backward check**: indirect call via sshkey_ecdsa_funcs.equal: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_11
- **Syntax**: PASS
- **Comment header**: `/* fnptr: sshkey_ed25519_funcs.serialize_public, targets: ssh_ed25519_serialize_public */`
- **Comment matches code**: YES
- **Forward check**: ssh_ed25519_sk_serialize_public -> ssh_ed25519_serialize_public: FOUND-INDIRECT
- **Backward check**: indirect call via sshkey_ed25519_funcs.serialize_public: COVERED
- **Verdict**: PASS
- **Issues**: none

---

## Category: fnptr-global-struct-array (12 examples)

### example_1
- **Syntax**: PASS
- **Comment header**: `/* fnptr: auxFieldHandlers[j].setter, targets: auxShardIdSetter, auxHumanNodenameSetter, auxTcpPortSetter, auxTlsPortSetter */`
- **Comment matches code**: YES
- **Forward check**: curl_version_info -> https_proxy_present: NOT FOUND -- ground truth belongs to example_2
- **Backward check**: indirect call via auxFieldHandlers[j].setter: MISSING from ground truth
- **Verdict**: FAIL-BOTH
- **Issues**: ground_truth.json appears swapped with example_2

### example_2
- **Syntax**: FAIL (parse errors at lines 9, 32, 34)
- **Comment header**: `/* fnptr: p->present, targets: https_proxy_present */`
- **Comment matches code**: YES
- **Forward check**: curl_version_info -> https_proxy_present: FOUND-INDIRECT
- **Backward check**: indirect call via p->present: COVERED
- **Verdict**: FAIL-BOTH
- **Issues**: ground_truth.json appears swapped with example_1

### example_3
- **Syntax**: FAIL (parse errors at lines 34, 38, 43)
- **Comment header**: `/* fnptr: mappings[i].writefunc, targets: writeLong, writeOffset, writeString, writeTime */`
- **Comment matches code**: YES
- **Forward check**: ourWriteOutJSON -> all 4 targets: FOUND-INDIRECT
- **Backward check**: indirect call via mappings[i].writefunc: COVERED
- **Verdict**: FAIL-BOTH
- **Issues**: Parse errors from ellipsis fragments

### example_4
- **Syntax**: FAIL (parse errors at lines 13, 16)
- **Comment header**: `/* fnptr: ci_decompress, targets: lzjb_decompress, gzip_decompress, zle_decompress, lz4_decompress_zfs, zfs_zstd_decompress */`
- **Comment matches code**: YES
- **Forward check**: zstream_do_recompress -> all 5 targets: FOUND-INDIRECT
- **Backward check**: indirect call via dinfo->ci_decompress: COVERED
- **Verdict**: FAIL-BOTH
- **Issues**: Parse errors from ellipsis fragments

### example_5
- **Syntax**: FAIL (parse error at line 7 -- trailing comma in macro)
- **Comment header**: `/* fnptr: fstypes[protocol]->commit_shares, targets: nfs_commit_shares, smb_update_shares */`
- **Comment matches code**: YES
- **Forward check**: sa_commit_shares -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via fstypes[protocol]->commit_shares: COVERED
- **Verdict**: FAIL-BOTH
- **Issues**: Parse error

### example_6
- **Syntax**: FAIL (parse errors at lines 7, 64, 67)
- **Comment header**: `/* fnptr: command_table[i].func, targets: ~40 zfs_do_* functions */`
- **Comment matches code**: YES
- **Forward check**: main -> all targets: FOUND-INDIRECT
- **Backward check**: indirect call via command_table[i].func: COVERED
- **Verdict**: FAIL-BOTH
- **Issues**: Parse errors from preprocessor fragments

### example_7
- **Syntax**: FAIL (parse errors at lines 43, 117, 118)
- **Comment header**: `/* fnptr: ops->transform, targets: sha256_generic, sha512_generic, tf_sha512_transform_x64, tf_sha256_transform_x64 */`
- **Comment matches code**: YES
- **Forward check**: sha256_update -> all 4 targets: FOUND-INDIRECT
- **Backward check**: indirect call via ops->transform: COVERED
- **Verdict**: FAIL-BOTH
- **Issues**: Parse errors from ASSERT macros

### example_8
- **Syntax**: PASS
- **Comment header**: `/* fnptr: ddt_ops[type]->ddt_op_lookup, targets: ddt_zap_lookup */`
- **Comment matches code**: YES
- **Forward check**: ddt_object_lookup -> ddt_zap_lookup: FOUND-INDIRECT
- **Backward check**: indirect call via ddt_ops[type]->ddt_op_lookup: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_9
- **Syntax**: PASS
- **Comment header**: `/* fnptr: speex_modes[s->mode].decode, targets: nb_decode, sb_decode */`
- **Comment matches code**: YES
- **Forward check**: speex_decode_frame -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via speex_modes[s->mode].decode: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_10
- **Syntax**: FAIL (parse errors at lines 40, 43, 50)
- **Comment header**: `/* fnptr: impl->funcs->size, targets: ssh_rsa_size */`
- **Comment matches code**: YES
- **Forward check**: sshkey_size -> ssh_rsa_size: FOUND-INDIRECT
- **Backward check**: indirect call via impl->funcs->size: COVERED
- **Verdict**: FAIL-BOTH
- **Issues**: Parse errors from preprocessor conditionals

### example_11
- **Syntax**: FAIL (parse errors at lines 50, 53, 60)
- **Comment header**: `/* fnptr: impl->funcs->alloc, targets: ssh_rsa_alloc */`
- **Comment matches code**: YES
- **Forward check**: sshkey_new -> ssh_rsa_alloc: FOUND-INDIRECT
- **Backward check**: indirect call via impl->funcs->alloc: COVERED
- **Verdict**: FAIL-BOTH
- **Issues**: Parse errors from preprocessor conditionals

### example_12
- **Syntax**: PASS
- **Comment header**: `/* fnptr: mux_master_handlers[i].handler, targets: 9 mux_master_process_* functions */`
- **Comment matches code**: YES
- **Forward check**: mux_master_read_cb -> all 9 targets: FOUND-INDIRECT
- **Backward check**: indirect call via mux_master_handlers[i].handler: COVERED
- **Verdict**: PASS
- **Issues**: none

---

## Category: fnptr-library (20 examples)

### example_1
- **Syntax**: PASS
- **Comment header**: `/* fnptr: c->funcs->read, targets: redisNetRead */`
- **Comment matches code**: YES
- **Forward check**: readConn -> redisNetRead: FOUND-INDIRECT
- **Backward check**: indirect call via c->funcs->read: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_2
- **Syntax**: PASS
- **Comment header**: `/* fnptr: g->allocf, targets: l_alloc, lj_alloc_f */`
- **Comment matches code**: YES
- **Forward check**: lj_mem_free -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via g->allocf: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_3
- **Syntax**: PASS
- **Comment header**: `/* fnptr: sock.read, targets: ssl_read, sock_read */`
- **Comment matches code**: YES
- **Forward check**: socket_readable -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via sock.read: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_4
- **Syntax**: PASS
- **Comment header**: `/* fnptr: c->input_filter, targets: client_simple_escape_filter, sys_tun_infilter */`
- **Comment matches code**: YES
- **Forward check**: channel_handle_rfd -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via c->input_filter: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_5
- **Syntax**: PASS
- **Comment header**: `/* fnptr: s->decode_mb, targets: ff_h263_decode_mb */`
- **Comment matches code**: YES
- **Forward check**: decode_slice -> ff_h263_decode_mb: FOUND-INDIRECT
- **Backward check**: indirect call via s->decode_mb: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_6
- **Syntax**: PASS
- **Comment header**: `/* fnptr: mt->mem_usage2, targets: NULL */`
- **Comment matches code**: NO -- NULL is a null pointer guard, not a function target
- **Forward check**: moduleGetMemUsage -> NULL: FOUND-BUT-DIRECT (NULL is not a real function)
- **Backward check**: indirect call via mt->mem_usage2: MISSING -- no real function assigned
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Ground truth incorrectly lists "NULL" as callee. No actual function target assigned.

### example_7
- **Syntax**: PASS
- **Comment header**: `/* fnptr: conn->type->set_write_handler, targets: connSocketSetWriteHandler, connTLSSetWriteHandler, connUnixSetWriteHandler */`
- **Comment matches code**: YES
- **Forward check**: connSetWriteHandlerWithBarrier -> all 3 targets: FOUND-INDIRECT
- **Backward check**: indirect call via conn->type->set_write_handler: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_8
- **Syntax**: PASS
- **Comment header**: `/* fnptr: cf->cft->get_host, targets: cf_socket_get_host */`
- **Comment matches code**: YES
- **Forward check**: Curl_conn_get_host -> cf_socket_get_host: FOUND-INDIRECT
- **Backward check**: indirect call via cf->cft->get_host: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_9
- **Syntax**: PASS
- **Comment header**: `/* fnptr: list->dtor, targets: 8 destructor functions including curl_free, gsasl_free */`
- **Comment matches code**: PARTIAL -- curl_free and gsasl_free are assigned to br->dtor (bufref struct), not list->dtor (llist struct)
- **Forward check**: Curl_llist_remove -> 6 of 8 targets: FOUND-INDIRECT; -> curl_free: FOUND-BUT-DIRECT (wrong struct); -> gsasl_free: FOUND-BUT-DIRECT (wrong struct)
- **Backward check**: indirect call via list->dtor: 6/8 correct; 2 targets on different struct
- **Verdict**: FAIL-SEMANTIC
- **Issues**: curl_free and gsasl_free assigned to br->dtor (bufref), not list->dtor (llist) -- different data structures conflated

### example_10
- **Syntax**: PASS
- **Comment header**: `/* fnptr: ctx->lookup_crls, targets: crls_http_cb, X509_STORE_CTX_get1_crls */`
- **Comment matches code**: YES
- **Forward check**: get_crl_delta -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via ctx->lookup_crls: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_11
- **Syntax**: PASS
- **Comment header**: `/* fnptr: synth->synth_filter_float, targets: synth_filter_sse2, synth_filter_avx, synth_filter_fma3 */`
- **Comment matches code**: YES
- **Forward check**: sub_qmf32_float_c -> all 3 targets: FOUND-INDIRECT
- **Backward check**: indirect call via synth->synth_filter_float: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_12
- **Syntax**: PASS
- **Comment header**: `/* fnptr: s->vectorscope, targets: vectorscope8, vectorscope16 */`
- **Comment matches code**: YES
- **Forward check**: filter_frame -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via s->vectorscope: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_13
- **Syntax**: PASS
- **Comment header**: `/* fnptr: s->out_transform, targets: 25 *_to_xyz functions */`
- **Comment matches code**: YES
- **Forward check**: v360_slice -> all 25 targets: FOUND-INDIRECT
- **Backward check**: indirect call via s->out_transform: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_14
- **Syntax**: PASS
- **Comment header**: `/* fnptr: ctx->dsp.upsample_plane, targets: upsample_plane_c */`
- **Comment matches code**: YES
- **Forward check**: decode_wmv9 -> upsample_plane_c: FOUND-INDIRECT
- **Backward check**: indirect call via ctx->dsp.upsample_plane: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_15
- **Syntax**: PASS
- **Comment header**: `/* fnptr: s->fdsp->vector_fmul, targets: vector_fmul_c, ff_vector_fmul_neon, ff_vector_fmul_vfp */`
- **Comment matches code**: YES
- **Forward check**: apply_mdct -> all 3 targets: FOUND-INDIRECT
- **Backward check**: indirect call via s->fdsp->vector_fmul: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_16
- **Syntax**: PASS
- **Comment header**: `/* fnptr: context->bbdsp.bswap16_buf, targets: bswap16_buf, ff_bswap16_buf_rvv */`
- **Comment matches code**: YES
- **Forward check**: raw_decode -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via context->bbdsp.bswap16_buf: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_17
- **Syntax**: PASS
- **Comment header**: `/* fnptr: ctx->celpf_ctx.celp_lp_synthesis_filterf, targets: ff_celp_lp_synthesis_filterf, ff_celp_lp_synthesis_filterf_mips */`
- **Comment matches code**: YES
- **Forward check**: synthesis -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via ctx->celpf_ctx.celp_lp_synthesis_filterf: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_18
- **Syntax**: PASS
- **Comment header**: `/* fnptr: kex->verify_host_key, targets: key_print_wrapper, _ssh_verify_host_key */`
- **Comment matches code**: YES
- **Forward check**: kex_verify_host_key -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via kex->verify_host_key: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_19
- **Syntax**: PASS
- **Comment header**: `/* fnptr: c->output_filter, targets: sys_tun_outfilter */`
- **Comment matches code**: YES (partial)
- **Forward check**: channel_handle_wfd -> sys_tun_outfilter: FOUND-INDIRECT
- **Backward check**: indirect call via c->output_filter: MISSING -- NULL also assigned via client_loop
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Missing NULL target assigned via client_loop

### example_20
- **Syntax**: PASS
- **Comment header**: `/* fnptr: c->open_confirm, targets: mux_session_confirm, mux_stdio_confirm, ssh_stdio_confirm, ssh_session2_setup, ssh_tun_confirm */`
- **Comment matches code**: YES
- **Forward check**: channel_input_open_failure -> all 5 targets: FOUND-INDIRECT
- **Backward check**: indirect call via c->open_confirm: COVERED
- **Verdict**: PASS
- **Issues**: none

---

## Category: fnptr-only (12 examples)

### example_1
- **Syntax**: PASS
- **Comment header**: `/* fnptr: zmalloc_oom_handler, targets: zmalloc_default_oom */`
- **Comment matches code**: YES
- **Forward check**: zmalloc -> zmalloc_default_oom: FOUND-INDIRECT
- **Backward check**: indirect call via zmalloc_oom_handler: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_2
- **Syntax**: FAIL (truncated function signature at lines 5-7)
- **Comment header**: `/* fnptr: Curl_ccalloc, targets: calloc */`
- **Comment matches code**: YES
- **Forward check**: Curl_ccalloc_caller -> calloc: FOUND-INDIRECT
- **Backward check**: indirect call via Curl_ccalloc: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Circular macro `#define calloc Curl_ccalloc` corrupts line 19

### example_3
- **Syntax**: FAIL (truncated code at lines 4-6)
- **Comment header**: `/* fnptr: md_final_raw, targets: tls1_md5_final_raw, tls1_sha1_final_raw, tls1_sha256_final_raw, tls1_sha512_final_raw */`
- **Comment matches code**: YES
- **Forward check**: md_final_raw_caller -> all 4 targets: FOUND-INDIRECT
- **Backward check**: indirect call via md_final_raw: COVERED; md_transform fnptr also has indirect call site but not tracked
- **Verdict**: FAIL-SYNTAX
- **Issues**: Truncated code; md_transform fnptr not represented in ground truth

### example_4
- **Syntax**: PASS
- **Comment header**: `/* fnptr: conv, targets: gray8aToPacked32, gray8aToPacked32_1, gray8aToPacked24, sws_convertPalette8ToPacked32, sws_convertPalette8ToPacked24 */`
- **Comment matches code**: YES
- **Forward check**: palToRgbWrapper -> all 5 targets: FOUND-INDIRECT
- **Backward check**: indirect call via conv: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_5
- **Syntax**: FAIL (ellipsis at lines 8, 15, 29)
- **Comment header**: `/* fnptr: tmp_handler, targets: mm_log_handler */`
- **Comment matches code**: YES
- **Forward check**: do_log -> mm_log_handler: FOUND-INDIRECT
- **Backward check**: indirect call via tmp_handler: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Ellipsis and truncated code

### example_6
- **Syntax**: FAIL (ellipsis at line 6)
- **Comment header**: `/* fnptr: junk_alloc_callback, targets: default_junk_alloc */`
- **Comment matches code**: YES
- **Forward check**: do_rallocx -> default_junk_alloc: FOUND-INDIRECT
- **Backward check**: indirect call via junk_alloc_callback: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Ellipsis

### example_7
- **Syntax**: FAIL (ellipsis at lines 7, 22, 24)
- **Comment header**: `/* fnptr: xfunc, targets: lzjb_decompress, gzip_decompress, zle_decompress, lz4_decompress_zfs, zfs_zstd_decompress */`
- **Comment matches code**: YES
- **Forward check**: zstream_do_decompress -> all 5 targets: FOUND-INDIRECT
- **Backward check**: indirect call via xfunc: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Ellipsis

### example_8
- **Syntax**: FAIL (ellipsis at lines 16, 22)
- **Comment header**: `/* fnptr: Curl_cfree, targets: free */`
- **Comment matches code**: PARTIAL -- self-referential macro
- **Forward check**: Curl_cfree_caller -> free: FOUND-INDIRECT
- **Backward check**: indirect call via Curl_cfree: COVERED; Curl_cookie_add also calls through fnptr but edge MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Missing edge: Curl_cookie_add -> free; self-referential macro on line 31

### example_9
- **Syntax**: FAIL (ellipsis at lines 10, 21)
- **Comment header**: `/* fnptr: Curl_cfree, targets: free */`
- **Comment matches code**: YES
- **Forward check**: Curl_cfree_caller -> free: FOUND-INDIRECT
- **Backward check**: indirect call via Curl_cfree: COVERED; Curl_output_digest also calls through fnptr but edge MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Missing edge: Curl_output_digest -> free

### example_10
- **Syntax**: FAIL (ellipsis at lines 8, 22)
- **Comment header**: `/* fnptr: Curl_cfree, targets: malloc */`
- **Comment matches code**: NO -- "Curl_cfree" does not exist in this file; macro maps malloc->Curl_cmalloc
- **Forward check**: Curl_cfree_caller -> malloc: NOT FOUND -- Curl_cfree is never defined
- **Backward check**: No valid indirect call chain exists
- **Verdict**: FAIL-BOTH
- **Issues**: Comment variable name doesn't match code; ground truth edge is unconnectable

### example_11
- **Syntax**: FAIL (ellipsis at lines 16, 20, 23)
- **Comment header**: `/* fnptr: strdup, targets: Curl_strdup, strdup */`
- **Comment matches code**: YES
- **Forward check**: strdup_caller -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via Curl_cstrdup: COVERED; Curl_cookie_add also calls through fnptr but edges MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Missing edges: Curl_cookie_add -> Curl_strdup and Curl_cookie_add -> strdup

### example_12
- **Syntax**: FAIL (preprocessor conditional at line 29)
- **Comment header**: `/* fnptr: deinterleaveBytes, targets: deinterleaveBytes_c */`
- **Comment matches code**: YES
- **Forward check**: nv12ToPlanarWrapper -> deinterleaveBytes_c: FOUND-INDIRECT
- **Backward check**: indirect call via deinterleaveBytes: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Preprocessor conditional

---

## Category: fnptr-struct (14 examples)

### example_1
- **Syntax**: PASS
- **Comment header**: `/* fnptr: handler->finalizeResultEmission, targets: zrangeResultFinalizeClient, zrangeResultFinalizeStore */`
- **Comment matches code**: YES
- **Forward check**: genericZrangebyrankCommand -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via handler->finalizeResultEmission: COVERED; handler->beginResultEmission also called but edges MISSING
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Missing beginResultEmission edges

### example_2
- **Syntax**: FAIL (parse errors at lines 13, 26)
- **Comment header**: `/* fnptr: pfile->cb.before_define, targets: dump_queued_macros */`
- **Comment matches code**: YES
- **Forward check**: cpp_pop_definition -> dump_queued_macros: FOUND-INDIRECT
- **Backward check**: indirect call via pfile->cb.before_define: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Standalone if blocks not enclosed in function definition

### example_3
- **Syntax**: FAIL (parse errors at lines 4, 15, 18)
- **Comment header**: `/* fnptr: s->abs_pow34, targets: ff_abs_pow34_sse, abs_pow34_v */`
- **Comment matches code**: YES
- **Forward check**: search_for_quantizers_fast -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via s->abs_pow34: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Ellipsis and undeclared variables

### example_4
- **Syntax**: FAIL (parse errors at lines 7, 9, 14)
- **Comment header**: `/* fnptr: te->timeProc, targets: record_rate */`
- **Comment matches code**: YES
- **Forward check**: processTimeEvents -> record_rate: FOUND-INDIRECT
- **Backward check**: indirect call via te->timeProc: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Ellipsis and undeclared identifiers

### example_5
- **Syntax**: FAIL (parse errors at lines 18, 37, 40)
- **Comment header**: `/* fnptr: uic->uic_cb, targets: 5 callback functions */`
- **Comment matches code**: YES
- **Forward check**: iterate_through_spacemap_logs_cb -> all 5 targets: FOUND-INDIRECT
- **Backward check**: indirect call via uic->uic_cb: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Ellipsis and undeclared types

### example_6
- **Syntax**: FAIL (parse errors at lines 31, 33, 39)
- **Comment header**: `/* fnptr: defragalloc, targets: activeDefragAlloc */`
- **Comment matches code**: YES
- **Forward check**: dictDefragBucket -> activeDefragAlloc: FOUND-INDIRECT
- **Backward check**: indirect call via defragalloc: COVERED; defragkey and defragval also used with activeDefragSds but edges MISSING
- **Verdict**: FAIL-BOTH
- **Issues**: Missing defragkey/defragval edges

### example_7
- **Syntax**: FAIL (parse error at line 30)
- **Comment header**: `/* fnptr: engine->get_engine_memory_overhead, targets: luaEngineMemoryOverhead */`
- **Comment matches code**: YES
- **Forward check**: functionsRegisterEngine -> luaEngineMemoryOverhead: FOUND-INDIRECT
- **Backward check**: indirect call via engine->get_engine_memory_overhead: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Ellipsis and incomplete code

### example_8
- **Syntax**: PASS
- **Comment header**: `/* fnptr: pkey->ameth->pkey_security_bits, targets: NULL */`
- **Comment matches code**: PARTIAL -- no concrete function target assigned
- **Forward check**: EVP_PKEY_security_bits -> NULL: NOT FOUND -- NULL is not a real function
- **Backward check**: indirect call via pkey->ameth->pkey_security_bits: no concrete target
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Invalid callee "NULL"

### example_9
- **Syntax**: PASS
- **Comment header**: `/* fnptr: sdb->old_cb, targets: ssl_security_default_callback */`
- **Comment matches code**: YES
- **Forward check**: security_callback_debug -> ssl_security_default_callback: FOUND-INDIRECT
- **Backward check**: indirect call via sdb->old_cb: COVERED; ssl_security and ssl_ctx_security also call through sec_cb but edges MISSING
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Missing ssl_security/ssl_ctx_security edges

### example_10
- **Syntax**: PASS
- **Comment header**: `/* fnptr: dsa->meth->bn_mod_exp, targets: NULL */`
- **Comment matches code**: PARTIAL -- no concrete function target assigned
- **Forward check**: dsa_sign_setup -> NULL: NOT FOUND
- **Backward check**: indirect call via dsa->meth->bn_mod_exp: no concrete target
- **Verdict**: FAIL-SEMANTIC
- **Issues**: Invalid callee "NULL"

### example_11
- **Syntax**: PASS
- **Comment header**: `/* fnptr: s->method->put_cipher_by_char, targets: ssl3_put_cipher_by_char */`
- **Comment matches code**: YES
- **Forward check**: put_cipher_by_char_caller -> ssl3_put_cipher_by_char: FOUND-INDIRECT
- **Backward check**: indirect call via s->method->put_cipher_by_char: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_12
- **Syntax**: PASS
- **Comment header**: `/* fnptr: s->ctx->ext.alpn_select_cb, targets: alpn_cb */`
- **Comment matches code**: YES
- **Forward check**: tls_handle_alpn -> alpn_cb: FOUND-INDIRECT
- **Backward check**: indirect call via s->ctx->ext.alpn_select_cb: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_13
- **Syntax**: PASS
- **Comment header**: `/* fnptr: block, targets: aesni_encrypt */`
- **Comment matches code**: YES
- **Forward check**: CRYPTO_gcm128_encrypt -> aesni_encrypt: FOUND-INDIRECT
- **Backward check**: indirect call via block: COVERED
- **Verdict**: PASS
- **Issues**: none

### example_14
- **Syntax**: FAIL (parse errors at lines 26, 34)
- **Comment header**: `/* fnptr: s->handshake_func, targets: ossl_statem_accept, ossl_statem_connect */`
- **Comment matches code**: YES
- **Forward check**: ssl3_write_bytes -> both targets: FOUND-INDIRECT
- **Backward check**: indirect call via s->handshake_func: COVERED
- **Verdict**: FAIL-SYNTAX
- **Issues**: Preprocessor directives inside expressions

---

## Category: fnptr-varargs (1 example)

### example_1
- **Syntax**: PASS
- **Comment header**: `/* fnptr: share->lockfunc, targets: my_lock */`
- **Comment matches code**: YES
- **Forward check**: curl_share_cleanup -> my_lock: FOUND-INDIRECT
- **Backward check**: indirect call via share->lockfunc: COVERED
- **Verdict**: PASS
- **Issues**: none

---

## Category: fnptr-virtual (1 example)

### example_1
- **Syntax**: FAIL (parse errors at lines 9, 13 -- C++ syntax)
- **Comment header**: `/* fnptr: get_state_map_by_name, targets: noop_region_model_context::get_state_map_by_name, region_model_context_decorator::get_state_map_by_name */`
- **Comment matches code**: YES
- **Forward check**: get_fd_map -> both targets: FOUND-INDIRECT (virtual dispatch)
- **Backward check**: indirect call via get_state_map_by_name: COVERED for get_fd_map; get_malloc_map and get_taint_map also dispatch to both targets but edges MISSING (4 missing edges total)
- **Verdict**: FAIL-BOTH
- **Issues**: C++ syntax causes tree-sitter C parser errors; ground truth incomplete -- missing get_malloc_map and get_taint_map edges

---

*Report generated: 2026-05-09*
*Analysis method: LLM manual code reading + tree-sitter automated parsing*
*Total fixtures analyzed: 104 across 11 categories*
