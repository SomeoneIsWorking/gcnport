"""Canonical first-party build and quality gate."""

from __future__ import annotations

import sys
from pathlib import Path

from .host import HostTarget
from .runner import run


def verify_project(root: Path, host: HostTarget) -> None:
    build = root / "build" / "verify"
    install = root / "build" / "install"
    python = sys.executable
    checks = (
        "tools/check_structure.py",
        "tools/check_dependency.py",
        "tools/check_dolphin_contract.py",
        "tools/check_ci.py",
    )
    for check in checks:
        run([python, check, "--selftest"], root)
    run([python, "tools/check_structure.py", "--root", str(root)], root)
    run([python, "tools/check_dependency.py", "--root", "extern/dolphin"], root)
    run([python, "tools/check_dolphin_contract.py", "--root", "extern/dolphin"], root)
    run([python, "tools/check_ci.py", "--root", str(root)], root)
    run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            str(build),
            "-G",
            "Ninja",
            f"-DCMAKE_CXX_COMPILER={host.cxx_compiler}",
            "-DCMAKE_BUILD_TYPE=Debug",
        ],
        root,
    )
    run(["cmake", "--build", str(build)], root)
    run(["ctest", "--test-dir", str(build), "--output-on-failure"], root)
    run(["cmake", "--install", str(build), "--prefix", str(install)], root)
    cpp_files = sorted(
        str(path.relative_to(root))
        for owner in ("include", "src", "tests")
        for path in (root / owner).rglob("*")
        if path.suffix in {".cpp", ".h"}
    )
    run([host.formatter, "--dry-run", "--Werror", *cpp_files], root)
    translation_units = [path for path in cpp_files if path.endswith(".cpp")]
    run(
        [
            host.linter,
            "-p",
            str(build),
            "--config-file=.clang-tidy",
            *translation_units,
        ],
        root,
    )
