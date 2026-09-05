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
| S005 | Apple Silicon macOS AArch64 JIT is qualified through gcnport | partial | native hosted synthetic JIT test passes; complete S003 adapter and representative gameplay remain missing | G001, G003 |
| S006 | Android arm64-v8a JIT is qualified through gcnport | missing | requires S003 | G001, G003 |
| S007 | Local C++/Python structure and verification gate is reproducible | verified | Clang/Ninja gate and controlled negatives pass | G003 |
| S008 | Asset-free hosted synthetic-JIT verification covers supported native desktop hosts | partial | Linux x64/arm64 and macOS x64/arm64 pass in run 33893139587; Windows passed accelerated AES compilation in run 33960614709, then exposed incomplete rename-structure initialization and awaits the corrected fork | G003 |

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
`818ef9de938b3672880f5ff1468729fdaf643679` includes an instance-owned
`PowerPC::GcnPort::RuntimeSession`, exact digest/generation/address hook selection, Jit64 and JitArm64
generated hook guards, PPC analyzer may-exit liveness, real cache invalidation, and typed cold/cache/
hook/original-entry counters. The x86_64 shipping-JIT test passes. The pinned contract probe reports
six of eleven operations still absent.

Gap: authenticated image boot, public one-block execution, bounded typed fallback, explicit diagnostic
interpretation, and synchronous native → original → native continuation remain missing. The existing
Dolphin instruction-lowering fallback is still untyped and therefore cannot support a no-interpreter
gameplay claim.

### S004 — x86_64 execution

Partial capability: `GcnPortRuntime.ShippingJitCacheHookOriginalAndInvalidation` runs a redistributable
PPC arithmetic/branch program through Dolphin's ordinary JIT64 loop. Generated instrumentation proves
a cold compilation, cache/direct-link entries, hook-triggered invalidation and recompilation, one
ordinary-body execution followed by hook re-entry, and a controlled-negative identity generation.
All 1,344 Dolphin unit tests pass in the Clang/Ninja evidence build.

Gap: the gcnport `RuntimeBackend` adapter, one-observable-block exit, exact retired-instruction counters,
typed bounded fallback, publication-failure injection, and a synchronous original-call continuation
remain missing.

### S005 — Apple Silicon execution

Partial capability: [hosted run 33893139587](https://github.com/SomeoneIsWorking/gcnport/actions/runs/33893139587)
passes the synthetic shipping-JIT discriminator on native Apple Silicon macOS. This exercises cold
translation, cached execution, hooks, original-entry suppression, and invalidation. Gap: complete S003
adapter coverage, publication-failure tests, exception/signal handling, and representative gameplay
still require evidence.

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
present, and requires exactly one non-skipped pass.

[Run 33893139587](https://github.com/SomeoneIsWorking/gcnport/actions/runs/33893139587)
passed both Linux architectures and both macOS architectures. Windows failed while compiling the
bundled C image library because Dolphin passed unsupported MSVC options to clang-cl. The fork now
uses its existing per-language compiler-option probes; a Windows-target clang-cl C/C++ compile
rejects all four unsupported options and accepts a supported UTF-8 positive control with warnings
treated as errors.

[Windows job 101286556368](https://github.com/SomeoneIsWorking/gcnport/actions/runs/33958682030/job/101286556368)
advanced past compiler-option and header-search failures, then rejected a shared binary PCH built
without the consuming targets' dependency macros. The PCH owner now uses CMake's per-target
precompilation through the existing `use_pch` interface. Each target builds the same header with its
own definitions and options; CMake owns its header path and build ordering. A Windows-target
clang-cl C++23 probe using the production CMake owner builds two consumers with distinct macro
values and asserts the corresponding precompiled values. Forcing binary PCH reuse reproduces the
macro mismatch; an unchanged second positive build performs no compilations.

[Windows job 101288561785](https://github.com/SomeoneIsWorking/gcnport/actions/runs/33959422491/job/101288561785)
passed PCH compilation, then exposed AES attributes disabled by clang-cl's `_MSC_VER` compatibility
macro. `Common/Intrinsics.h` now owns compiler-aware function targeting for AES, SHA1, CPU culling,
and the existing SSE helpers. Runtime CPU checks and baseline implementations are unchanged. A
clang-cl probe using that owner emits AES, SHA and AVX/FMA instructions without global ISA flags;
removing attribution reproduces the failure. The three affected production translation units also
compile with native Clang.

[Windows job 101291749334](https://github.com/SomeoneIsWorking/gcnport/actions/runs/33960614709/job/101291749334)
passed accelerated AES compilation, then rejected omitted fields in `DirectIOFile.cpp`'s
`FILE_RENAME_INFO` initializer. The pinned fork explicitly initializes `RootDirectory` to null and
the filename placeholder to zero; the existing bounded buffer still supplies the destination path.
A real Windows-target clang-cl probe extracts the shipping initializer and checks both pre-RS1
and RS1-union SDK layouts, their ABI offsets, and field values. Both positive cases compile; both
omitted-field controls reproduce the diagnostic under warnings-as-errors. This standalone check
does not include the full Windows SDK or execute the NT rename API. Gap: full Windows runtime
qualification remains unsupported pending a passing hosted runtime gate.

The canonical verifier passes Ninja `-k 0` through its shared build owner so independent compiler
failures are collected in one build attempt. A nonzero build still propagates immediately before
test discovery, test execution, or installation. Four command-orchestration controls cover the
successful and failed first-party and Dolphin-runtime paths; this does not change launcher behavior.
