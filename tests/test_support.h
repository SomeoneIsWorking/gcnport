// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace gcnport::test {

inline void require(bool condition, std::string_view expression, std::string_view file, int line) {
  if (condition) {
    return;
  }
  std::cerr << file << ':' << line << ": requirement failed: " << expression << '\n';
  std::exit(1);
}

} // namespace gcnport::test

#define GCPORT_REQUIRE(expression)                                                                 \
  ::gcnport::test::require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
