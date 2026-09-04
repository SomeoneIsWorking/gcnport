// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>

#include "test_support.h"

namespace {

void successful_body_returns_success() {
  bool called = false;
  std::ostringstream output;

  const int result = gcnport::test::run_test_main("success", [&called] { called = true; }, output);

  GCPORT_REQUIRE(result == EXIT_SUCCESS);
  GCPORT_REQUIRE(called);
  GCPORT_REQUIRE(output.str().empty());
}

void standard_exception_is_reported() {
  std::ostringstream output;

  const int result = gcnport::test::run_test_main(
      "standard", [] { throw std::runtime_error("planted failure"); }, output);

  GCPORT_REQUIRE(result == EXIT_FAILURE);
  GCPORT_REQUIRE(output.str() == "standard: uncaught exception: planted failure\n");
}

void non_standard_exception_is_reported() {
  std::ostringstream output;

  const int result = gcnport::test::run_test_main("non_standard", [] { throw 7; }, output);

  GCPORT_REQUIRE(result == EXIT_FAILURE);
  GCPORT_REQUIRE(output.str() == "non_standard: uncaught non-standard exception\n");
}

} // namespace

int main() {
  return gcnport::test::run_test_main("test_support", [] {
    successful_body_returns_success();
    standard_exception_is_reported();
    non_standard_exception_is_reported();
  });
}
