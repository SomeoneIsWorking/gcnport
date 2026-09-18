// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

#include "gcnport/execution_types.h"
#include "gcnport/native_hooks.h"
#include "gcnport/runtime_backend.h"

namespace Core {
class System;
}
namespace PowerPC {
struct PowerPCState;
namespace GcnPort {
class RuntimeSession;
enum class JitRefusalReason : std::uint8_t;
} // namespace GcnPort
} // namespace PowerPC

namespace gcnport {

// Makes Dolphin's RuntimeSession satisfy gcnport's backend contracts, so a consuming title programs
// against `gcnport::` alone and never includes a Dolphin header. Without this, every consumer
// reimplements the same conversions -- and Sunbright's boot tool, the only consumer so far, did
// exactly that: it reached straight into `Core/PowerPC/GcnPortRuntime.h` and carried its own copy
// of Dolphin's include directories, C++ standard selection and architecture macros because those
// arrive as directory properties of Dolphin's own CMakeLists rather than as usage requirements of a
// target. `gcnport::dolphin` is that target: link it and the whole set arrives.
//
// One adapter instance per RuntimeSession. Every method here must be called on Dolphin's CPU
// thread; hook mutations additionally require a stopped CPU safe point, exactly as
// RuntimeSession's own contract states.
class DolphinRuntimeAdapter final : public RuntimeBackend, public CodeInvalidator {
public:
  DolphinRuntimeAdapter(Core::System &system, PowerPC::GcnPort::RuntimeSession &session);
  ~DolphinRuntimeAdapter() override;
  DolphinRuntimeAdapter(const DolphinRuntimeAdapter &) = delete;
  DolphinRuntimeAdapter(DolphinRuntimeAdapter &&) = delete;
  DolphinRuntimeAdapter &operator=(const DolphinRuntimeAdapter &) = delete;
  DolphinRuntimeAdapter &operator=(DolphinRuntimeAdapter &&) = delete;

  // RuntimeBackend: exactly one guest basic block per call.
  [[nodiscard]] JitStep execute_jit_block() override;
  [[nodiscard]] InterpretedBlock
  execute_refused_block(const JitRefusal &refusal,
                        std::uint32_t maximum_instruction_count) override;
  [[nodiscard]] InterpretedBlock
  execute_diagnostic_interpreter_block(std::uint32_t maximum_instruction_count) override;

  // CodeInvalidator.
  void invalidate_instruction(GuestAddress address) override;

  // The identity the session is running under, so a consumer can build a HookKey without naming a
  // Dolphin type. Installing at an address under any other identity is refused by the session.
  [[nodiscard]] ExecutionIdentity identity() const;

  // Installs `hook` as the native override at `key.address`. The session remains the dispatch
  // authority -- it decides which hook fires and when -- and this class owns only the callable's
  // storage, because Dolphin's hook ABI is a raw function pointer plus a context pointer and a
  // std::function needs somewhere stable to live.
  //
  // `hook` runs inside generated code and therefore may not unwind: an exception escaping it
  // terminates the process rather than corrupting the JIT's call stack.
  void install_hook(HookKey key, NativeHook hook);
  [[nodiscard]] bool remove_hook(const HookKey &key);
  [[nodiscard]] std::size_t hook_count() const noexcept;

private:
  struct Binding;

  Core::System &system_;
  PowerPC::GcnPort::RuntimeSession &session_;
  std::map<HookKey, std::unique_ptr<Binding>> bindings_;
};

// The two vocabularies are separate on purpose -- gcnport's public headers stay free of Dolphin's
// -- which leaves their enumerators a hand-maintained mirror. These are where the mirror is
// crossed, and the adapter's own test walks every enumerator through them and compares the two
// owners' names, so a divergence fails a check instead of quietly mislabelling a fallback.
[[nodiscard]] JitRefusalReason to_gcnport(PowerPC::GcnPort::JitRefusalReason reason) noexcept;
[[nodiscard]] PowerPC::GcnPort::JitRefusalReason to_dolphin(JitRefusalReason reason) noexcept;

} // namespace gcnport
