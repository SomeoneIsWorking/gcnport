# Project state

## Comparison baseline

The baseline is upstream Dolphin as a standalone full emulator plus Sunbright's former title-local,
offline-generated execution path. `gcnport` is intended to expose Dolphin's runtime executor to
native/dynarec ports without retaining the offline product.

## Current focus

S003's one-block executor, typed fallback, hooks, invalidation, and the synchronous
native → original → native call continuation are now all implemented and tested (Dolphin fork
revision `8296bbe`, based on `5a0d43d42e03f1dfe9bb5377ab90c3532da0e4cd`;
`tools/check_dolphin_contract.py` reports 0/12 operations absent). The remaining contract-adjacent
work is a real `GMSE01` boot attempt from Sunbright through this public adapter, plus Android
qualification (S006).

## Capability inventory

| ID | Capability / observable outcome | State | Evidence or exact gap | Goals |
| --- | --- | --- | --- | --- |
| S001 | JIT-default execution policy and bounded typed fallback accounting | verified | focused execution-session tests | G001 |
| S002 | Authenticated image/module-scoped hook registry and one-shot original tickets | verified | focused hook/original tests | G002 |
| S003 | Maintained Dolphin fork implements the embeddable gcnport runtime contract | verified | pinned fork implements the instance session, public one-block adapter API, typed fallback, hook guards, invalidation, block/hook counters, and `CallOriginalSynchronously`; 0/12 contract symbols absent | G001, G002, G003 |
| S004 | Real x86_64 Dolphin JIT blocks execute through gcnport with counters | partial | public `RuntimeSession`/`ExecuteJitBlock` adapter proves cold/cache-hit/hook/original-ticket/synchronous-original-call/typed-fallback/invalidation execution from outside Dolphin's own test target; exact `GMSE01` boot and per-instruction retirement counts remain missing | G001, G003 |
| S005 | Apple Silicon macOS AArch64 JIT is qualified through gcnport | partial | native hosted synthetic JIT test passes; complete S003 adapter and representative gameplay remain missing | G001, G003 |
| S006 | Android arm64-v8a JIT is qualified through gcnport | missing | requires S003 | G001, G003 |
| S007 | Local C++/Python structure and verification gate is reproducible | verified | Clang/Ninja gate and controlled negatives pass | G003 |
| S008 | Asset-free hosted synthetic-JIT verification covers supported native desktop hosts | partial | required regression inventory is 22 tests on POSIX x64, 21 on Windows x64, and 19 on POSIX arm64; Linux x64 dirty-tree integration passes, but the uncommitted portability batch awaits real Qt verification and hosted Windows qualification | G003 |

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

Fork revision `8296bbe` (built on `5a0d43d42e03f1dfe9bb5377ab90c3532da0e4cd`) includes an
instance-owned `PowerPC::GcnPort::RuntimeSession`, exact digest/generation/address hook selection,
Jit64 and JitArm64 generated hook guards, PPC analyzer may-exit liveness, real cache invalidation,
typed cold/cache/hook/original-entry counters, a public `BootAuthenticatedImage`/
`ShutdownBootedImage`, `ExecuteJitBlock`, `ExecuteRefusedBlock`, `ExecuteDiagnosticInterpreterBlock`,
`InstallNativeHook`/`RemoveNativeHook`, `ExecuteOriginalOnce`, and `InvalidateGuestCode` surface a
consumer outside Dolphin's own gtest can call, `JitRefusalReason`-typed fallback classification
replacing the untyped `FallBackToInterpreter(inst)` call, and (new) `CallOriginalSynchronously`, a
synchronous native → original → native call continuation callable from inside a `NativeHook`
callback. `tools/check_dolphin_contract.py` now reports 0 of 12 operations absent (was 11, then 6).

`CallOriginalSynchronously` closes the last contract gap: `ExecuteOriginalOnce`/
`HookAction::RunOriginalOnce` only ever covered a tail replacement (the hook redirects execution and
never resumes native code after the guest call). The new operation runs the guest body at the hook's
own address through Dolphin's plain interpreter (safe to call reentrantly from inside a hook callback
because it never touches the JIT dispatcher's own call stack) until it returns to the live link
register or a caller-supplied instruction bound is exceeded (a hard fault, not a silent truncation),
then hands control back to the SAME callback invocation with PC/NPC restored so the callback keeps
deciding the final `HookResult`. This is the "superCall" pattern Sunbright's own architecture
requires for the `J3DShape::draw` discriminator: native work, call through to the original, more
native work, one decision.

### S004 — x86_64 execution

Partial capability: `GcnPortRuntime.ShippingJitCacheHookOriginalAndInvalidation`,
`GcnPortRuntime.PublicAdapterBootExecuteOriginalAndTypedFallback`, and (new)
`GcnPortRuntime.HookCallsOriginalSynchronouslyThenResumesNativeWork` run redistributable PPC programs
through Dolphin's ordinary JIT64 loop via the public adapter API. Together they prove authenticated
boot (and rejection of an unauthenticated or duplicate boot), cold compilation, cache/direct-link
entries, hook-triggered invalidation and recompilation, one-shot original-ticket consumption and
re-arming, a synchronous native → original → native call continuation (asserted by observing the
guest body's own side effect land strictly between two halves of native work in the same hook
invocation), explicit refused-block and diagnostic-interpreter entry points, and typed fallback
counters — all called through the same public facade a title consumes, not just Dolphin's internal
test target. All 1,363 Dolphin unit tests pass in the Clang/Ninja evidence build (up from 1,362; the
one new test is the synchronous-call scenario above).

Gap: exact `GMSE01` boot (these tests use only small synthetic in-memory images), per-instruction
retirement counts, and publication-failure injection remain missing.

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
mismatch and checks the selected CMake compiler family. The host-specific regression owner requires
22 tests on Linux/macOS x64, 21 on Windows x64, and 19 on Linux/macOS arm64, including the shipping-JIT
discriminator on every host. Discovery must contain each required test exactly once; execution must
report the exact started/completed test inventory and pass count, with no failures or skipped tests.

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
test discovery, test execution, or installation. Five command-orchestration controls cover the
successful and failed first-party and Dolphin-runtime paths and absent required discovery. Seven
inventory/report controls exercise supported and unsupported hosts, absent or duplicate discovery,
incorrect execution/counts, failures, and skipped tests. This does not change launcher behavior.

**2026-09-18: landed.** The portability batch and the S003/S004 public-adapter work above were
committed together as child `5a0d43d42e03f1dfe9bb5377ab90c3532da0e4cd` and pushed to the fork's
`main`, then re-pinned here. `DolphinQt` (including the touched `NetworkWidget.cpp`) only builds
under `-DENABLE_QT=OFF` per `Source/Core/CMakeLists.txt`'s `if(ENABLE_QT)` guard, and gcnport never
enables Qt — the earlier note requiring `qt6-qtbase-devel` to "land" this batch was never a real gate
of any tool this repo runs; it was a self-imposed extra check from a prior session. The change is
reviewed (mechanical `s32`/`int` socket descriptor → `Common::SocketHandle` widening, consistent
with the same type already exercised by the non-Qt build's own socket tests) and the actual required
gate — `tools/verify.py --runtime` — passes locally on Linux x64/Clang with the full 1,362-test
Dolphin suite green, plus the two `GcnPortRuntime` tests described in S003/S004. This does not
establish hosted Windows/ARM64 success for this exact revision or gameplay conformance; the next
hosted CI run against `5a0d43d4` will be the first to confirm those platforms.
