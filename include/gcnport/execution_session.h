// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <optional>

#include "gcnport/diagnostics.h"
#include "gcnport/fallback_ledger.h"
#include "gcnport/runtime_backend.h"

namespace gcnport {

class ExecutionSession {
public:
  ExecutionSession(RuntimeConfiguration configuration, RuntimeBackend &backend,
                   DiagnosticsSink &diagnostics);

  [[nodiscard]] RunResult run(std::uint64_t maximum_dispatches);
  [[nodiscard]] const ExecutionStatistics &statistics() const noexcept { return statistics_; }

private:
  [[nodiscard]] std::optional<RunResult> run_jit_dispatch();
  [[nodiscard]] std::optional<RunResult> run_diagnostic_dispatch();
  [[nodiscard]] RunResult stopped(RunStopReason reason, GuestAddress guest_pc,
                                  std::string detail) const;

  RuntimeConfiguration configuration_;
  RuntimeBackend &backend_;
  DiagnosticsSink &diagnostics_;
  FallbackLedger fallback_ledger_;
  ExecutionStatistics statistics_{};
};

} // namespace gcnport
