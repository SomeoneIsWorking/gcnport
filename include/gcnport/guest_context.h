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
};

} // namespace gcnport
