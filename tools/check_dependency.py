#!/usr/bin/env python3
"""Validate the exact local Dolphin fork checkout used by gcnport."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess

from gcnport_tools.dependency import inspect_checkout, load_specification, self_test


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("extern/dolphin"))
    parser.add_argument("--manifest", type=Path, default=Path("dependencies.json"))
    parser.add_argument("--selftest", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.selftest:
        self_test()
        print("dependency checker self-test passed: matching and mismatched fixtures checked")
        return 0

    try:
        specification = load_specification(args.manifest)
        status = inspect_checkout(args.root)
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"Dolphin dependency check failed before inspection: {error}")
        return 1
    errors = status.errors(specification)
    if errors:
        print(f"Dolphin dependency check failed: {len(errors)} mismatch(es)")
        print("\n".join(f"- {error}" for error in errors))
        return 1
    print(f"Dolphin dependency check passed: revision={status.revision}, origin={status.origin}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
