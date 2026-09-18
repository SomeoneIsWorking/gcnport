// SPDX-License-Identifier: GPL-2.0-or-later
#include "gcnport/dolphin_adapter.h"

#include <cstring>
#include <span>
#include <stdexcept>
#include <utility>

#include "Core/HW/Memmap.h"
#include "Core/PowerPC/GcnPortRuntime.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace gcnport {
namespace {

[[nodiscard]] ImageIdentity to_gcnport(const PowerPC::GcnPort::ImageIdentity &image) noexcept {
  return ImageIdentity{.sha256 = image.sha256};
}

[[nodiscard]] PowerPC::GcnPort::ExecutionIdentity
to_dolphin(const ExecutionIdentity &identity) noexcept {
  return PowerPC::GcnPort::ExecutionIdentity{
      .image = PowerPC::GcnPort::ImageIdentity{.sha256 = identity.image.sha256},
      .module_generation = identity.module_generation};
}

[[nodiscard]] PowerPC::GcnPort::HookKey to_dolphin(const HookKey &key) noexcept {
  return PowerPC::GcnPort::HookKey{.identity = to_dolphin(key.identity), .address = key.address};
}

[[nodiscard]] PowerPC::GcnPort::HookResult to_dolphin(const HookResult &result) noexcept {
  switch (result.action) {
  case HookAction::ReturnToCaller:
    return PowerPC::GcnPort::HookResult::ReturnToCaller();
  case HookAction::ContinueAtAddress:
    return PowerPC::GcnPort::HookResult::ContinueAt(result.continuation);
  case HookAction::CallOriginalOnce:
    return PowerPC::GcnPort::HookResult::RunOriginalOnce();
  }
  // An action outside the enumeration is a caller bug, and returning to the caller would silently
  // skip the guest function the hook was standing in for. Run the original body instead: the title
  // keeps behaving, and the fallback is the conservative direction rather than the invisible one.
  return PowerPC::GcnPort::HookResult::RunOriginalOnce();
}

// Dolphin's guest CPU and memory, through gcnport's narrow view of them. Constructed per hook
// invocation on the stack: it holds references to state the session already owns, so there is
// nothing to keep alive between calls.
class DolphinGuestContext final : public GuestContext {
public:
  DolphinGuestContext(Memory::MemoryManager &memory, PowerPC::PowerPCState &state) noexcept
      : memory_(memory), state_(state) {}

  [[nodiscard]] GuestAddress program_counter() const noexcept override { return state_.pc; }
  void set_program_counter(GuestAddress value) noexcept override {
    state_.pc = value;
    state_.npc = value;
  }
  [[nodiscard]] GuestAddress link_register() const noexcept override { return state_.spr[SPR_LR]; }
  void set_link_register(GuestAddress value) noexcept override { state_.spr[SPR_LR] = value; }

  [[nodiscard]] std::uint32_t general_register(std::size_t index) const override {
    return state_.gpr[checked_register_index(index)];
  }
  void set_general_register(std::size_t index, std::uint32_t value) override {
    state_.gpr[checked_register_index(index)] = value;
  }

  // Dolphin owns what counts as a valid guest range, and GetPointerForRange is where it says so --
  // including raising its own panic alert, which the embedding process answers through its message
  // handler. Re-deriving the answer here would be a second, quieter address map that could disagree
  // with the one the JIT itself uses.
  [[nodiscard]] bool read_memory(GuestAddress address, std::span<std::byte> destination) override {
    const u8 *const source = memory_.GetPointerForRange(address, destination.size());
    if (source == nullptr) {
      return false;
    }
    std::memcpy(destination.data(), source, destination.size());
    return true;
  }

  [[nodiscard]] bool write_memory(GuestAddress address,
                                  std::span<const std::byte> source) override {
    u8 *const destination = memory_.GetPointerForRange(address, source.size());
    if (destination == nullptr) {
      return false;
    }
    std::memcpy(destination, source.data(), source.size());
    return true;
  }

private:
  // Dolphin's register file is a plain array, so an out-of-range index would read or write
  // whatever follows it in PowerPCState. The interface is not noexcept, and a hook that lets this
  // escape terminates the process -- which is the right outcome for a caller that has lost track of
  // which register it means, and a far better one than a silent write into the SPR file.
  [[nodiscard]] static std::size_t checked_register_index(std::size_t index) {
    constexpr std::size_t kGeneralRegisterCount = 32;
    if (index >= kGeneralRegisterCount) {
      throw std::out_of_range("guest general register index out of range");
    }
    return index;
  }

  Memory::MemoryManager &memory_;
  PowerPC::PowerPCState &state_;
};

} // namespace

JitRefusalReason to_gcnport(PowerPC::GcnPort::JitRefusalReason reason) noexcept {
  return static_cast<JitRefusalReason>(static_cast<std::uint8_t>(reason));
}

PowerPC::GcnPort::JitRefusalReason to_dolphin(JitRefusalReason reason) noexcept {
  return static_cast<PowerPC::GcnPort::JitRefusalReason>(static_cast<std::uint8_t>(reason));
}

