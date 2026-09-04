// SPDX-License-Identifier: GPL-2.0-or-later
#include "gcnport/fallback_ledger.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace gcnport {

namespace {

std::string pc_message(const char *prefix, GuestAddress pc) {
  std::ostringstream message;
  message << prefix << " 0x" << std::hex << pc;
  return message.str();
}

} // namespace

FallbackLedger::FallbackLedger(FallbackBudget budget) : budget_(budget) {
  const bool disabled = budget_.maximum_blocks == 0 && budget_.maximum_total_instructions == 0 &&
                        budget_.maximum_instructions_per_block == 0;
  const bool fully_configured = budget_.maximum_blocks != 0 &&
                                budget_.maximum_total_instructions != 0 &&
                                budget_.maximum_instructions_per_block != 0;
  if (!disabled && !fully_configured) {
    throw std::invalid_argument("fallback limits must be all zero or all nonzero");
  }
}

std::optional<std::string> FallbackLedger::can_enter(const JitRefusal &refusal,
                                                     const ExecutionStatistics &statistics) const {
  if (reason_index(refusal.reason) >= kJitRefusalReasonCount) {
    return pc_message("JIT returned an invalid refusal reason at", refusal.guest_pc);
  }
  if (statistics.fallback_blocks >= budget_.maximum_blocks) {
    std::ostringstream message;
    message << "fallback block budget exhausted before 0x" << std::hex << refusal.guest_pc
            << std::dec << ": " << statistics.fallback_blocks << " >= " << budget_.maximum_blocks;
    return message.str();
  }
  if (statistics.fallback_instructions >= budget_.maximum_total_instructions) {
    std::ostringstream message;
    message << "fallback instruction budget exhausted before 0x" << std::hex << refusal.guest_pc
            << std::dec << ": " << statistics.fallback_instructions
            << " >= " << budget_.maximum_total_instructions;
    return message.str();
  }
  return std::nullopt;
}

std::uint32_t
FallbackLedger::instruction_limit(const ExecutionStatistics &statistics) const noexcept {
  if (statistics.fallback_instructions >= budget_.maximum_total_instructions) {
    return 0;
  }
  const std::uint64_t remaining =
      budget_.maximum_total_instructions - statistics.fallback_instructions;
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(budget_.maximum_instructions_per_block, remaining));
}

std::optional<std::string> FallbackLedger::record(const JitRefusal &refusal,
                                                  const InterpretedBlock &execution,
                                                  ExecutionStatistics &statistics) const {
  if (reason_index(refusal.reason) >= kJitRefusalReasonCount) {
    return pc_message("JIT returned an invalid refusal reason at", refusal.guest_pc);
  }
  if (execution.guest_pc != refusal.guest_pc) {
    std::ostringstream message;
    message << "fallback PC mismatch: refusal=0x" << std::hex << refusal.guest_pc
            << ", execution=0x" << execution.guest_pc;
    return message.str();
  }
  if (execution.instruction_count == 0) {
    return pc_message("fallback executed no instructions at", refusal.guest_pc);
  }
  if (execution.instruction_count > budget_.maximum_instructions_per_block) {
    std::ostringstream message;
    message << "fallback at 0x" << std::hex << refusal.guest_pc << std::dec
            << " exceeded per-block limit: " << execution.instruction_count << " > "
            << budget_.maximum_instructions_per_block;
    return message.str();
  }
  if (statistics.fallback_instructions + execution.instruction_count >
      budget_.maximum_total_instructions) {
    std::ostringstream message;
    message << "fallback instruction budget exhausted at 0x" << std::hex << refusal.guest_pc
            << std::dec << ": " << statistics.fallback_instructions << " + "
            << execution.instruction_count << " > " << budget_.maximum_total_instructions;
    return message.str();
  }

  ++statistics.fallback_blocks;
  statistics.fallback_instructions += execution.instruction_count;
  ++statistics.fallback_blocks_by_reason.at(reason_index(refusal.reason));
  statistics.fallback_instructions_by_reason.at(reason_index(refusal.reason)) +=
      execution.instruction_count;
  return std::nullopt;
}

} // namespace gcnport
