# Dolphin embedding contract

This document specifies the fork changes that turn the framework policy into a real GameCube runtime
executor. The native-hook slice described below is pinned and has executable x86_64 evidence; the
remaining operations are still a contract rather than implemented capability.

## Evidence at the pinned revision

The inspected dependency is `SomeoneIsWorking/dolphin` revision
`804be144296bfca46931d76cd9ff9f4b0f4e4e4b`, published on the fork's `main` branch and pinned by the
gcnport gitlink.

- `JitBase::Dispatch` returns an already-published block from Dolphin's block cache.
- The generated x86_64 and AArch64 dispatchers call `JitTrampoline` only after cache lookup fails.
- The older `sb_slot_jit_trampoline` cache-miss boundary remains insufficient on its own. The pinned
  slice instead emits title-neutral runtime-session guards in Jit64 and JitArm64 guest operations.
- `JitInterface::InvalidateICache` can revoke translated ranges and their links, which is the correct
  backend primitive for hook/image invalidation.
- `Jit64::FallBackToInterpreter` and `JitArm64::FallBackToInterpreter` emit direct interpreter calls
  into generated blocks. They receive only an instruction word, not a typed reason, and expose no
  runtime execution event or denominator to `gcnport`.
- `CPUCoreBase::Run` enters Dolphin's open-ended CPU loop. There is no instance API that executes one
  observable JIT block, reports a bounded host exit, or performs an unpublished one-shot original.
- `Core::System::GetInstance()` remains widely used. The first adapter may explicitly constrain the
  process to one live Dolphin system, but it must make that constraint executable and cannot present
  it as multi-instance support.

The existing trampoline is insufficient for original calls. Letting it return `false` compiles and
publishes the hooked target. Recursive calls and cache-linked callers can then execute that published
block without consulting the hook, so a global suppression flag cannot mean “only this invocation.”

The pinned implementation emits a hook guard before every hooked operation in the published
block. `RunOriginalOnce` falls through that guard for only the current entry; a loop, recursive call,
cache hit, or direct-linked entry reaches the guard again. This is sufficient for a tail replacement
that does not resume native code after the guest body. A synchronous native → original → native call
still needs an explicit continuation and remains missing.

## Required Dolphin owner

Add `Source/Core/Core/PowerPC/GcnPortRuntime.h/.cpp` to the maintained fork and a `core` CMake source
entry. It should define an instance-owned `PowerPC::GcnPort::RuntimeSession` associated with one
`Core::System`. The session implements `gcnport::RuntimeBackend` and `gcnport::CodeInvalidator` in a
small adapter outside Dolphin internals, or exposes an equivalent narrow C++ facade that the adapter
implements without reaching into private JIT fields.

`tools/check_dolphin_contract.py` is only an absence detector for this expected surface. Finding the
spelled symbols cannot prove that they are live, correctly wired, or semantically complete; only the
fork-backed executable test below can close S003.

The public surface needs these semantic operations (the exact spelling is deliberately checked by
`tools/check_dolphin_contract.py` so absence cannot look green):

| Operation | Required behavior |
| --- | --- |
| `RuntimeSession` | RAII ownership tied to one `Core::System`; refuse a second live session while Dolphin remains singleton-bound. |
| `BootAuthenticatedImage` | Boot only after receiving the caller-validated image digest and generation; preserve that identity on every event. |
| `ExecuteJitBlock` | Offer the current cold PC to JIT64/JitArm64, execute exactly one observable basic block, and return compile/cache-hit/instruction data or a bounded exit. An unavailable JIT returns a backend fault. |
| `ExecuteRefusedBlock` | Execute only the explicitly refused PC and no more than the supplied instruction bound, then return to JIT dispatch. |
| `ExecuteDiagnosticInterpreterBlock` | Execute only through an explicit diagnostic session; never share the gameplay selector. |
| `InstallNativeHook` | Make hook selection part of every entry path, including cache hits and block links, using the full image/generation/address key. |
| `ExecuteOriginalOnce` / `RunOriginalOnce` | Consume one matching call and run the ordinary body once without making recursive, cache-hit, or linked entries bypass the hook. An unpublished block is required for dispatcher-only interception; an always-emitted per-operation guard may instead fall through only the current guard. Synchronous native continuation after the guest returns remains a separate required operation. |
| `InvalidateGuestCode` | Revoke affected blocks and direct links for hook changes, executable writes, module changes, and restore events before execution resumes. |
| `ExecutionCounters` | Report actual compiled blocks, cache-hit executions, total JIT block/instruction executions, invalidations, hook calls, originals, and runtime fallback blocks/instructions by reason. |

## Dispatch ordering

For gameplay mode, one dispatch has this order:

1. Read the live PC and current authenticated image/module generation.
2. If an exact unconsumed original-call ticket is presented, claim it and enter the one-shot path.
3. Otherwise consult the exact native-hook key before cache lookup or linked entry.
4. If a hook handles the call, apply its explicit continuation/return state and resume at the
   dispatcher.
5. On a cache hit, execute the block and emit its actual block/instruction counters.
6. On a miss, safely fetch and compile the block, publish it, execute it, and report both compile and
   execution.
7. Only an explicit compile/safe-execution refusal may enter the bounded fallback. Record the typed
   reason and actual instruction count, then resume at step 1.
8. A missing backend, code-publication failure, corrupt state, or invalid result is a hard fault.

No asynchronous “interpret while compiling” path and no first-pass interpretation are allowed.

## Hook and invalidation mechanics

A correct implementation can use one of three shapes:

- emit a small hook-aware entry stub as the only published target for a hooked address; or
- force every edge to a hooked address through a hook-aware dispatcher and prevent a direct link; or
- emit a guard at the hooked guest operation in every block that contains it, and invalidate every
  containing block and inbound link whenever hook selection changes.

Both shapes must revoke older links when a hook changes. Checking only in the current
`JitTrampoline` cache-miss callback is not sufficient.

`OriginalCallCoordinator::begin` produces a ticket for one full key. A dispatcher-only implementation
must claim that ticket at the target before compilation, keep the resulting block out of the ordinary
lookup table, and destroy it on every exit. A guard implementation may consume the current hook result
by falling through the already-live guard, because every later entry still consults the guard. It must
not use a global suppression flag. Resuming the same native callback after guest return still requires
a bounded continuation owner and cannot be represented by the tail-only hook result.

Executable writes and MMU/image lifecycle changes call the same invalidation owner. A savestate
restore increments the runtime generation (or restores a generation that cannot alias stale cache
state), clears affected blocks, invalidates hook link stubs, and cancels outstanding original tickets.

## Fallback instrumentation

The fork must replace untyped calls to `FallBackToInterpreter(inst)` with an API that carries a
`JitRefusalReason` and compile PC. The generated runtime call records each actual execution; merely
counting that a fallback instruction was emitted is not sufficient. At minimum, distinguish:

- unsupported instruction lowering;
- unsafe instruction fetch;
- unsafe host execution; and
- a deliberately interpreter-owned privileged instruction.

JIT-disable configuration is not a fallback reason. Gameplay construction must reject it. Backend
absence, executable-memory failure, and host-code publication failure are backend faults.

## Host qualification

The shared API must not encode host registers or patch an observed host address. Both JIT64 and
JitArm64 implement the same fork facade, with architecture-specific code kept inside Dolphin.

After the x86_64 vertical slice is real, Apple Silicon macOS and Android arm64-v8a remain separate
gates. Each must exercise executable-memory write/execute transitions, instruction-cache coherence,
guest/host ABI transitions, exceptions/signals, cache hits, invalidation, hooks, one-shot originals,
and bounded fallback counters. Representative interactive gameplay is required later in the title;
an emitter unit test, boot sequence, FMV, or the other AArch64 OS is not substitute evidence.

## First executable test

The first fork-backed test should boot a small redistributable PowerPC test image rather than a game
disc and prove, through the shipping adapter:

1. a cold arithmetic block compiles and executes through the host JIT;
2. the second entry is a cache hit;
3. installing an exact-key hook invalidates that block and the hook runs;
4. a different image generation does not select the hook;
5. an original ticket runs the ordinary body once, a recursive entry still selects the hook, and the
   one-shot code is gone afterward;
6. a planted unsupported instruction produces one typed fallback event within budget, followed by a
   JIT block; and
7. a planted backend-publication failure hard-fails without interpreter execution.

Only after that test passes should a title consume the adapter and attempt the `GMSE01`
`J3DShape::draw` discriminator.

### Implemented pinned subset

`GcnPortRuntime.ShippingJitCacheHookOriginalAndInvalidation` uses Dolphin's ordinary CPU/JIT loop and
a three-instruction, redistributable PPC program. On x86_64 it proves a cold JIT64 compilation, later
cache/direct-link entries, exact identity-scoped hook selection, invalidation and recompilation on
hook install, one ordinary body execution followed by hook re-entry, and controlled-negative module
identity selection. Its counters are incremented by generated block-entry and hook guards, not by a
test-side model.

This does not implement or prove authenticated image boot, a public one-block executor, bounded typed
fallback, diagnostic-only interpretation, synchronous native continuation after an original call,
or per-instruction retirement counts. Hosted verification is configured to execute JitArm64 on
Apple Silicon macOS. Android remains unqualified and has no CI job until a real NDK/APK/device
runtime boundary exists; macOS AArch64 evidence cannot substitute for it.
