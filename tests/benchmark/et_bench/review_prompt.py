REVIEW_PROMPT = """\
You are reviewing a C code benchmark to verify that the claimed indirect call relationships are correct.

## Context
This is part of ET-Bench, a benchmark suite for function pointer call graph analysis.
Each example has a fixture.c file and a ground_truth.json listing caller->callee pairs that SHOULD be detected as indirect calls (via function pointer indirection).

## Task
Review the fixture.c code and verify each claim in ground_truth.json.

For EACH claim (caller -> callee), check:
1. **Caller exists**: Does the caller function exist in fixture.c?
2. **Callee exists**: Does the callee function exist in fixture.c?
3. **Indirect call**: Is there a call from caller to callee that happens through a function pointer indirection (not a direct `callee()` call)?
4. **Evidence**: Quote the specific line(s) in fixture.c that show the indirect call mechanism.

## Indirect Call Patterns to Recognize
- Function pointer assignment + call through pointer: `fp = callee; fp();`
- Callback parameter: caller passes callee as a function argument
- Function pointer array/index: `array[i]()` where array contains callee
- Struct vtable dispatch: `obj->vtable->method()`
- Function pointer return: `get_fp()()` returns callee
- Callback registration: `register_callback(callee)` then invoked later
- Union containing function pointer, typedef'd function pointer, lazy init, macro-based, dlsym, etc.

## IMPORTANT: A direct call like `callee(args)` is NOT an indirect call.
Only function-pointer-based indirection counts.

## Input
```c
{fixture_code}
```

```json
{ground_truth_json}
```

## Output Format
Return a JSON object with this exact structure:

```json
{
  "example": "{category}/{name}",
  "verdict": "PASS" | "FAIL",
  "claims": [
    {
      "caller": "...",
      "callee": "...",
      "caller_exists": true/false,
      "callee_exists": true/false,
      "is_indirect_call": true/false,
      "evidence": "quote the relevant code lines",
      "notes": "any additional observations"
    }
  ],
  "summary": "brief overall assessment"
}
```

If ANY claim fails (caller/callee missing, or call is direct rather than indirect), set top-level "verdict" to "FAIL". Otherwise "PASS".
"""
