---
id: 001
state: open
affects: S003 S004 S005 S006
---

# Dolphin lacks the title-neutral gcnport execution seam

Pinned revision `7fd812471e8f2030ccde7081b7c329aa252d5360` exposes a Sunbright-named cache-miss
trampoline, but not the block lifecycle required by `docs/dolphin-embedding-contract.md`.

Resolve this in the maintained fork by implementing the title-neutral runtime session, runtime
fallback instrumentation, hook-aware link invalidation, and unpublished one-shot originals. Then pin
the pushed fork commit here and validate real x86_64 execution before changing this issue or S003.

The current pin is published on the maintained fork's `sunbright` branch; the
remaining blocker is the missing embedding contract itself.
