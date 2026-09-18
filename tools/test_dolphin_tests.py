#!/usr/bin/env python3
"""Positive and negative controls for the shipping native regression selector and reports."""

import unittest
from dataclasses import replace

from gcnport_tools.dolphin_tests import required_tests, validate_listing, validate_result
from gcnport_tools.host import HostTarget

HOST = HostTarget("windows", "x64", "clang-cl", "clang-cl", "Clang", "clang-format", "clang-tidy")


def listing_for(names: tuple[str, ...]) -> str:
    suites: dict[str, list[str]] = {}
    for name in names:
        suite, test = name.split(".", 1)
        suites.setdefault(suite, []).append(test)
    return "".join(
        f"{suite}.\n" + "".join(f"  {test}\n" for test in tests) for suite, tests in suites.items()
    )


def result_for(names: tuple[str, ...]) -> str:
    return "".join(f"[ RUN      ] {name}\n[       OK ] {name} (0 ms)\n" for name in names) + (
        f"[  PASSED  ] {len(names)} tests.\n"
    )


class DolphinReportTests(unittest.TestCase):
    def test_exact_platform_architecture_inventories(self) -> None:
        for system, arch, count in (
            ("windows", "x64", 30),
            ("linux", "x64", 31),
            ("macos", "x64", 31),
            ("linux", "arm64", 28),
            ("macos", "arm64", 28),
        ):
            with self.subTest(system=system, arch=arch):
                names = required_tests(replace(HOST, operating_system=system, architecture=arch))
                self.assertEqual(len(names), count)
                self.assertEqual(len(set(names)), count)
                self.assertEqual(
                    any(name.startswith("DSPJitState.") for name in names), arch == "x64"
                )
                self.assertEqual(
                    "SSLTransportTest.MatchesMbedTLSReference" in names, system != "windows"
                )
                validate_listing(listing_for(names), names)
                validate_result(result_for(names), names)

    def test_unsupported_host_refuses_inventory(self) -> None:
        for system, arch in (("windows", "arm64"), ("android", "arm64"), ("linux", "unknown")):
            with (
                self.subTest(system=system, arch=arch),
                self.assertRaisesRegex(RuntimeError, "inventory"),
            ):
                required_tests(replace(HOST, operating_system=system, architecture=arch))

    def test_listing_refuses_empty_missing_and_duplicate_required_tests(self) -> None:
        names = required_tests(HOST)
        for listing in ("", listing_for(names[1:]), listing_for(names + names[:1])):
            with (
                self.subTest(listing=listing),
                self.assertRaisesRegex(RuntimeError, "discovery scanned"),
            ):
                validate_listing(listing, names)

    def test_unrelated_discovered_tests_do_not_enter_selected_inventory(self) -> None:
        names = required_tests(HOST)
        validate_listing(listing_for(names + ("Unrelated.Other",)), names)

    def test_result_refuses_missing_duplicate_and_wrong_test_even_with_same_count(self) -> None:
        names = required_tests(HOST)
        for observed in ((), names[1:], names + names[:1], names[1:] + ("Unrelated.Other",)):
            with (
                self.subTest(observed=observed),
                self.assertRaisesRegex(RuntimeError, "inventory mismatch"),
            ):
                validate_result(result_for(observed), names)

    def test_result_requires_each_start_completion_and_exact_single_summary(self) -> None:
        names = required_tests(HOST)
        output = result_for(names)
        summary = f"[  PASSED  ] {len(names)} tests.\n"
        cases = (
            output.replace(f"[ RUN      ] {names[0]}\n", ""),
            output.replace(f"[       OK ] {names[0]} (0 ms)\n", ""),
            output.replace(summary, "[  PASSED  ] 0 tests.\n"),
            output.replace(summary, ""),
            output + summary,
        )
        for observed in cases:
            with self.subTest(observed=observed), self.assertRaises(RuntimeError):
                validate_result(observed, names)

    def test_result_refuses_failure_or_skip_despite_passing_summary(self) -> None:
        names = required_tests(HOST)
        for status in ("FAILED", "SKIPPED"):
            with (
                self.subTest(status=status),
                self.assertRaisesRegex(RuntimeError, "failure or skip"),
            ):
                validate_result(result_for(names) + f"[  {status} ] {names[0]}\n", names)


if __name__ == "__main__":
    unittest.main()
