# Spec: Fix fnptr-struct example_8 and example_10 Missing Targets

## Problem

Both `et_bench/fnptr-struct/example_8` and `example_10` have function pointer calls through struct fields, but no concrete target function is ever bound to the pointer. The static analyzer can't resolve the callee, so `ground_truth.json` lists `"callee": "NULL"`.

## Changes

### example_8: EVP_PKEY ASN1 security bits dispatch

**fixture.c additions:**
1. Add a concrete target function `rsa_pkey_security_bits(const EVP_PKEY *pk)` — a realistic RSA-specific implementation of the `pkey_security_bits` callback
2. Add a binding function `bind_asn1_methods(EVP_PKEY_ASN1_METHOD *ameth)` that calls `EVP_PKEY_asn1_set_security_bits(ameth, rsa_pkey_security_bits)` — this is the explicit binding step that wires the target

**ground_truth.json update:**
- Change `"callee": "NULL"` to `"callee": "rsa_pkey_security_bits"`

### example_10: DSA bn_mod_exp through DSA_METHOD vtable

**fixture.c additions:**
1. The fallback function `BN_mod_exp_mont` already exists — it will serve as the target
2. Add a binding function `bind_dsa_methods(DSA_METHOD *dsam)` that calls `DSA_meth_set_bn_mod_exp(dsam, BN_mod_exp_mont)` — this wires `BN_mod_exp_mont` as the `bn_mod_exp` field target through the existing setter

**ground_truth.json update:**
- Change `"callee": "NULL"` to `"callee": "BN_mod_exp_mont"`

## Rationale

Both fixes follow the established pattern from working examples (example_1, example_2):
1. Define a concrete target function with a realistic signature
2. Provide an explicit binding step that assigns the function pointer to the struct field
3. Update `ground_truth.json` to list the caller → callee pair

The binding functions mirror real-world OpenSSL patterns where algorithm-specific implementations are registered via setter functions at initialization time.
