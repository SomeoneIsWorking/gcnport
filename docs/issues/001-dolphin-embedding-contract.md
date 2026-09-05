---
id: 001
state: open
affects: S003 S004 S005 S006
---

# Dolphin lacks the title-neutral gcnport execution seam

Pinned revision `6a00a76230b7474e30af5786fd633cba5f6dbebc` implements the title-neutral runtime
session, generated Jit64 and JitArm64 hook guards, hook-aware invalidation, tail-only original
fallthrough, and real cold/cache/hook counters.

Its synthetic x86_64 shipping-JIT discriminator and the full Dolphin unit-test binary pass. The guard
is re-entered by recursion and direct links, so it does not use global suppression.

Resolve the remaining issue by adding authenticated boot, public one-block execution, typed bounded
fallback, diagnostic-only interpretation, and synchronous native → original → native continuation,
then build the actual gcnport adapter.

The current pin is still missing six of the eleven absence-probe requirements. Symbol presence is
not the completion gate.
