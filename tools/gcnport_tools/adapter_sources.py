"""The first-party sources that need Dolphin's headers to compile.

They are linted against the adapter build tree by the runtime gate, not against the plain build tree
the project gate configures, because that tree deliberately never compiles Dolphin and clang-tidy
would fail on the very first `#include "Core/..."`. Naming them in one place is what lets the project
gate prove it deferred exactly these and nothing else: a first-party translation unit missing from
the compile database and missing from this list is an unlinted file, which is the failure mode this
module exists to make impossible.
"""

from __future__ import annotations

from pathlib import Path

DOLPHIN_DEPENDENT_SOURCES = (
    "include/gcnport/dolphin_adapter.h",
    "src/dolphin_adapter.cpp",
    "tests/dolphin_adapter_test.cpp",
)


def dolphin_dependent_sources(root: Path) -> tuple[str, ...]:
    """Validate that every named source exists, so a rename cannot silently drop one from linting."""
    missing = [name for name in DOLPHIN_DEPENDENT_SOURCES if not (root / name).is_file()]
    if missing:
        raise RuntimeError(
            "adapter_sources names files that do not exist: " + ", ".join(sorted(missing))
        )
    return DOLPHIN_DEPENDENT_SOURCES


def dolphin_dependent_translation_units(root: Path) -> list[str]:
    return [name for name in dolphin_dependent_sources(root) if name.endswith(".cpp")]
