// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "gcnport/execution_types.h"

namespace gcnport {

// gcnport emits typed events instead of writing to a process stream. Product consumers adapt this
// boundary to their configured Lucent logger and metrics sink.
class DiagnosticsSink {
public:
  virtual ~DiagnosticsSink() = default;

  virtual void on_jit_execution(const JitExecution &execution,
                                const ExecutionStatistics &statistics) = 0;
  virtual void on_fallback(const JitRefusal &refusal, const InterpretedBlock &execution,
                           const ExecutionStatistics &statistics) = 0;
  virtual void on_diagnostic_interpreter(const InterpretedBlock &execution,
                                         const ExecutionStatistics &statistics) = 0;
};

} // namespace gcnport
