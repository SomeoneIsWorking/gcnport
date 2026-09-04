# Codemap

```text
title composition
  -> immutable runtime configuration + diagnostics sink
  -> gcnport execution session
       -> fallback ledger
       -> native hook registry
       -> original-call coordinator
       -> Dolphin runtime backend
            -> Dolphin PowerPC JIT/cache/memory/devices
```

| Subsystem | Responsibility | Current / target location | Entry point | Deep doc |
| --- | --- | --- | --- | --- |
| Execution types | Typed modes, JIT results/refusals/faults, budgets, counters, exits | `include/gcnport/execution_types.h` | `RuntimeConfiguration`, `ExecutionStatistics` | `docs/dolphin-embedding-contract.md` |
| Execution policy | JIT-first block dispatch and bounded fallback routing | `include/gcnport/execution_session.h`, `src/execution_session.cpp` | `ExecutionSession::run` | `docs/dolphin-embedding-contract.md` |
| Fallback accounting | Validate refused-block execution and update reasoned counters | `include/gcnport/fallback_ledger.h`, `src/fallback_ledger.cpp` | `FallbackLedger::record` | `docs/dolphin-embedding-contract.md` |
| Backend boundary | One-block JIT, refused-block fallback, and diagnostic interpretation | `include/gcnport/runtime_backend.h` | `RuntimeBackend` | `docs/dolphin-embedding-contract.md` |
| Native CPU/memory view | Narrow register and guest-memory access for native hooks | `include/gcnport/guest_context.h` | `GuestContext` | `docs/dolphin-embedding-contract.md` |
| Hook selection | Authenticated image/module/address registration and invalidation | `include/gcnport/native_hooks.h`, `src/native_hooks.cpp` | `NativeHookRegistry` | `docs/dolphin-embedding-contract.md` |
| Original calls | Single-use exact-key suppression tickets and invalidation | `include/gcnport/original_calls.h`, `src/original_calls.cpp` | `OriginalCallCoordinator` | `docs/dolphin-embedding-contract.md` |
| Diagnostics | Typed runtime event delivery without direct process output | `include/gcnport/diagnostics.h` | `DiagnosticsSink` | `AGENTS.md` |
| Dolphin backend | Boot/runtime ownership, JIT observation, hooks, original calls, invalidation | Maintained fork `Source/Core/Core/PowerPC/GcnPortRuntime.*` plus future adapter source | `PowerPC::GcnPort::RuntimeSession` | `docs/dolphin-embedding-contract.md` |
| Dependency pin | Exact maintained-fork checkout identity | `dependencies.json`, `.gitmodules`, `cmake/DolphinDependency.cmake`, `extern/dolphin/` | `gcnport_require_dolphin_checkout` | `README.md` |
| Verification tooling | Structure, dependency, CI-contract, build, test, format, lint, and native synthetic-JIT orchestration | `tools/`, `.github/workflows/hosted-verification.yml` | `tools/verify.py` | `README.md` |

## Placement index

- PPC decode/lowering/emission/cache/memory/device behavior → maintained Dolphin fork.
- JIT/refusal policy or execution counters → gcnport execution owner.
- Guest-address hook selection or one-call suppression → gcnport hook/original owners.
- Exact title digest, address, object layout, or native renderer behavior → consuming title.
- Product log formatting/filtering → consuming title's Lucent adapter over `DiagnosticsSink`.
- CLI, environment, or persisted configuration parsing → consuming title's configuration owner.
