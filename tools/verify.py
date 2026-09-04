#!/usr/bin/env python3
"""Run gcnport's canonical local or native-hosted landing gate."""

from __future__ import annotations

import argparse
from pathlib import Path

from gcnport_tools.dolphin_runtime import verify_runtime
from gcnport_tools.host import detect_host, self_test as host_self_test
from gcnport_tools.project_verifier import verify_project


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--runtime", action="store_true", help="build and execute the Dolphin JIT test"
    )
    parser.add_argument("--expected-os", choices=("linux", "windows", "macos"))
    parser.add_argument("--expected-arch", choices=("x64", "arm64"))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parents[1]
    host_self_test()
    host = detect_host(args.expected_os, args.expected_arch)
    verify_project(root, host)
    if args.runtime:
        verify_runtime(root, host)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
