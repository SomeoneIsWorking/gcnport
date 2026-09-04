// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <utility>

namespace gcnport::test {

inline void require(bool condition, std::string_view expression, std::string_view file, int line) {
  if (condition) {
    return;
  }
  std::cerr << file << ':' << line << ": requirement failed: " << expression << '\n';
  std::exit(1);
}

template <typename TestBody>
int run_test_main(std::string_view suite_name, TestBody &&body,
                  std::ostream &output = std::cerr) noexcept {
  try {
    std::forward<TestBody>(body)();
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    output << suite_name << ": uncaught exception: " << error.what() << '\n';
  } catch (...) {
    output << suite_name << ": uncaught non-standard exception\n";
  }
  return EXIT_FAILURE;
}

} // namespace gcnport::test

#define GCPORT_REQUIRE(expression)                                                                 \
  ::gcnport::test::require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
