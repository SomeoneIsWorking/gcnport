#!/usr/bin/env python3
"""Run gcnport's local landing gate with Clang and Ninja."""

from __future__ import annotations

import sys
from pathlib import Path

from gcnport_tools.runner import run


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    build = root / "build" / "verify"
    python = sys.executable
    run([python, "tools/check_structure.py", "--selftest"], root)
    run([python, "tools/check_structure.py", "--root", str(root)], root)
    run([python, "tools/check_dependency.py", "--selftest"], root)
    run([python, "tools/check_dependency.py", "--root", "extern/dolphin"], root)
    run([python, "tools/check_dolphin_contract.py", "--selftest"], root)
    run([python, "tools/check_dolphin_contract.py", "--root", "extern/dolphin"], root)
    run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            str(build),
            "-G",
            "Ninja",
            "-DCMAKE_CXX_COMPILER=clang++",
            "-DCMAKE_BUILD_TYPE=Debug",
        ],
        root,
    )
    run(["cmake", "--build", str(build)], root)
    run(["ctest", "--test-dir", str(build), "--output-on-failure"], root)
    cpp_files = sorted(
        str(path.relative_to(root))
        for owner in ("include", "src", "tests")
        for path in (root / owner).rglob("*")
        if path.suffix in {".cpp", ".h"}
    )
    run(["clang-format", "--dry-run", "--Werror", *cpp_files], root)
    translation_units = [path for path in cpp_files if path.endswith(".cpp")]
    run(
        [
            "clang-tidy",
            "-p",
            str(build),
            "--config-file=.clang-tidy",
            *translation_units,
        ],
        root,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
