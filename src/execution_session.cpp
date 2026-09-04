// SPDX-License-Identifier: GPL-2.0-or-later
#include "gcnport/execution_session.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace gcnport {

ExecutionSession::ExecutionSession(RuntimeConfiguration configuration, RuntimeBackend &backend,
                                   DiagnosticsSink &diagnostics)
    : configuration_(configuration), backend_(backend), diagnostics_(diagnostics),
      fallback_ledger_(configuration.fallback) {
  if (configuration_.diagnostic_instructions_per_dispatch == 0) {
    throw std::invalid_argument("diagnostic interpreter dispatch limit must be nonzero");
  }
}

RunResult ExecutionSession::run(std::uint64_t maximum_dispatches) {
  for (std::uint64_t dispatch = 0; dispatch < maximum_dispatches; ++dispatch) {
    const auto result =
        configuration_.mode == ExecutionMode::Jit ? run_jit_dispatch() : run_diagnostic_dispatch();
    if (result.has_value()) {
      return *result;
    }
  }
  std::ostringstream detail;
  detail << "dispatch budget exhausted after " << maximum_dispatches << " dispatches";
  return stopped(RunStopReason::DispatchBudgetExhausted, 0, detail.str());
}

std::optional<RunResult> ExecutionSession::run_jit_dispatch() {
  ++statistics_.jit_dispatches;
  JitStep step = backend_.execute_jit_block();

  if (const auto *execution = std::get_if<JitExecution>(&step)) {
    if (execution->instruction_count == 0) {
      return stopped(RunStopReason::InvalidBackendResult, execution->guest_pc,
                     "JIT reported an executed block with zero instructions");
    }
    ++statistics_.jit_blocks_executed;
    statistics_.jit_instructions_executed += execution->instruction_count;
    if (execution->compiled) {
      ++statistics_.jit_blocks_compiled;
    }
    diagnostics_.on_jit_execution(*execution, statistics_);
    return std::nullopt;
  }

  if (const auto *refusal = std::get_if<JitRefusal>(&step)) {
    if (const auto error = fallback_ledger_.can_enter(*refusal, statistics_)) {
      return stopped(RunStopReason::FallbackBudgetExceeded, refusal->guest_pc, *error);
    }
    const InterpretedBlock execution =
        backend_.execute_refused_block(*refusal, fallback_ledger_.instruction_limit(statistics_));
    if (const auto error = fallback_ledger_.record(*refusal, execution, statistics_)) {
      return stopped(RunStopReason::FallbackBudgetExceeded, refusal->guest_pc, *error);
    }
    diagnostics_.on_fallback(*refusal, execution, statistics_);
    return std::nullopt;
  }

  if (auto *exit = std::get_if<BackendExit>(&step)) {
    return stopped(RunStopReason::BackendExit, 0, std::move(exit->detail));
  }

  auto &fault = std::get<BackendFault>(step);
  return stopped(RunStopReason::BackendFault, fault.guest_pc, std::move(fault.detail));
}

std::optional<RunResult> ExecutionSession::run_diagnostic_dispatch() {
  const InterpretedBlock execution = backend_.execute_diagnostic_interpreter_block(
      configuration_.diagnostic_instructions_per_dispatch);
  if (execution.instruction_count == 0 ||
      execution.instruction_count > configuration_.diagnostic_instructions_per_dispatch) {
    return stopped(RunStopReason::InvalidBackendResult, execution.guest_pc,
                   "diagnostic interpreter returned an invalid instruction count");
  }
  ++statistics_.diagnostic_interpreter_blocks;
  statistics_.diagnostic_interpreter_instructions += execution.instruction_count;
  diagnostics_.on_diagnostic_interpreter(execution, statistics_);
  return std::nullopt;
}

RunResult ExecutionSession::stopped(RunStopReason reason, GuestAddress guest_pc,
                                    std::string detail) const {
  return RunResult{
      .reason = reason,
      .guest_pc = guest_pc,
      .detail = std::move(detail),
      .statistics = statistics_,
  };
}

} // namespace gcnport
