"""Canonical first-party build and quality gate."""

from __future__ import annotations

import json
import sys
from pathlib import Path

from .adapter_sources import dolphin_dependent_sources
from .host import HostTarget
from .runner import build_ninja, run


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
    run([python, "tools/test_verifier_orchestration.py"], root)
    run([python, "tools/test_dolphin_tests.py"], root)
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
    build_ninja(root, build)
    run(["ctest", "--test-dir", str(build), "--output-on-failure"], root)
    run(["cmake", "--install", str(build), "--prefix", str(install)], root)
    cpp_files = sorted(
        str(path.relative_to(root))
        for owner in ("include", "src", "tests")
        for path in (root / owner).rglob("*")
        if path.suffix in {".cpp", ".h"}
    )
    # Formatting needs no compile database, so it covers every first-party file here.
    run([host.formatter, "--dry-run", "--Werror", *cpp_files], root)

    # Linting does need one, and this build tree deliberately never compiles Dolphin. The
    # Dolphin-dependent sources are therefore linted by the runtime gate against the adapter build
    # instead -- and named, so this can prove it deferred exactly those. Anything else absent from
    # the compile database would be a file nothing lints at all.
    deferred = set(dolphin_dependent_sources(root))
    translation_units = [
        path for path in cpp_files if path.endswith(".cpp") and path not in deferred
    ]
    compiled = _compiled_sources(root, build)
    unlinted = sorted(set(translation_units) - compiled)
    if unlinted:
        raise RuntimeError(
            "first-party translation units are in no compile database and are not deferred to the "
            "runtime gate: " + ", ".join(unlinted)
        )
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
    print(
        f"project verification passed: linted {len(translation_units)} translation unit(s); "
        f"{len(deferred)} Dolphin-dependent source(s) deferred to --runtime"
    )


def _compiled_sources(root: Path, build: Path) -> set[str]:
    database = build / "compile_commands.json"
    if not database.is_file():
        raise RuntimeError(f"no compile database at {database}")
    entries = json.loads(database.read_text(encoding="utf-8"))
    compiled = set()
    for entry in entries:
        path = Path(entry["directory"]) / Path(entry["file"])
        resolved = path.resolve()
        if resolved.is_relative_to(root):
            compiled.add(resolved.relative_to(root).as_posix())
    return compiled
