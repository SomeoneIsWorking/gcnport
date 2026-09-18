"""Build and execute the asset-free native runtime gates: Dolphin's own shipping-JIT
discriminator, and gcnport's Dolphin adapter."""

from __future__ import annotations

import json
from pathlib import Path

from .adapter_sources import dolphin_dependent_translation_units
from .dolphin_tests import required_tests, validate_listing, validate_result
from .host import HostTarget
from .runner import build_ninja, capture, run

def _embedded_option_names(root: Path) -> tuple[list[str], list[str]]:
    """Read the embedded-build option names from the one file that records them.

    cmake/DolphinDependency.cmake reads the same entries, so the adapter build and this standalone
    runtime build cannot drift apart. An absent or empty list would silently configure a full
    Dolphin build -- frontend, audio backends and all -- so it is refused by name here rather than
    discovered as a twenty-minute compile.
    """
    manifest = json.loads((root / "dependencies.json").read_text(encoding="utf-8"))
    options = manifest["dolphin"]["embedded_build_options"]
    names = []
    for member in ("off", "linux_off"):
        member_names = options.get(member)
        if not isinstance(member_names, list) or not member_names:
            raise RuntimeError(
                f"dependencies.json has no non-empty dolphin.embedded_build_options.{member} array"
            )
        names.append([str(name) for name in member_names])
    return names[0], names[1]


def _compiler_id(build: Path) -> str:
    candidates = list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if len(candidates) != 1:
        raise RuntimeError(f"expected one CMake compiler identity file, found {len(candidates)}")
    source = candidates[0].read_text(encoding="utf-8")
    marker = 'set(CMAKE_CXX_COMPILER_ID "'
    start = source.find(marker)
    if start < 0:
        raise RuntimeError("CMake compiler identity is absent")
    start += len(marker)
    return source[start : source.find('"', start)]


def verify_runtime(root: Path, host: HostTarget) -> None:
    dolphin = root / "extern" / "dolphin"
    build = root / "build" / "dolphin-runtime"
    off_names, linux_off_names = _embedded_option_names(root)
    # ENABLE_TESTS is the one option this build disagrees with the adapter build about: Dolphin's
    # own gtest binary is exactly what this gate runs.
    dolphin_options = ["-DENABLE_TESTS=ON", *(f"-D{name}=OFF" for name in off_names)]
    platform_options = (
        [f"-D{name}=OFF" for name in linux_off_names]
        if host.operating_system == "linux"
        else []
    )
    run(
        [
            "cmake",
            "-S",
            str(dolphin),
            "-B",
            str(build),
            "-G",
            "Ninja",
            f"-DCMAKE_C_COMPILER={host.c_compiler}",
            f"-DCMAKE_CXX_COMPILER={host.cxx_compiler}",
            "-DCMAKE_BUILD_TYPE=Release",
            *dolphin_options,
            *platform_options,
        ],
        root,
    )
    actual_compiler_id = _compiler_id(build)
    if actual_compiler_id != host.cmake_compiler_id:
        raise RuntimeError(
            f"Dolphin compiler mismatch: expected {host.cmake_compiler_id}, "
            f"found {actual_compiler_id}"
        )
    build_ninja(root, build, target="tests")
    suffix = "Tests/tests.exe" if host.operating_system == "windows" else "Tests/tests"
    executable = build / "Binaries" / suffix
    listing = capture([str(executable), "--gtest_list_tests"], root)
    expected = required_tests(host)
    validate_listing(listing, expected)
    output = capture([str(executable), f"--gtest_filter={':'.join(expected)}"], root)
    validate_result(output, expected)
    _verify_dolphin_adapter(root, host)
    print(
        "native Dolphin runtime verification passed: "
        f"host={host.operating_system}/{host.architecture}, passed={len(expected)} required tests"
    )


def _verify_dolphin_adapter(root: Path, host: HostTarget) -> None:
    """Build and run the adapter that binds gcnport's contracts to Dolphin's RuntimeSession.

    This is a separate build tree from the one above: that one configures the Dolphin fork
    standalone with its own gtest binary, while this configures gcnport itself with
    GCPORT_BUILD_DOLPHIN_ADAPTER=ON, which pulls the fork in as a subdirectory through
    cmake/DolphinDependency.cmake. The adapter cannot be checked by verify_project, which
    deliberately never compiles Dolphin.
    """
    build = root / "build" / "dolphin-adapter"
    run(
        [
            "cmake",
            "-S",
            str(root),
            "-B",
            str(build),
            "-G",
            "Ninja",
            f"-DCMAKE_C_COMPILER={host.c_compiler}",
            f"-DCMAKE_CXX_COMPILER={host.cxx_compiler}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DGCPORT_BUILD_DOLPHIN_ADAPTER=ON",
        ],
        root,
    )
    actual_compiler_id = _compiler_id(build)
    if actual_compiler_id != host.cmake_compiler_id:
        raise RuntimeError(
            f"Dolphin adapter compiler mismatch: expected {host.cmake_compiler_id}, "
            f"found {actual_compiler_id}"
        )
    build_ninja(root, build, target="gcnport_dolphin_adapter_test")
    run(["ctest", "--test-dir", str(build), "-R", "dolphin_adapter", "--output-on-failure"], root)
    # The half of the lint the project gate cannot do: these translation units only have compile
    # commands in this tree, because only this tree compiles Dolphin.
    translation_units = dolphin_dependent_translation_units(root)
    run(
        [host.linter, "-p", str(build), "--config-file=.clang-tidy", *translation_units],
        root,
    )
