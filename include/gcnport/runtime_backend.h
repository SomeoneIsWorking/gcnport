// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "gcnport/execution_types.h"

namespace gcnport {

// A backend implementation must execute exactly one guest basic block per call. The production
// implementation is Dolphin's JIT; an absent or failed JIT is a BackendFault, never a refusal.
class RuntimeBackend {
public:
  virtual ~RuntimeBackend() = default;

  [[nodiscard]] virtual JitStep execute_jit_block() = 0;
  [[nodiscard]] virtual InterpretedBlock
  execute_refused_block(const JitRefusal &refusal, std::uint32_t maximum_instruction_count) = 0;
  [[nodiscard]] virtual InterpretedBlock
  execute_diagnostic_interpreter_block(std::uint32_t maximum_instruction_count) = 0;
};

} // namespace gcnport
