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
constexpr std::uint32_t ADDI_R3_R3_1 = 0x38630001;
constexpr std::uint32_t BRANCH_BACK_ONE_INSTRUCTION = 0x4bfffffc;
constexpr std::uint32_t BRANCH_TO_SELF = 0x48000000;

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

  HookResult operator()(GuestContext &guest) {
    ++calls;
    observed_pc = guest.program_counter();
    read_succeeded = guest.read_memory(HOOK_ADDRESS, observed_instruction);
    // A read of an address no memory region backs must answer false rather than a zeroed buffer
    // that reads like a legitimate window of guest RAM.
    std::array<std::byte, sizeof(std::uint32_t)> discarded{};
    refused_unmapped_read = !guest.read_memory(0x0f000000, discarded);
    guest.set_general_register(3, 7);
    return HookResult::continue_at(LANDING_ADDRESS);
  }
};

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
