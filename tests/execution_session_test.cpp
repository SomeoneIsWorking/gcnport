// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <utility>
#include <vector>

#include "gcnport/execution_session.h"
#include "test_support.h"

namespace {

using namespace gcnport;

class RecordingBackend final : public RuntimeBackend {
public:
  JitStep execute_jit_block() override {
    ++jit_calls;
    GCPORT_REQUIRE(!jit_steps.empty());
    JitStep result = std::move(jit_steps.front());
    jit_steps.pop_front();
    return result;
  }

  InterpretedBlock execute_refused_block(const JitRefusal &refusal,
                                         std::uint32_t maximum_instruction_count) override {
    ++fallback_calls;
    fallback_limits.push_back(maximum_instruction_count);
    return {.guest_pc = refusal.guest_pc, .instruction_count = fallback_instruction_count};
  }

  InterpretedBlock
  execute_diagnostic_interpreter_block(std::uint32_t maximum_instruction_count) override {
    ++diagnostic_calls;
    return {.guest_pc = diagnostic_pc,
            .instruction_count = std::min(diagnostic_instruction_count, maximum_instruction_count)};
  }

  std::deque<JitStep> jit_steps;
  std::vector<std::uint32_t> fallback_limits;
  std::uint32_t fallback_instruction_count = 1;
  std::uint32_t diagnostic_instruction_count = 1;
  GuestAddress diagnostic_pc = 0x8000'1000;
  std::uint64_t jit_calls = 0;
  std::uint64_t fallback_calls = 0;
  std::uint64_t diagnostic_calls = 0;
};

class RecordingDiagnostics final : public DiagnosticsSink {
public:
  void on_jit_execution(const JitExecution &execution,
                        const ExecutionStatistics &statistics) override {
    jit.push_back(execution);
    snapshots.push_back(statistics);
  }
  void on_fallback(const JitRefusal &refusal, const InterpretedBlock &execution,
                   const ExecutionStatistics &statistics) override {
    refusals.push_back(refusal);
    fallbacks.push_back(execution);
    snapshots.push_back(statistics);
  }
  void on_diagnostic_interpreter(const InterpretedBlock &execution,
                                 const ExecutionStatistics &statistics) override {
    diagnostics.push_back(execution);
    snapshots.push_back(statistics);
  }

