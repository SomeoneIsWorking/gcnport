// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <optional>
#include <string>

#include "gcnport/execution_types.h"

namespace gcnport {

class FallbackLedger {
public:
  explicit FallbackLedger(FallbackBudget budget);

  [[nodiscard]] std::optional<std::string> can_enter(const JitRefusal &refusal,
                                                     const ExecutionStatistics &statistics) const;
  [[nodiscard]] std::uint32_t
  instruction_limit(const ExecutionStatistics &statistics) const noexcept;
  [[nodiscard]] std::optional<std::string> record(const JitRefusal &refusal,
                                                  const InterpretedBlock &execution,
                                                  ExecutionStatistics &statistics) const;

private:
  FallbackBudget budget_;
};

} // namespace gcnport
