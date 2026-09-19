// SPDX-License-Identifier: GPL-2.0-or-later
//
// The adapter is the only place gcnport's vocabulary and Dolphin's meet, so it is the only place
// a divergence between them can be caught. Everything here drives the shipping adapter: there is
// no second implementation of a conversion to agree with itself.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <variant>

#include "gcnport/dolphin_adapter.h"
#include "test_support.h"

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/GcnPortRuntime.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "UICommon/UICommon.h"

namespace {

using namespace gcnport;

constexpr GuestAddress ENTRY_ADDRESS = 0x80001000;
constexpr GuestAddress HOOK_ADDRESS = 0x80002000;
constexpr GuestAddress LANDING_ADDRESS = 0x80003000;
constexpr GuestAddress ORIGINAL_ADDRESS = 0x80004000;
constexpr std::uint32_t ADDI_R3_R3_1 = 0x38630001;
constexpr std::uint32_t BRANCH_BACK_ONE_INSTRUCTION = 0x4bfffffc;
constexpr std::uint32_t BRANCH_TO_SELF = 0x48000000;
constexpr std::uint32_t BLR = 0x4e800020;

// The first floating argument register, one the caller leaves alone, and the one the hook writes.
constexpr std::size_t FLOAT_ARGUMENT_REGISTER = 1;
constexpr std::size_t UNTOUCHED_FLOAT_REGISTER = 5;
constexpr std::size_t FLOAT_RESULT_REGISTER = 2;
// Neither value is representable as a single, which is the point: the register holds a double
// whatever the callee declared, and an accessor that narrowed through a float would answer a
// different number rather than failing outright.
constexpr double GUEST_LOADED_FLOAT = 1.0 / 3.0;
constexpr double HOOK_WRITTEN_FLOAT = -2.0 / 7.0;

// Relative to the branch instruction itself, which sits one word into the hooked block -- not to
// the block's start. Getting that wrong lands one word past the landing pad, in zero-filled memory,
// and the symptom is the interpreter walking forward through "Unknown instruction 00000000".
constexpr std::uint32_t HOOK_BRANCH_DISPLACEMENT =
    LANDING_ADDRESS - (HOOK_ADDRESS + sizeof(std::uint32_t));
constexpr std::uint32_t BRANCH_TO_LANDING = 0x48000000 | (HOOK_BRANCH_DISPLACEMENT & 0x03fffffcu);
static_assert((HOOK_BRANCH_DISPLACEMENT & ~0x03fffffcu) == 0,
              "landing displacement must fit the B-form 24-bit field");

// gcnport's JitRefusalReason is a hand-kept mirror of Dolphin's, because gcnport's public headers
// stay free of Dolphin's. A mirror maintained by a comment is one rename away from silently
// relabelling every fallback a consumer reports, so walk every enumerator through the adapter's
// conversions in both directions and make the two owners' own names agree. A reordering shows up
// here as two different strings, which is the only symptom it would otherwise ever produce.
void CheckRefusalReasonMirror() {
  constexpr std::array kReasons = {
      PowerPC::GcnPort::JitRefusalReason::UnsupportedInstruction,
      PowerPC::GcnPort::JitRefusalReason::UnsafeInstructionFetch,
      PowerPC::GcnPort::JitRefusalReason::UnsafeHostExecution,
      PowerPC::GcnPort::JitRefusalReason::PrivilegedInstruction,
  };
  GCPORT_REQUIRE(kReasons.size() == kJitRefusalReasonCount);
  GCPORT_REQUIRE(PowerPC::GcnPort::kJitRefusalReasonCount == kJitRefusalReasonCount);

  for (const PowerPC::GcnPort::JitRefusalReason dolphin_reason : kReasons) {
    const JitRefusalReason mirrored = to_gcnport(dolphin_reason);
    GCPORT_REQUIRE(to_dolphin(mirrored) == dolphin_reason);
    GCPORT_REQUIRE(std::string(to_string(mirrored)) ==
                   std::string(PowerPC::GcnPort::ToString(dolphin_reason)));
  }
}

// Dolphin's default handler prompts on stdin, which in an automated test is not a prompt but a
// hang. Answer every alert without prompting and count them, so the one alert this test expects --
// Dolphin refusing the deliberately unmapped guest read below -- is an assertion rather than noise,
// and any other alert fails the test instead of scrolling past.
std::uint32_t g_alerts = 0;

bool AnswerAlertWithoutPrompting(const char *caption, const char *text, bool /*yes_no*/,
                                 Common::MsgType /*style*/) {
  ++g_alerts;
  std::cerr << "dolphin_adapter: dolphin alert [" << caption << "] " << text << '\n';
  return true;
}

// Counts its invocations and reports what it saw through the GuestContext, so a passing run cannot
// come from a hook that fired against stale or empty state.
struct RecordingHook {
  std::uint32_t calls = 0;
  GuestAddress observed_pc = 0;
  std::array<std::byte, sizeof(std::uint32_t)> observed_instruction{};
  bool read_succeeded = false;
  bool refused_unmapped_read = false;
  double observed_float = 0.0;
  double observed_untouched_float = 1.0;