  std::vector<JitExecution> jit;
  std::vector<JitRefusal> refusals;
  std::vector<InterpretedBlock> fallbacks;
  std::vector<InterpretedBlock> diagnostics;
  std::vector<ExecutionStatistics> snapshots;
};

void default_mode_compiles_before_execution() {
  RuntimeConfiguration configuration;
  GCPORT_REQUIRE(configuration.mode == ExecutionMode::Jit);

  RecordingBackend backend;
  backend.jit_steps.emplace_back(
      JitExecution{.guest_pc = 0x8000'1000, .instruction_count = 3, .compiled = true});
  backend.jit_steps.emplace_back(BackendExit{.detail = "bounded test exit"});
  RecordingDiagnostics diagnostics;

  ExecutionSession session(configuration, backend, diagnostics);
  const RunResult result = session.run(2);

  GCPORT_REQUIRE(result.reason == RunStopReason::BackendExit);
  GCPORT_REQUIRE(result.statistics.jit_dispatches == 2);
  GCPORT_REQUIRE(result.statistics.jit_blocks_compiled == 1);
  GCPORT_REQUIRE(result.statistics.jit_blocks_executed == 1);
  GCPORT_REQUIRE(result.statistics.jit_instructions_executed == 3);
  GCPORT_REQUIRE(result.statistics.fallback_blocks == 0);
  GCPORT_REQUIRE(backend.fallback_calls == 0);
  GCPORT_REQUIRE(backend.diagnostic_calls == 0);
  GCPORT_REQUIRE(diagnostics.jit.size() == 1);
}

void refused_block_is_bounded_then_returns_to_jit() {
  RuntimeConfiguration configuration{
      .mode = ExecutionMode::Jit,
      .fallback = {.maximum_blocks = 2,
                   .maximum_total_instructions = 5,
                   .maximum_instructions_per_block = 3},
  };
  RecordingBackend backend;
  backend.fallback_instruction_count = 2;
  backend.jit_steps.emplace_back(
      JitRefusal{.guest_pc = 0x8000'2000, .reason = JitRefusalReason::UnsupportedInstruction});
  backend.jit_steps.emplace_back(
      JitExecution{.guest_pc = 0x8000'2008, .instruction_count = 4, .compiled = true});
  backend.jit_steps.emplace_back(BackendExit{.detail = "complete"});
  RecordingDiagnostics diagnostics;

  ExecutionSession session(configuration, backend, diagnostics);
  const RunResult result = session.run(3);

  GCPORT_REQUIRE(result.reason == RunStopReason::BackendExit);
  GCPORT_REQUIRE(backend.jit_calls == 3);
  GCPORT_REQUIRE(backend.fallback_calls == 1);
  GCPORT_REQUIRE(backend.fallback_limits == std::vector<std::uint32_t>{3});
  GCPORT_REQUIRE(result.statistics.jit_blocks_executed == 1);
  GCPORT_REQUIRE(result.statistics.fallback_blocks == 1);
  GCPORT_REQUIRE(result.statistics.fallback_instructions == 2);
  GCPORT_REQUIRE(result.statistics.fallback_blocks_by_reason.at(
                     reason_index(JitRefusalReason::UnsupportedInstruction)) == 1);
  GCPORT_REQUIRE(diagnostics.fallbacks.size() == 1);
  GCPORT_REQUIRE(diagnostics.snapshots.front().fallback_blocks == 1);
  GCPORT_REQUIRE(diagnostics.snapshots.front().jit_blocks_executed == 0);
}

void backend_fault_never_enters_interpreter() {
  RecordingBackend backend;
  backend.jit_steps.emplace_back(
      BackendFault{.guest_pc = 0x8000'3000, .detail = "host code publication failed"});
  RecordingDiagnostics diagnostics;
  ExecutionSession session(RuntimeConfiguration{}, backend, diagnostics);

  const RunResult result = session.run(1);

  GCPORT_REQUIRE(result.reason == RunStopReason::BackendFault);
  GCPORT_REQUIRE(result.guest_pc == 0x8000'3000);
  GCPORT_REQUIRE(backend.fallback_calls == 0);
  GCPORT_REQUIRE(result.statistics.fallback_blocks == 0);
}

void default_fallback_budget_is_disabled() {
  RecordingBackend backend;
  backend.jit_steps.emplace_back(
      JitRefusal{.guest_pc = 0x8000'3500, .reason = JitRefusalReason::UnsupportedInstruction});
  RecordingDiagnostics diagnostics;
  ExecutionSession session(RuntimeConfiguration{}, backend, diagnostics);

  const RunResult result = session.run(1);

  GCPORT_REQUIRE(result.reason == RunStopReason::FallbackBudgetExceeded);
  GCPORT_REQUIRE(backend.fallback_calls == 0);
}

void partially_configured_fallback_budget_is_rejected() {
  RecordingBackend backend;
  RecordingDiagnostics diagnostics;
  bool rejected = false;
  try {
    ExecutionSession session(
        RuntimeConfiguration{
            .fallback = {.maximum_blocks = 1, .maximum_total_instructions = 1},
        },
        backend, diagnostics);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  GCPORT_REQUIRE(rejected);
}

void fallback_budget_fails_closed() {
  RuntimeConfiguration configuration{
      .mode = ExecutionMode::Jit,
      .fallback = {.maximum_blocks = 1,
                   .maximum_total_instructions = 1,
                   .maximum_instructions_per_block = 1},
  };
  RecordingBackend backend;
  backend.fallback_instruction_count = 1;
  backend.jit_steps.emplace_back(
      JitRefusal{.guest_pc = 0x8000'4000, .reason = JitRefusalReason::UnsafeInstructionFetch});
  backend.jit_steps.emplace_back(
      JitRefusal{.guest_pc = 0x8000'4004, .reason = JitRefusalReason::UnsafeInstructionFetch});
  RecordingDiagnostics diagnostics;
  ExecutionSession session(configuration, backend, diagnostics);

  const RunResult result = session.run(2);

  GCPORT_REQUIRE(result.reason == RunStopReason::FallbackBudgetExceeded);
  GCPORT_REQUIRE(result.guest_pc == 0x8000'4004);
  GCPORT_REQUIRE(result.statistics.fallback_blocks == 1);
  GCPORT_REQUIRE(result.statistics.fallback_instructions == 1);
  GCPORT_REQUIRE(backend.fallback_calls == 1);
}

void diagnostic_mode_is_explicit_and_separate() {
  RuntimeConfiguration configuration{
      .mode = ExecutionMode::DiagnosticInterpreter,
      .diagnostic_instructions_per_dispatch = 2,
  };
  RecordingBackend backend;
  backend.diagnostic_instruction_count = 2;
  RecordingDiagnostics diagnostics;
  ExecutionSession session(configuration, backend, diagnostics);

  const RunResult result = session.run(3);

  GCPORT_REQUIRE(result.reason == RunStopReason::DispatchBudgetExhausted);
  GCPORT_REQUIRE(backend.jit_calls == 0);
  GCPORT_REQUIRE(backend.fallback_calls == 0);
  GCPORT_REQUIRE(backend.diagnostic_calls == 3);
  GCPORT_REQUIRE(result.statistics.jit_dispatches == 0);
  GCPORT_REQUIRE(result.statistics.diagnostic_interpreter_blocks == 3);
  GCPORT_REQUIRE(result.statistics.diagnostic_interpreter_instructions == 6);
  GCPORT_REQUIRE(diagnostics.diagnostics.size() == 3);
}

} // namespace

int main() {
  default_mode_compiles_before_execution();
  refused_block_is_bounded_then_returns_to_jit();
  backend_fault_never_enters_interpreter();
  default_fallback_budget_is_disabled();
  partially_configured_fallback_budget_is_rejected();
  fallback_budget_fails_closed();
  diagnostic_mode_is_explicit_and_separate();
  return 0;
}
