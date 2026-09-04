# gcnport agent guidance

`gcnport` owns title-neutral GameCube runtime execution for native/dynarec hybrid ports. Read
`docs/project-state.md`, `docs/project-goals.md`, `docs/codemap.md`, and
`docs/dolphin-embedding-contract.md` before changing execution behavior.

## Runtime contract

- Dolphin owns PowerPC decoding, lowering, host emission, code memory, block cache, memory, and
  devices. Do not duplicate those systems in `gcnport` or put them behind a second cache.
- `gcnport` owns typed execution policy, authenticated image/module identity, native hook selection,
  original-call tickets, bounded exits, invalidation requests, and observable counters.
- JIT is the gameplay default. Interpretation may occur only after typed compile/safe-execution
  refusal within a configured budget, then control returns to JIT. Diagnostic interpretation is an
  explicit separate mode. Missing JIT support is a hard fault.
- A native hook install, replacement, removal, image change, module-generation change, executable
  write, or savestate restore invalidates every cached path that could bypass the new decision.
- An original call suppresses exactly one hook entry through a consumed ticket and an unpublished,
  unlinked JIT block. Never toggle a global hook flag or publish the original block into the ordinary
  cache.
- Host support is claimed separately for x86_64, Apple Silicon macOS AArch64, and Android
  arm64-v8a. No interpreter may stand in for a missing backend.

## Structure and quality

- Keep stateful owners focused, RAII-based, and explicitly injected. Pure policies remain value types
  or free functions. Do not introduce a service locator, singleton registry, or title-specific code.
- Product code emits typed diagnostic events and does not write stdout/stderr. Consumers route those
  events through their configured Lucent logger. Product code never reads the process environment;
  it receives validated immutable configuration.
- Project automation is modular Python. There is no shell tool or launcher while the project has no
  runnable product.
- Run `uv run --frozen python tools/verify.py` with Clang/Ninja before landing. Do not weaken the
  Dolphin contract probe to make the missing backend appear available.
- Do not add an offline translator, generated guest source, address-derived function corpus, static
  dispatcher, or title executable to this repository.