  HookResult operator()(GuestContext &guest) {
    ++calls;
    observed_pc = guest.program_counter();
    read_succeeded = guest.read_memory(HOOK_ADDRESS, observed_instruction);
    // A read of an address no memory region backs must answer false rather than a zeroed buffer
    // that reads like a legitimate window of guest RAM.
    std::array<std::byte, sizeof(std::uint32_t)> discarded{};
    refused_unmapped_read = !guest.read_memory(0x0f000000, discarded);
    guest.set_general_register(3, 7);
    // The floating file, which the ABI puts every float argument in and the general file never
    // carries. Both halves are recorded: the register the caller loaded, and one the caller left
    // alone -- an implementation reading the wrong file or the wrong index would answer the same
    // thing for both, and reading only the loaded one could not tell.
    observed_float = guest.floating_register(FLOAT_ARGUMENT_REGISTER);
    observed_untouched_float = guest.floating_register(UNTOUCHED_FLOAT_REGISTER);
    guest.set_floating_register(FLOAT_RESULT_REGISTER, HOOK_WRITTEN_FLOAT);
    return HookResult::continue_at(LANDING_ADDRESS);
  }
};

// The "superCall": native work, then the real guest body as a subroutine, then more native work,
// with the hook still choosing the outcome. Every step records what it saw, so the assertions can
// tell a body that ran from one that was skipped and a native write that landed from one that the
// body overwrote.
struct SuperCallHook {
  std::uint32_t calls = 0;
  std::uint32_t before_original = 0;
  std::uint32_t after_original = 0;
  InterpretedBlock original{};

