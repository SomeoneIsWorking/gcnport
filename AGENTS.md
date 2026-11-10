# gcnport agent guidance

`gcnport` owns title-neutral GameCube runtime execution for native/dynarec hybrid ports. Its
contract with Dolphin is `docs/dolphin-embedding-contract.md`; module map `docs/codemap.md`.
Execution policy (JIT default, bounded fallback, invalidation, per-host backends): the
`dynarec-port`, `dynarec-runtime` and `dynarec-overrides` skills.

## Runtime contract

- Dolphin owns PowerPC decoding, lowering, host emission, code memory, block cache, memory, and
  devices. Do not duplicate those systems in `gcnport` or put them behind a second cache.
- `gcnport` owns typed execution policy, authenticated image/module identity, native hook selection,
  original-call tickets, bounded exits, invalidation requests, and observable counters.
- A module-generation change or savestate restore invalidates every cached path that could bypass
  the current hook decision, like a hook change or executable write.
- An original call suppresses exactly one hook entry through a consumed ticket and an unpublished,
  unlinked JIT block. Never toggle a global hook flag or publish the original block into the ordinary
  cache.
- Hosts: x86_64, Apple Silicon macOS AArch64, and Android arm64-v8a.

## Structure

- Do not introduce a service locator, singleton registry, or title-specific code.
- Product code emits typed diagnostic events and does not write stdout/stderr; consumers route them
  through their Lucent logger. It receives validated immutable configuration from the consumer.
- There is no launcher: the project has no runnable product.
- Full verifier: `uv run --frozen python tools/verify.py` (Clang/Ninja).
