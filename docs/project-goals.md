# Project goals

## G001 — Shared runtime GameCube execution

**Outcome:** Embed Dolphin's maintained PowerPC JIT and GameCube device model behind one reusable,
title-neutral runtime executor.

**Why it matters:** Native ports need complete original-game coverage without generating a second
title-specific source program offline.

**Success conditions:**

- The executor boots an authenticated user-supplied GameCube image and executes ordinary cold blocks
  through Dolphin's x86_64 or AArch64 JIT before they run.
- Missing or failed JIT support hard-fails; bounded fallback follows only a typed compile or safe-
  execution refusal and reports guest PC, block/instruction counts, reasons, and JIT denominators.
- Interpreter-only mode is diagnostic and cannot qualify gameplay or performance.
- CPU/thread state, bounded exits, executable writes, savestate restore, and cache invalidation remain
  under explicit instance-owned lifetime.

**Constraints and non-goals:** Dolphin continues to own decoding, lowering, emission, memory, cache,
and devices. `gcnport` does not implement another PowerPC engine or generate title code.

Contributing state items: S001, S003, S004, S005, S006.

## G002 — Robust native ownership seams

**Outcome:** Let titles replace deliberate guest functions while preserving exact guest identity and
an independently runnable original JIT body.

**Why it matters:** Native subsystems cannot be trusted if cache hits, block links, overlays, or an
original call can silently bypass the current hook decision.

**Success conditions:**

- Hook keys contain authenticated image identity, module generation, and guest address.
- Installs, replacements, removals, executable changes, and restores revoke every cached link that
  captured a prior decision.
- A normal call honors the hook; one original-call ticket suppresses only that entry and executes an
  unpublished, unlinked JIT block, so recursion still observes the hook.
- Hooks and original calls work through the same API on x86_64 and AArch64.

**Constraints and non-goals:** Hooks do not replace missing instruction semantics and do not encode a
title's addresses or object layouts in this shared project.

Contributing state items: S002, S003.

## G003 — Portable and maintainable framework

**Outcome:** Provide a cohesive C++20 library and Python verification surface that can be consumed by
GameCube title projects without forked policy.

**Why it matters:** The shared executor is a long-lived platform boundary, not a one-title patch set.

**Success conditions:**

- Focused owners expose narrow typed interfaces; configuration is immutable and diagnostics are
  injected for routing to Lucent.
- Mechanical checks enforce source limits, formatting, linting, ownership boundaries, and the absence
  of offline generated-code paths.
- The maintained Dolphin fork is pinned reproducibly and the framework qualifies x86_64, Apple
  Silicon macOS AArch64, and Android arm64-v8a independently.

**Constraints and non-goals:** A host emitter test or one AArch64 operating system is not platform
qualification, and boot alone is not gameplay compatibility.

Contributing state items: S003, S004, S005, S006, S007.
