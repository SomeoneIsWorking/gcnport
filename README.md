# gcnport

`gcnport` is the shared GameCube runtime boundary for native/dynamic-recompiler hybrid ports. It is
designed to use Dolphin's PowerPC JIT and device model at runtime; it does not translate a title into
generated C or C++.

The repository currently contains the verified framework-side execution policy, fallback accounting,
image/module-scoped native hook registry, and one-shot original-call ticketing. The pinned Dolphin
fork does **not** yet expose the title-neutral embedding API needed to boot or execute a game through
this library. There is therefore no runnable GameCube product and no x86_64, macOS AArch64, or
Android arm64-v8a compatibility claim yet. See
[`docs/dolphin-embedding-contract.md`](docs/dolphin-embedding-contract.md) for the exact fork work.

## Execution rules

- JIT is the default and every ordinary cold block is offered to it before execution.
- Fallback is disabled until all three limits are explicitly configured. A bounded interpreter
  fallback is legal only after an explicit typed JIT refusal. Its guest PC,
  block count, instruction count, reason, and JIT denominators are recorded before returning to JIT.
- Interpreter-only execution is an explicit diagnostic mode and cannot establish gameplay or
  performance compatibility.
- A missing or failed host JIT is a hard backend fault. It is never represented as a fallback reason.
- Native hooks are keyed by authenticated image hash, module generation, and guest address.
- An original call uses a single-use ticket and must execute from an unpublished, unlinked JIT block;
  global hook suppression is forbidden.

## Local verification

The project has no player launcher until the Dolphin adapter exists. Its local gate is:

```text
uv run --frozen python tools/verify.py
```

The verifier uses Clang and Ninja, runs focused C++ tests, checks formatting and `clang-tidy`, tests
the policy diagnostics against positive and negative fixtures, and reports the Dolphin contract gap
with a denominator. To make the missing adapter a failing gate explicitly:

```text
uv run --frozen python tools/check_dolphin_contract.py --require
```

The Dolphin dependency is the maintained `SomeoneIsWorking/dolphin` fork at
`7fd812471e8f2030ccde7081b7c329aa252d5360`. The exact commit is published on
the fork's `sunbright` branch and is reproducible by the pinned submodule.
