"""Build and execute Dolphin's asset-free native shipping-JIT discriminator."""

from __future__ import annotations

from pathlib import Path

from .host import HostTarget
from .runner import build_ninja, capture, run

TEST_NAME = "GcnPortRuntime.ShippingJitCacheHookOriginalAndInvalidation"
DOLPHIN_OPTIONS = (
    "-DENABLE_TESTS=ON",
    "-DENABLE_QT=OFF",
    "-DENABLE_NOGUI=OFF",
    "-DENABLE_CLI_TOOL=OFF",
    "-DENABLE_VULKAN=OFF",
    "-DENABLE_SDL=OFF",
    "-DENABLE_CUBEB=OFF",
    "-DENABLE_ALSA=OFF",
    "-DENABLE_PULSEAUDIO=OFF",
    "-DENABLE_LLVM=OFF",
    "-DENCODE_FRAMEDUMPS=OFF",
    "-DUSE_UPNP=OFF",
    "-DUSE_DISCORD_PRESENCE=OFF",
    "-DUSE_MGBA=OFF",
    "-DUSE_RETRO_ACHIEVEMENTS=OFF",
    "-DENABLE_ANALYTICS=OFF",
    "-DENABLE_AUTOUPDATE=OFF",
    "-DUSE_SYSTEM_LIBS=OFF",
)
LINUX_OPTIONS = (
    "-DENABLE_X11=OFF",
    "-DENABLE_EGL=OFF",
    "-DENABLE_HWDB=OFF",
    "-DENABLE_EVDEV=OFF",
)


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
    platform_options = LINUX_OPTIONS if host.operating_system == "linux" else ()
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
            *DOLPHIN_OPTIONS,
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
    if (
        "GcnPortRuntime." not in listing
        or "ShippingJitCacheHookOriginalAndInvalidation" not in listing
    ):
        raise RuntimeError(f"shipping Dolphin JIT test is absent from {executable}")
    output = capture([str(executable), f"--gtest_filter={TEST_NAME}"], root)
    if "[  PASSED  ] 1 test." not in output or "[  SKIPPED ]" in output:
        raise RuntimeError(
            "shipping Dolphin JIT discriminator did not execute exactly one passing test"
        )
    print(
        "native Dolphin runtime verification passed: "
        f"host={host.operating_system}/{host.architecture}, test={TEST_NAME}"
    )
