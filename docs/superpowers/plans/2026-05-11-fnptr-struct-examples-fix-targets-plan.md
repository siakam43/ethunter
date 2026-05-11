# fnptr-struct Example Target Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add concrete function pointer targets and binding to example_8 and example_10 so the static analyzer can resolve callees.

**Architecture:** Each fixture gets a target function and a binding function that assigns the target to the struct field, following the pattern established by working examples (example_1, example_2). Then `ground_truth.json` is updated to list the new caller → callee pair.

**Tech Stack:** C (tree-sitter parsed), pytest, JSON

---

### Task 1: Fix example_8 — Add RSA target and binding

**Files:**
- Modify: `tests/benchmark/et_bench/fnptr-struct/example_8/fixture.c`
- Modify: `tests/benchmark/et_bench/fnptr-struct/example_8/ground_truth.json`

**Context:** The current fixture has `EVP_PKEY_security_bits` calling through `pkey->ameth->pkey_security_bits(pkey)`, and a setter `EVP_PKEY_asn1_set_security_bits(ameth, callback)`, but no concrete target function is defined and no binding code calls the setter.

- [ ] **Step 1: Add target function and binding to fixture.c**

Append the following code at the end of `tests/benchmark/et_bench/fnptr-struct/example_8/fixture.c` (after line 35):

```c
/* Target: RSA-specific security bits implementation */
static int rsa_pkey_security_bits(const EVP_PKEY *pk)
{
    (void)pk;
    return 2048; /* RSA default key size */
}

/* Binding: wire RSA method to the ASN1 vtable */
void bind_asn1_methods(EVP_PKEY_ASN1_METHOD *ameth)
{
    EVP_PKEY_asn1_set_security_bits(ameth, rsa_pkey_security_bits);
}
```

- [ ] **Step 2: Update ground_truth.json**

Replace the entire contents of `tests/benchmark/et_bench/fnptr-struct/example_8/ground_truth.json` with:

```json
{
  "examples": [
    {
      "caller": "EVP_PKEY_security_bits",
      "callee": "rsa_pkey_security_bits"
    }
  ]
}
```

- [ ] **Step 3: Run the et_bench test for example_8**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -k "fnptr-struct and example_8" -v`

Expected: The test for example_8 should pass, showing that the analyzer now resolves `rsa_pkey_security_bits` as the callee of `EVP_PKEY_security_bits`.

- [ ] **Step 4: Commit**

```bash
git add tests/benchmark/et_bench/fnptr-struct/example_8/fixture.c tests/benchmark/et_bench/fnptr-struct/example_8/ground_truth.json
git commit -m "fix: add rsa_pkey_security_bits target and binding to fnptr-struct example_8"
```

---

### Task 2: Fix example_10 — Wire BN_mod_exp_mont as target

**Files:**
- Modify: `tests/benchmark/et_bench/fnptr-struct/example_10/fixture.c`
- Modify: `tests/benchmark/et_bench/fnptr-struct/example_10/ground_truth.json`

**Context:** The current fixture has `dsa_sign_setup` calling through `dsa->meth->bn_mod_exp(...)`. The function `BN_mod_exp_mont` exists at line 63-68 as a static function. There is a `DSA_meth_set_bn_mod_exp` setter at line 93-99. But no code ever calls this setter with `BN_mod_exp_mont` as an argument, so the analyzer cannot find a target.

Note: `BN_mod_exp_mont` is declared `static` at line 63. To make it usable as a binding target, we don't need to change its visibility — the `bind_dsa_methods` function is in the same translation unit, so it can reference `BN_mod_exp_mont` directly.

- [ ] **Step 1: Add binding function to fixture.c**

Append the following code at the end of `tests/benchmark/et_bench/fnptr-struct/example_10/fixture.c` (after line 214):

```c
/* Binding: wire BN_mod_exp_mont as the bn_mod_exp target */
void bind_dsa_methods(DSA_METHOD *dsam)
{
    DSA_meth_set_bn_mod_exp(dsam, BN_mod_exp_mont);
}
```

The `dsa_sign_setup` caller at line 80-82 calls `dsa->meth->bn_mod_exp(...)`. The `bind_dsa_methods` function sets `dsam->bn_mod_exp = BN_mod_exp_mont` through the existing `DSA_meth_set_bn_mod_exp` setter. The analyzer will trace this assignment and resolve the target.

- [ ] **Step 2: Update ground_truth.json**

Replace the entire contents of `tests/benchmark/et_bench/fnptr-struct/example_10/ground_truth.json` with:

```json
{
  "examples": [
    {
      "caller": "dsa_sign_setup",
      "callee": "BN_mod_exp_mont"
    }
  ]
}
```

- [ ] **Step 3: Run the et_bench test for example_10**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -k "fnptr-struct and example_10" -v`

Expected: The test for example_10 should pass, showing that the analyzer now resolves `BN_mod_exp_mont` as the callee of `dsa_sign_setup`.

- [ ] **Step 4: Commit**

```bash
git add tests/benchmark/et_bench/fnptr-struct/example_10/fixture.c tests/benchmark/et_bench/fnptr-struct/example_10/ground_truth.json
git commit -m "fix: wire BN_mod_exp_mont target in fnptr-struct example_10"
```

---

### Task 3: Full test suite verification

**Files:** None (test run only)

- [ ] **Step 1: Run all et_bench tests**

Run: `.venv/bin/python -m pytest tests/test_et_bench.py -v`

Expected: All tests pass, including example_8 and example_10.

- [ ] **Step 2: Run full test suite**

Run: `.venv/bin/python -m pytest tests/ -q`

Expected: All tests pass. No regressions introduced.

- [ ] **Step 3: Final commit if needed**

No code changes here — just verify.
