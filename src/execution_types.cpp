// SPDX-License-Identifier: GPL-2.0-or-later
#include "gcnport/execution_types.h"

namespace gcnport {

const char *to_string(JitRefusalReason reason) noexcept {
  switch (reason) {
  case JitRefusalReason::UnsupportedInstruction:
    return "unsupported_instruction";
  case JitRefusalReason::UnsafeInstructionFetch:
    return "unsafe_instruction_fetch";
  case JitRefusalReason::UnsafeHostExecution:
    return "unsafe_host_execution";
  case JitRefusalReason::PrivilegedInstruction:
    return "privileged_instruction";
  }
  return "invalid_refusal_reason";
}

} // namespace gcnport
