"""Native host identity and compiler selection for verification."""

from __future__ import annotations

from dataclasses import dataclass
import platform
from pathlib import Path
import shutil
import subprocess


SUPPORTED_TARGETS = {
    ("linux", "x64"),
    ("linux", "arm64"),
    ("windows", "x64"),
    ("macos", "x64"),
    ("macos", "arm64"),
}


@dataclass(frozen=True)
class HostTarget:
    operating_system: str
    architecture: str
    c_compiler: str
    cxx_compiler: str
    cmake_compiler_id: str
    formatter: str
    linter: str


def _quality_tool(name: str, system: str) -> str:
    executable = shutil.which(name)
    if executable is not None:
        return executable
    if system == "macos" and shutil.which("brew") is not None:
        prefix = subprocess.run(
            ["brew", "--prefix", "llvm@18"], check=True, capture_output=True, text=True
        ).stdout.strip()
        candidate = Path(prefix) / "bin" / name
        if candidate.is_file():
            return str(candidate)
    raise RuntimeError(f"required verification tool is unavailable: {name}")


def detect_host(expected_os: str | None = None, expected_arch: str | None = None) -> HostTarget:
    system_names = {"Linux": "linux", "Windows": "windows", "Darwin": "macos"}
    architecture_names = {
        "x86_64": "x64",
        "amd64": "x64",
        "aarch64": "arm64",
        "arm64": "arm64",
    }
    system = system_names.get(platform.system())
    architecture = architecture_names.get(platform.machine().lower())
    if system is None or architecture is None or (system, architecture) not in SUPPORTED_TARGETS:
        raise RuntimeError(
            f"unsupported native verification host: system={platform.system()}, "
            f"architecture={platform.machine()}"
        )
    if expected_os is not None and system != expected_os:
        raise RuntimeError(f"runner OS mismatch: expected {expected_os}, detected {system}")
    if expected_arch is not None and architecture != expected_arch:
        raise RuntimeError(
            f"runner architecture mismatch: expected {expected_arch}, detected {architecture}"
        )
    if system == "windows":
        return HostTarget(
            system,
            architecture,
            "clang-cl",
            "clang-cl",
            "Clang",
            _quality_tool("clang-format", system),
            _quality_tool("clang-tidy", system),
        )
    compiler_id = "AppleClang" if system == "macos" else "Clang"
    return HostTarget(
        system,
        architecture,
        "clang",
        "clang++",
        compiler_id,
        _quality_tool("clang-format", system),
        _quality_tool("clang-tidy", system),
    )


def self_test() -> None:
    if ("linux", "arm64") not in SUPPORTED_TARGETS or ("windows", "arm64") in SUPPORTED_TARGETS:
        raise AssertionError("supported-host matrix does not match the qualified scope")