// What the JIT ABI calls: the std::function the consumer installed, plus the system whose memory
// the guest context reads. Held by the adapter because Dolphin's hook is a plain function pointer
// and a context pointer, and neither can own a callable.
struct DolphinRuntimeAdapter::Binding {
  NativeHook hook;
  Core::System *system = nullptr;

  static PowerPC::GcnPort::HookResult Invoke(void *context, PowerPC::PowerPCState &state) noexcept {
    auto *const binding = static_cast<Binding *>(context);
    DolphinGuestContext guest(binding->system->GetMemory(), state);
    return to_dolphin(binding->hook(guest));
  }
};

DolphinRuntimeAdapter::DolphinRuntimeAdapter(Core::System &system,
                                             PowerPC::GcnPort::RuntimeSession &session)
    : system_(system), session_(session) {}

DolphinRuntimeAdapter::~DolphinRuntimeAdapter() {
  // The session dispatches through raw pointers into `bindings_`, so every installed hook has to go
  // before this object's storage does.
  for (const auto &[key, binding] : bindings_) {
    static_cast<void>(session_.RemoveNativeHook(to_dolphin(key)));
  }
}

JitStep DolphinRuntimeAdapter::execute_jit_block() {
  const PowerPC::GcnPort::JitBlockOutcome outcome = session_.ExecuteJitBlock();
  switch (outcome.kind) {
  case PowerPC::GcnPort::JitBlockKind::Compiled:
    return JitExecution{.guest_pc = outcome.guest_pc,
                        .instruction_count = outcome.instruction_count,
                        .compiled = true};
  case PowerPC::GcnPort::JitBlockKind::CacheHit:
    return JitExecution{.guest_pc = outcome.guest_pc,
                        .instruction_count = outcome.instruction_count,
                        .compiled = false};
  case PowerPC::GcnPort::JitBlockKind::Refused:
    return JitRefusal{.guest_pc = outcome.guest_pc, .reason = to_gcnport(outcome.refusal_reason)};
  case PowerPC::GcnPort::JitBlockKind::BackendFault:
    return BackendFault{.guest_pc = outcome.guest_pc, .detail = outcome.detail};
  }
  // A kind outside the enumeration means the two sides disagree about the contract, which is not
  // something to continue executing guest code through.
  return BackendFault{.guest_pc = outcome.guest_pc,
                      .detail = "Dolphin reported an unknown JitBlockKind"};
}

InterpretedBlock
DolphinRuntimeAdapter::execute_refused_block(const JitRefusal &refusal,
                                             std::uint32_t maximum_instruction_count) {
  const PowerPC::GcnPort::InterpretedBlockResult result =
      session_.ExecuteRefusedBlock(refusal.guest_pc, maximum_instruction_count);
  return InterpretedBlock{.guest_pc = result.guest_pc,
                          .instruction_count = result.instruction_count};
}

InterpretedBlock DolphinRuntimeAdapter::execute_diagnostic_interpreter_block(
    std::uint32_t maximum_instruction_count) {
  const PowerPC::GcnPort::InterpretedBlockResult result =
      session_.ExecuteDiagnosticInterpreterBlock(maximum_instruction_count);
  return InterpretedBlock{.guest_pc = result.guest_pc,
                          .instruction_count = result.instruction_count};
}

void DolphinRuntimeAdapter::invalidate_instruction(GuestAddress address) {
  constexpr std::uint32_t kInstructionBytes = 4;
  session_.InvalidateGuestCode(address, kInstructionBytes);
}

ExecutionIdentity DolphinRuntimeAdapter::identity() const {
  const PowerPC::GcnPort::ExecutionIdentity &session_identity = session_.GetExecutionIdentity();
  return ExecutionIdentity{.image = to_gcnport(session_identity.image),
                           .module_generation = session_identity.module_generation};
}

void DolphinRuntimeAdapter::install_hook(HookKey key, NativeHook hook) {
  auto binding = std::make_unique<Binding>(Binding{.hook = std::move(hook), .system = &system_});
  Binding *const installed = binding.get();
  // Point the session at the new storage before dropping any storage it was previously pointing
  // at. Mutations happen at a stopped CPU safe point, so nothing dispatches in between either way,
  // but this ordering does not depend on that being true.
  session_.InstallNativeHook(
      to_dolphin(key),
      PowerPC::GcnPort::NativeHookBinding{.context = installed, .function = &Binding::Invoke});
  bindings_.insert_or_assign(key, std::move(binding));
}

bool DolphinRuntimeAdapter::remove_hook(const HookKey &key) {
  // The session goes first: until it stops dispatching through this binding, the storage behind it
  // is still live state the JIT can reach.
  const bool removed = session_.RemoveNativeHook(to_dolphin(key));
  bindings_.erase(key);
  return removed;
}

std::size_t DolphinRuntimeAdapter::hook_count() const noexcept { return bindings_.size(); }

} // namespace gcnport
