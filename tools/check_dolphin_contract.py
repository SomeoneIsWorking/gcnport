#!/usr/bin/env python3
"""Report whether the pinned Dolphin fork exposes gcnport's embedding seam."""

from __future__ import annotations

import argparse
from pathlib import Path

from gcnport_tools.dolphin_contract import FilesystemReader, format_report, missing_requirements, self_test


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("extern/dolphin"))
    parser.add_argument("--require", action="store_true", help="fail when the contract is incomplete")
    parser.add_argument("--selftest", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.selftest:
        self_test()
        print("dolphin contract probe self-test passed: positive and negative fixtures checked")
        return 0

    missing = missing_requirements(FilesystemReader(args.root))
    print(format_report(missing))
    return 1 if args.require and missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