  HookResult operator()(GuestContext &guest) {
    ++calls;
    before_original = guest.general_register(3);
    constexpr std::uint32_t kInstructionBudget = 8;
    original = guest.call_original(kInstructionBudget);
    after_original = guest.general_register(3);
    // A value the guest body could not have produced by itself, written after it ran: this is what
    // separates "the original executed and the hook kept control" from either one alone.
    guest.set_general_register(3, after_original * 10);
    return HookResult::return_to_caller();
  }
};

// Both halves of the original-call surface, against the shipping session rather than a stand-in:
// call_original runs the body inside the callback, arm_original_call hands it a dispatch of its
// own.
void CheckOriginalCallSurface(DolphinRuntimeAdapter &adapter,
                              PowerPC::GcnPort::RuntimeSession &session,
                              const ExecutionIdentity &identity, PowerPC::PowerPCState &state) {
  const HookKey key{.identity = identity, .address = ORIGINAL_ADDRESS};

  // Arming a ticket at an address with no installed hook must fail: there is nothing to suppress,
  // and answering true would leave the caller believing the next entry was already accounted for.
  GCPORT_REQUIRE(!adapter.arm_original_call(key));

  SuperCallHook super;
  adapter.install_hook(key, std::ref(super));
  GCPORT_REQUIRE(adapter.hook_count() == 1);

  // A key that names another module generation is not this key, however similar it looks.
  ExecutionIdentity stale = identity;
  stale.module_generation += 1;
  GCPORT_REQUIRE(
      !adapter.arm_original_call(HookKey{.identity = stale, .address = ORIGINAL_ADDRESS}));

  constexpr std::uint32_t kSeed = 5;
  state.gpr[3] = kSeed;
  state.spr[SPR_LR] = LANDING_ADDRESS;
  state.pc = ORIGINAL_ADDRESS;
  state.npc = ORIGINAL_ADDRESS;
  const JitStep called = adapter.execute_jit_block();
  GCPORT_REQUIRE(!std::holds_alternative<BackendFault>(called));

  GCPORT_REQUIRE(super.calls == 1);
  GCPORT_REQUIRE(super.before_original == kSeed);
  // The body really executed: two instructions from its own address, and r3 moved by exactly the
  // one increment it contains.
  GCPORT_REQUIRE(super.original.guest_pc == ORIGINAL_ADDRESS);
  GCPORT_REQUIRE(super.original.instruction_count == 2);
  GCPORT_REQUIRE(super.after_original == kSeed + 1);
  // And the hook still owned the outcome afterwards, in both the register file and the PC.
  GCPORT_REQUIRE(state.gpr[3] == (kSeed + 1) * 10);
  GCPORT_REQUIRE(state.pc == LANDING_ADDRESS);
  GCPORT_REQUIRE(session.GetExecutionCounters().synchronous_original_calls == 1);
  GCPORT_REQUIRE(session.GetExecutionCounters().synchronous_original_instructions == 2);

  // The ticket path: the body runs under the dispatcher and the callback is never entered at all,
  // which is what tells it apart from the call_original path above.
  GCPORT_REQUIRE(adapter.arm_original_call(key));
  state.gpr[3] = kSeed;
  state.spr[SPR_LR] = LANDING_ADDRESS;
  state.pc = ORIGINAL_ADDRESS;
  state.npc = ORIGINAL_ADDRESS;
  const JitStep ticketed = adapter.execute_jit_block();
  GCPORT_REQUIRE(!std::holds_alternative<BackendFault>(ticketed));
  GCPORT_REQUIRE(super.calls == 1);
  GCPORT_REQUIRE(state.gpr[3] == kSeed + 1);

  // One shot only. The next entry finds the hook again, so r3 ends on the hook's value, not the
  // body's -- a ticket that survived would show kSeed + 1 here.
  state.gpr[3] = kSeed;
  state.spr[SPR_LR] = LANDING_ADDRESS;
  state.pc = ORIGINAL_ADDRESS;
  state.npc = ORIGINAL_ADDRESS;
  const JitStep hooked_again = adapter.execute_jit_block();
  GCPORT_REQUIRE(!std::holds_alternative<BackendFault>(hooked_again));
  GCPORT_REQUIRE(super.calls == 2);
  GCPORT_REQUIRE(state.gpr[3] == (kSeed + 1) * 10);

  GCPORT_REQUIRE(adapter.remove_hook(key));
}

void RunAdapterScenario() {
  const std::string profile_path = File::CreateTempDir();
  GCPORT_REQUIRE(!profile_path.empty());

  static_cast<void>(Common::RegisterMsgAlertHandler(&AnswerAlertWithoutPrompting));
  Core::DeclareAsCPUThread();
  UICommon::SetUserDirectory(profile_path);
  Config::Init();
  SConfig::Init();

  Core::System &system = Core::System::GetInstance();
  system.GetMemory().Init();
  system.GetCoreTiming().Init();
  system.GetCPU().Init(PowerPC::DefaultCPUCore());

  Memory::MemoryManager &memory = system.GetMemory();
  memory.Write_U32(ADDI_R3_R3_1, ENTRY_ADDRESS);
  memory.Write_U32(BRANCH_BACK_ONE_INSTRUCTION, ENTRY_ADDRESS + sizeof(std::uint32_t));
  memory.Write_U32(ADDI_R3_R3_1, HOOK_ADDRESS);
  memory.Write_U32(BRANCH_TO_LANDING, HOOK_ADDRESS + sizeof(std::uint32_t));
  memory.Write_U32(BRANCH_TO_SELF, LANDING_ADDRESS);
  // An ordinary callable body: it increments r3 and returns through the link register, which is
  // what CallOriginalSynchronously watches for to know the call finished.
  memory.Write_U32(ADDI_R3_R3_1, ORIGINAL_ADDRESS);
  memory.Write_U32(BLR, ORIGINAL_ADDRESS + sizeof(std::uint32_t));

  PowerPC::GcnPort::ExecutionIdentity dolphin_identity;
  dolphin_identity.image.sha256.front() = 0x5b;
  dolphin_identity.module_generation = 1;

  auto &state = system.GetPPCState();
  {
    PowerPC::GcnPort::RuntimeSession session(system, dolphin_identity);
    DolphinRuntimeAdapter adapter(system, session);

    // The identity crosses the boundary intact, so a consumer can build a HookKey from
    // `adapter.identity()` alone and still be refused at any other image.
    const ExecutionIdentity identity = adapter.identity();
    GCPORT_REQUIRE(identity.image.sha256.front() == 0x5b);
    GCPORT_REQUIRE(identity.image.is_authenticated());
    GCPORT_REQUIRE(identity.module_generation == 1);

    state.gpr[3] = 0;
    state.pc = ENTRY_ADDRESS;
    state.npc = ENTRY_ADDRESS;

    const JitStep first = adapter.execute_jit_block();
    const auto *const first_execution = std::get_if<JitExecution>(&first);
    GCPORT_REQUIRE(first_execution != nullptr);
    GCPORT_REQUIRE(first_execution->guest_pc == ENTRY_ADDRESS);
    GCPORT_REQUIRE(first_execution->compiled);
    GCPORT_REQUIRE(first_execution->instruction_count > 0);

    // The second kind the backend reports is a cache hit. Dolphin's analyzer may merge several
    // passes of a tight loop into one compiled unit, so allow a few dispatches rather than
    // asserting it lands on the very next one.
    constexpr int kCacheHitAttempts = 8;
    bool saw_cache_hit = false;
    for (int attempt = 0; attempt < kCacheHitAttempts && !saw_cache_hit; ++attempt) {
      const JitStep step = adapter.execute_jit_block();
      const auto *const execution = std::get_if<JitExecution>(&step);
      GCPORT_REQUIRE(execution != nullptr);
      saw_cache_hit = !execution->compiled;
    }
    GCPORT_REQUIRE(saw_cache_hit);
    GCPORT_REQUIRE(state.gpr[3] > 0);

    RecordingHook hook;
    const HookKey key{.identity = identity, .address = HOOK_ADDRESS};
    GCPORT_REQUIRE(key.is_valid());
    adapter.install_hook(key, std::ref(hook));
    GCPORT_REQUIRE(adapter.hook_count() == 1);

    state.gpr[3] = 0;
    state.pc = HOOK_ADDRESS;
    state.npc = HOOK_ADDRESS;
    state.ps[FLOAT_ARGUMENT_REGISTER].SetPS0(GUEST_LOADED_FLOAT);
    state.ps[UNTOUCHED_FLOAT_REGISTER].SetPS0(0.0);
    state.ps[FLOAT_RESULT_REGISTER].SetPS0(0.0);
    const JitStep hooked = adapter.execute_jit_block();
    GCPORT_REQUIRE(!std::holds_alternative<BackendFault>(hooked));

    GCPORT_REQUIRE(hook.calls == 1);
    GCPORT_REQUIRE(hook.observed_pc == HOOK_ADDRESS);
    GCPORT_REQUIRE(hook.read_succeeded);
    GCPORT_REQUIRE(hook.refused_unmapped_read);
    // Exactly the one alert the unmapped read provoked, and no other.
    GCPORT_REQUIRE(g_alerts == 1);
    std::array<std::byte, sizeof(std::uint32_t)> expected_instruction{};
    const std::uint32_t big_endian_instruction = Common::swap32(ADDI_R3_R3_1);
    std::memcpy(expected_instruction.data(), &big_endian_instruction, expected_instruction.size());
    GCPORT_REQUIRE(hook.observed_instruction == expected_instruction);

    // The hook wrote r3 and asked for a continuation, and both had to survive the conversion back
    // into Dolphin's own result type. The guest body at HOOK_ADDRESS would have incremented r3,
    // so 7 exactly also proves the original body did not run.
    GCPORT_REQUIRE(state.gpr[3] == 7);
    GCPORT_REQUIRE(state.pc == LANDING_ADDRESS);

    // The float the caller loaded reached the hook exactly, and the register it left alone did not
    // answer the same thing. The value the hook wrote reached the guest's own register file, which
    // is what makes this the register a guest function would go on to read rather than a copy.
    GCPORT_REQUIRE(hook.observed_float == GUEST_LOADED_FLOAT);
    GCPORT_REQUIRE(hook.observed_untouched_float == 0.0);
    GCPORT_REQUIRE(state.ps[FLOAT_RESULT_REGISTER].PS0AsDouble() == HOOK_WRITTEN_FLOAT);
    GCPORT_REQUIRE(state.ps[FLOAT_ARGUMENT_REGISTER].PS0AsDouble() == GUEST_LOADED_FLOAT);

    GCPORT_REQUIRE(adapter.remove_hook(key));
    GCPORT_REQUIRE(adapter.hook_count() == 0);
    GCPORT_REQUIRE(!adapter.remove_hook(key));

    // With the hook gone the guest body runs again, which is the negative half of the same proof:
    // the count stays where it was and r3 moves.
    state.gpr[3] = 0;
    state.pc = HOOK_ADDRESS;
    state.npc = HOOK_ADDRESS;
    const JitStep unhooked = adapter.execute_jit_block();
    GCPORT_REQUIRE(!std::holds_alternative<BackendFault>(unhooked));
    GCPORT_REQUIRE(hook.calls == 1);
    GCPORT_REQUIRE(state.gpr[3] == 1);

    CheckOriginalCallSurface(adapter, session, identity, state);
  }

  system.GetCPU().Shutdown();
  system.GetCoreTiming().Shutdown();
  system.GetMemory().Shutdown();
  SConfig::Shutdown();
  Config::Shutdown();
  File::DeleteDirRecursively(profile_path);
}

} // namespace

int main() {
  return gcnport::test::run_test_main("dolphin_adapter", [] {
    CheckRefusalReasonMirror();
    // Dolphin's BLR-return optimization installs a guard in the calling thread's own stack, so
    // guest code runs on a dedicated thread with a fully mapped one, matching the convention in
    // the fork's own GcnPortRuntimeTest.cpp.
    std::thread cpu_thread(RunAdapterScenario);
    cpu_thread.join();
  });
}
