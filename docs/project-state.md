# Project state

## Comparison baseline

The baseline is upstream Dolphin as a standalone full emulator plus Sunbright's former title-local,
offline-generated execution path. `gcnport` is intended to expose Dolphin's runtime executor to
native/dynarec ports without retaining the offline product.

## Current focus

S003 is the current focus: add the title-neutral fork API that connects the verified gcnport policy
to Dolphin's real JIT, hook dispatch, one-shot original execution, invalidation, and counters.

## Capability inventory

| ID | Capability / observable outcome | State | Evidence or exact gap | Goals |
| --- | --- | --- | --- | --- |
| S001 | JIT-default execution policy and bounded typed fallback accounting | verified | focused execution-session tests | G001 |
| S002 | Authenticated image/module-scoped hook registry and one-shot original tickets | verified | focused hook/original tests | G002 |
| S003 | Maintained Dolphin fork implements the embeddable gcnport runtime contract | blocked | issue 001: all 11 required fork seams are absent | G001, G002, G003 |
| S004 | Real x86_64 Dolphin JIT blocks execute through gcnport with counters | missing | requires S003 | G001, G003 |
| S005 | Apple Silicon macOS AArch64 JIT is qualified through gcnport | missing | requires S003 | G001, G003 |
| S006 | Android arm64-v8a JIT is qualified through gcnport | missing | requires S003 | G001, G003 |
| S007 | Local C++/Python structure and verification gate is reproducible | verified | Clang/Ninja gate and controlled negatives pass | G003 |

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

Blocker: issue 001. At pinned Dolphin revision
`7fd812471e8f2030ccde7081b7c329aa252d5360`, the only guest-dispatch interception is the title-named
`sb_slot_jit_trampoline`, which runs only on a cache miss. The fork exposes no one-block runtime
session, no title-neutral hook API, no unpublished one-shot original block, no typed runtime fallback
events, and no actual block-execution denominators. `tools/check_dolphin_contract.py` reports all
eleven missing surface requirements and its self-test proves both present and missing text fixtures;
the probe deliberately does not claim that finding symbols proves executable semantics.

The pinned commit is published on the maintained fork's `sunbright` branch, so
fresh submodule initialization is reproducible.

### S004 — x86_64 execution

Missing capability: build the fork adapter with Clang/Ninja, execute a real cold PPC block through JIT64, prove a
cache hit and invalidation/recompile, exercise native/disabled/original hook paths, and report nonzero
JIT denominators plus bounded fallback reasons.

### S005 — Apple Silicon execution

Missing capability: qualify the shipping JitArm64 backend on Apple Silicon macOS, including MAP_JIT/write-
protection transitions, instruction-cache coherence, ABI transitions, exceptions/signals, hooks,
original calls, and representative gameplay.

### S006 — Android execution

Missing capability: integrate the same fork contract into Android arm64-v8a and qualify executable-memory
publication, cache coherence, ABI transitions, lifecycle/exceptions, hooks, original calls, packaging,
and representative gameplay. macOS AArch64 evidence cannot substitute for this item.

### S007 — project quality gate

Evidence: `tools/verify.py` uses the locked Python environment, Clang, and Ninja; builds and runs both
focused C++ tests; checks `clang-format` and `clang-tidy`; runs structure and Dolphin-contract probes;
and exercises planted positive/negative controls. `tools/check_structure.py` checks 1,200-line limits
and rejects direct product output or environment reads. The maintained fork is a submodule rather than
copied first-party source.
