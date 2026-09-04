// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace gcnport {

using GuestAddress = std::uint32_t;

enum class ExecutionMode {
  Jit,
  DiagnosticInterpreter,
};

enum class JitRefusalReason : std::uint8_t {
  UnsupportedInstruction,
  UnsafeInstructionFetch,
  UnsafeHostExecution,
  PrivilegedInstruction,
};

constexpr std::size_t kJitRefusalReasonCount = 4;

[[nodiscard]] constexpr std::size_t reason_index(JitRefusalReason reason) noexcept {
  return static_cast<std::size_t>(reason);
}

[[nodiscard]] const char *to_string(JitRefusalReason reason) noexcept;

struct FallbackBudget {
  std::uint64_t maximum_blocks = 0;
  std::uint64_t maximum_total_instructions = 0;
  std::uint32_t maximum_instructions_per_block = 0;
};

struct RuntimeConfiguration {
  ExecutionMode mode = ExecutionMode::Jit;
  FallbackBudget fallback{};
  std::uint32_t diagnostic_instructions_per_dispatch = 1;
};

struct JitExecution {
  GuestAddress guest_pc = 0;
  std::uint32_t instruction_count = 0;
  bool compiled = false;
};

struct JitRefusal {
  GuestAddress guest_pc = 0;
  JitRefusalReason reason = JitRefusalReason::UnsupportedInstruction;
};

struct BackendExit {
  std::string detail;
};

struct BackendFault {
  GuestAddress guest_pc = 0;
  std::string detail;
};

using JitStep = std::variant<JitExecution, JitRefusal, BackendExit, BackendFault>;

struct InterpretedBlock {
  GuestAddress guest_pc = 0;
  std::uint32_t instruction_count = 0;
};

struct ExecutionStatistics {
  std::uint64_t jit_dispatches = 0;
  std::uint64_t jit_blocks_compiled = 0;
  std::uint64_t jit_blocks_executed = 0;
  std::uint64_t jit_instructions_executed = 0;
  std::uint64_t fallback_blocks = 0;
  std::uint64_t fallback_instructions = 0;
  std::array<std::uint64_t, kJitRefusalReasonCount> fallback_blocks_by_reason{};
  std::array<std::uint64_t, kJitRefusalReasonCount> fallback_instructions_by_reason{};
  std::uint64_t diagnostic_interpreter_blocks = 0;
  std::uint64_t diagnostic_interpreter_instructions = 0;
};

enum class RunStopReason {
  BackendExit,
  BackendFault,
  DispatchBudgetExhausted,
  FallbackBudgetExceeded,
  InvalidBackendResult,
};

struct RunResult {
  RunStopReason reason = RunStopReason::BackendExit;
  GuestAddress guest_pc = 0;
  std::string detail;
  ExecutionStatistics statistics{};
};

} // namespace gcnport
