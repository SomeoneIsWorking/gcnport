// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "gcnport/execution_types.h"

namespace gcnport {

class GuestContext {
public:
  virtual ~GuestContext() = default;

  [[nodiscard]] virtual GuestAddress program_counter() const noexcept = 0;
  virtual void set_program_counter(GuestAddress value) noexcept = 0;
  [[nodiscard]] virtual GuestAddress link_register() const noexcept = 0;
  virtual void set_link_register(GuestAddress value) noexcept = 0;
  [[nodiscard]] virtual std::uint32_t general_register(std::size_t index) const = 0;
  virtual void set_general_register(std::size_t index, std::uint32_t value) = 0;

  [[nodiscard]] virtual bool read_memory(GuestAddress address,
                                         std::span<std::byte> destination) = 0;
  [[nodiscard]] virtual bool write_memory(GuestAddress address,
                                          std::span<const std::byte> source) = 0;

  // Runs the original guest body this hook is standing in for, as an ordinary synchronous
  // subroutine call, and returns here. This is the "superCall" a native override needs: run native
  // code, call through to the real function, then run more native code and still decide the
  // HookResult. It is deliberately a method on the context rather than a free operation taking a
  // key, because the context already knows which hook is dispatching and a hook that had to restate
  // its own key could state a different one.
  //
  // `maximum_instruction_count` bounds the call. Exceeding it without the body returning is a hard
  // fault, not a truncated call: a callee that does not return within its bound means the caller
  // named the wrong address or the wrong bound, and continuing with half a function executed would
  // leave the guest in a state no ordinary call could have produced.
  [[nodiscard]] virtual InterpretedBlock call_original(std::uint32_t maximum_instruction_count) = 0;
};

} // namespace gcnport
