#!/usr/bin/env python3
"""Check gcnport first-party source boundaries."""

from __future__ import annotations

import argparse
from pathlib import Path

from gcnport_tools.structure import collect_sources, inspect_sources, inspect_tools, self_test


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--selftest", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.selftest:
        self_test()
        print("structure checker self-test passed: clean and planted-negative fixtures checked")
        return 0

    files = collect_sources(args.root)
    errors = [*inspect_sources(files), *inspect_tools(args.root)]
    if errors:
        print(f"structure check failed: {len(errors)} finding(s) across {len(files)} source files")
        print("\n".join(f"- {error}" for error in errors))
        return 1
    print(f"structure check passed: {len(files)} source files checked, limit={1_200}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
