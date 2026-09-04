# gcnport

`gcnport` is the shared GameCube runtime boundary for native/dynamic-recompiler hybrid ports. It is
designed to use Dolphin's PowerPC JIT and device model at runtime; it does not translate a title into
generated C or C++.

The repository contains the verified framework-side execution policy, fallback accounting,
image/module-scoped native hook registry, and one-shot original-call ticketing. Its pinned Dolphin
fork also contains a real title-neutral runtime session and a synthetic shipping-JIT discriminator,
but it does **not** yet expose the complete embedding API needed to boot or execute a game through
this library. There is therefore no runnable GameCube product or gameplay compatibility claim. See
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
the policy diagnostics against positive and negative fixtures, validates the hosted-CI contract, and
reports the Dolphin contract gap with a denominator. To build Dolphin and execute the real synthetic
JIT discriminator on the current native host:

```text
uv run --frozen python tools/verify.py --runtime
```

To make the missing complete adapter a failing gate explicitly:

```text
uv run --frozen python tools/check_dolphin_contract.py --require
```

The Dolphin dependency is the maintained `SomeoneIsWorking/dolphin` fork at
`9dfd5ac1f4c0c2d9da7661e1895a39b293286521`. The exact commit is published on
the fork's `main` branch and is reproducible by the pinned submodule.

Hosted verification uses full-history recursive checkout and immutable action revisions. It runs the
same Python verifier plus the synthetic Dolphin JIT test natively on Linux x64/arm64, Windows x64,
and macOS x64/arm64. Android is deliberately absent: no real NDK/APK/device runtime boundary exists
yet, so an Android job would be a placeholder rather than execution evidence.
