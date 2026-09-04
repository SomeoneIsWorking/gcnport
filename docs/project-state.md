# Project state

## Comparison baseline

The baseline is upstream Dolphin as a standalone full emulator plus Sunbright's former title-local,
offline-generated execution path. `gcnport` is intended to expose Dolphin's runtime executor to
native/dynarec ports without retaining the offline product.

## Current focus

S003 remains the current focus: add the one-block executor, bounded typed fallback, and synchronous
original-call continuation to the now-pinned native-hook/JIT-observation slice.

## Capability inventory

| ID | Capability / observable outcome | State | Evidence or exact gap | Goals |
| --- | --- | --- | --- | --- |
| S001 | JIT-default execution policy and bounded typed fallback accounting | verified | focused execution-session tests | G001 |
| S002 | Authenticated image/module-scoped hook registry and one-shot original tickets | verified | focused hook/original tests | G002 |
| S003 | Maintained Dolphin fork implements the embeddable gcnport runtime contract | partial | pinned fork implements the instance session, hook guards, invalidation, and block/hook counters; 6/11 contract requirements remain missing | G001, G002, G003 |
| S004 | Real x86_64 Dolphin JIT blocks execute through gcnport with counters | partial | synthetic shipping-JIT test proves cold/cache-hit/hook/original-tail/invalidation execution; public one-block adapter, instruction counts, and fallback remain missing | G001, G003 |
| S005 | Apple Silicon macOS AArch64 JIT is qualified through gcnport | missing | requires S003 | G001, G003 |
| S006 | Android arm64-v8a JIT is qualified through gcnport | missing | requires S003 | G001, G003 |
| S007 | Local C++/Python structure and verification gate is reproducible | verified | Clang/Ninja gate and controlled negatives pass | G003 |
| S008 | Asset-free hosted synthetic-JIT verification covers supported native desktop hosts | partial | pinned full-history workflow defines Linux x64/arm64, Windows x64, and macOS x64/arm64 execution; hosted results remain pending until the workflow runs | G003 |

## Capability details

### S001 — execution policy

Evidence: `ExecutionSession` always enters `RuntimeBackend::execute_jit_block` in its default mode.
Only a typed `JitRefusal` can reach `execute_refused_block`; fallback is disabled until all limits
are explicitly configured, and `BackendFault` stops without interpretation. `FallbackLedger`
enforces per-block, total-block, and total-instruction budgets and records per-reason
counts alongside JIT compile/execution denominators. The focused tests prove JIT-first, fallback-
then-JIT, hard backend fault, budget exhaustion, and separate diagnostic mode.

This verifies framework policy, not Dolphin execution.

### S002 — hooks and original-call state

Evidence: `NativeHookRegistry` rejects unauthenticated keys, selects by SHA-256 identity plus module
generation plus aligned PPC address, and invalidates on install/replacement/removal.
`OriginalCallCoordinator` issues single-use tickets, rejects a different generation and duplicate
claims, and invalidates before and after a claimed call. Focused tests exercise both matching and
controlled-negative identities.

The fork must still execute the claimed ticket as an unpublished, unlinked JIT block; the state
machine alone does not prove that backend property.

### S003 — Dolphin embedding contract

Issue 001 remains open. Pinned fork revision
`9dfd5ac1f4c0c2d9da7661e1895a39b293286521` adds an instance-owned
`PowerPC::GcnPort::RuntimeSession`, exact digest/generation/address hook selection, Jit64 and JitArm64
generated hook guards, PPC analyzer may-exit liveness, real cache invalidation, and typed cold/cache/
hook/original-entry counters. The x86_64 shipping-JIT test passes. The pinned contract probe reports
six of eleven operations still absent.

Authenticated image boot, public one-block execution, bounded typed fallback, explicit diagnostic
interpretation, and synchronous native → original → native continuation remain missing. The existing
Dolphin instruction-lowering fallback is still untyped and therefore cannot support a no-interpreter
gameplay claim.

### S004 — x86_64 execution

Partial capability: `GcnPortRuntime.ShippingJitCacheHookOriginalAndInvalidation` runs a redistributable
PPC arithmetic/branch program through Dolphin's ordinary JIT64 loop. Generated instrumentation proves
a cold compilation, cache/direct-link entries, hook-triggered invalidation and recompilation, one
ordinary-body execution followed by hook re-entry, and a controlled-negative identity generation.
All 1,344 Dolphin unit tests pass in the Clang/Ninja evidence build.

The gcnport `RuntimeBackend` adapter, one-observable-block exit, exact retired-instruction counters,
typed bounded fallback, publication-failure injection, and a synchronous original-call continuation
remain missing.

### S005 — Apple Silicon execution

Missing capability: the hosted workflow now schedules the synthetic shipping-JIT discriminator on
Apple Silicon macOS, but no hosted result has run from this uncommitted workflow. MAP_JIT/write-
protection transitions, instruction-cache coherence, ABI transitions, exceptions/signals, hooks,
original calls, and representative gameplay still require evidence.

### S006 — Android execution

Missing capability: there is no real NDK/APK/device runtime boundary, so hosted CI deliberately has
no Android job. Integrate the fork contract into Android arm64-v8a and qualify executable-memory
publication, cache coherence, ABI transitions, lifecycle/exceptions, hooks, original calls,
packaging, and representative gameplay. macOS AArch64 evidence cannot substitute for this item.

### S007 — project quality gate

Evidence: `tools/verify.py` uses the locked Python environment, Clang, and Ninja; builds and runs both
focused C++ tests; checks installability, `clang-format`, and `clang-tidy`; runs structure, dependency,
Dolphin-contract, and CI-contract probes; and exercises planted positive/negative controls.
`tools/check_structure.py` checks 1,200-line limits and rejects direct product output or environment
reads. The maintained fork is a submodule rather than copied first-party source.

### S008 — hosted synthetic runtime

Partial capability: `.github/workflows/hosted-verification.yml` checks out full recursive history
with immutable action revisions and calls the same `tools/verify.py --runtime` entry point on native
Linux x64/arm64, Windows x64, and macOS x64/arm64 runners. The verifier rejects a runner identity
mismatch, checks the selected CMake compiler family, asserts that the exact synthetic runtime test is
present, and requires exactly one non-skipped pass. This workflow has not run while the change is
uncommitted, so it is configuration evidence rather than host qualification evidence.
