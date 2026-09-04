#!/usr/bin/env python3
"""Validate gcnport's native hosted-verification workflow."""

from __future__ import annotations

import argparse
from pathlib import Path

from gcnport_tools.ci_contract import inspect_workflow, self_test


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--selftest", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.selftest:
        self_test()
        print("hosted-CI checker self-test passed: complete and mutable-pin fixtures checked")
        return 0
    workflow = args.root / ".github" / "workflows" / "hosted-verification.yml"
    try:
        source = workflow.read_text(encoding="utf-8")
    except OSError as error:
        print(f"hosted-CI check failed before inspection: {error}")
        return 1
    errors = inspect_workflow(source)
    if errors:
        print(f"hosted-CI check failed: {len(errors)} finding(s)")
        print("\n".join(f"- {error}" for error in errors))
        return 1
    print("hosted-CI check passed: 5 native runner targets and 3 immutable action pins checked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
