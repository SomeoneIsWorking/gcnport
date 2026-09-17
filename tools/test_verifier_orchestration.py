#!/usr/bin/env python3
"""Exercise canonical build sequencing without compiling the Dolphin corpus."""

import subprocess
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from unittest.mock import patch

from gcnport_tools.dolphin_runtime import verify_runtime
from gcnport_tools.dolphin_tests import required_tests
from gcnport_tools.host import HostTarget
from gcnport_tools.project_verifier import verify_project
from test_dolphin_tests import listing_for, result_for

ROOT = Path(__file__).resolve().parents[1]
HOST = HostTarget("windows", "x64", "clang-cl", "clang-cl", "Clang", "clang-format", "clang-tidy")


class VerificationBuildTests(unittest.TestCase):
    def exercise(self, runtime: bool, failed: bool) -> None:
        commands: list[list[str]] = []

        def process(command: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
            self.assertIs(kwargs.get("check"), True)
            commands.append(command)
            if failed and command[:2] == ["cmake", "--build"]:
                raise subprocess.CalledProcessError(1, command)
            return subprocess.CompletedProcess(command, 0)

        with (
            redirect_stdout(StringIO()),
            patch("gcnport_tools.runner.subprocess.run", side_effect=process),
            patch("gcnport_tools.dolphin_runtime._compiler_id", return_value="Clang"),
            patch("gcnport_tools.dolphin_runtime.capture") as capture,
        ):
            capture.side_effect = [
                listing_for(required_tests(HOST)),
                result_for(required_tests(HOST)),
            ]
            verify = verify_runtime if runtime else verify_project
            if failed:
                with self.assertRaises(subprocess.CalledProcessError):
                    verify(ROOT, HOST)
                capture.assert_not_called()
                self.assertEqual(commands[-1][:2], ["cmake", "--build"])
                self.assertFalse(any(command[0] == "ctest" for command in commands))
                self.assertFalse(any("--install" in command for command in commands))
            else:
                verify(ROOT, HOST)
                if runtime:
                    self.assertEqual(capture.call_count, 2)
                    self.assertIn(
                        f"--gtest_filter={':'.join(required_tests(HOST))}",
                        capture.call_args.args[0],
                    )
                else:
                    self.assertTrue(any(command[0] == "ctest" for command in commands))
                    self.assertTrue(any("--install" in command for command in commands))

        builds = [command for command in commands if command[:2] == ["cmake", "--build"]]
        self.assertEqual(len(builds), 1)
        self.assertEqual(builds[0][-3:], ["--", "-k", "0"])
        configurations = [command for command in commands if command[:2] == ["cmake", "-S"]]
        self.assertEqual(len(configurations), 1)
        self.assertIn("Ninja", configurations[0])

    def test_project_success_continues_after_collecting_build(self) -> None:
        self.exercise(runtime=False, failed=False)

    def test_project_build_failure_stops_tests_and_install(self) -> None:
        self.exercise(runtime=False, failed=True)

    def test_runtime_success_executes_exact_discriminator(self) -> None:
        self.exercise(runtime=True, failed=False)

    def test_runtime_build_failure_stops_test_discovery_and_execution(self) -> None:
        self.exercise(runtime=True, failed=True)

    def test_runtime_missing_required_test_stops_before_execution(self) -> None:
        with (
            patch("gcnport_tools.dolphin_runtime.run"),
            patch("gcnport_tools.dolphin_runtime.build_ninja"),
            patch("gcnport_tools.dolphin_runtime._compiler_id", return_value="Clang"),
            patch("gcnport_tools.dolphin_runtime.capture", return_value="") as capture,
            self.assertRaisesRegex(RuntimeError, "discovery scanned 0"),
        ):
            verify_runtime(ROOT, HOST)
        capture.assert_called_once()
        self.assertIn("--gtest_list_tests", capture.call_args.args[0])


if __name__ == "__main__":
    unittest.main()
