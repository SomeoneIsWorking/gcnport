"""Inspect a Dolphin checkout for the title-neutral gcnport embedding contract."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Protocol


BRIDGE_HEADER = Path("Source/Core/Core/PowerPC/GcnPortRuntime.h")
BRIDGE_SOURCE = Path("Source/Core/Core/PowerPC/GcnPortRuntime.cpp")


class SourceReader(Protocol):
    def read(self, path: Path) -> str | None: ...


@dataclass(frozen=True)
class FilesystemReader:
    root: Path

    def read(self, path: Path) -> str | None:
        candidate = self.root / path
        if not candidate.is_file():
            return None
        return candidate.read_text(encoding="utf-8")


@dataclass(frozen=True)
class MappingReader:
    files: dict[Path, str]

    def read(self, path: Path) -> str | None:
        return self.files.get(path)


@dataclass(frozen=True)
class Requirement:
    path: Path
    symbol: str
    purpose: str


REQUIREMENTS = (
    Requirement(BRIDGE_HEADER, "class RuntimeSession", "instance-owned CPU/runtime lifetime"),
    Requirement(BRIDGE_HEADER, "BootAuthenticatedImage", "authenticated runtime image boot"),
    Requirement(BRIDGE_HEADER, "ExecuteJitBlock", "one observable JIT block dispatch"),
    Requirement(BRIDGE_HEADER, "ExecuteRefusedBlock", "typed bounded fallback after refusal"),
    Requirement(
        BRIDGE_HEADER,
        "ExecuteDiagnosticInterpreterBlock",
        "explicit diagnostic-only interpreter dispatch",
    ),
    Requirement(BRIDGE_HEADER, "InstallNativeHook", "guest-address hook registration"),
    Requirement(BRIDGE_HEADER, "ExecuteOriginalOnce", "unpublished one-shot original execution"),
    Requirement(BRIDGE_HEADER, "InvalidateGuestCode", "hook and executable-image invalidation"),
    Requirement(BRIDGE_HEADER, "ExecutionCounters", "compiled/executed/fallback denominators"),
    Requirement(BRIDGE_SOURCE, "FallBackToInterpreter", "runtime fallback reason instrumentation"),
    Requirement(BRIDGE_SOURCE, "RunOriginalOnce", "backend one-shot original implementation"),
)


def missing_requirements(reader: SourceReader) -> list[Requirement]:
    missing: list[Requirement] = []
    for requirement in REQUIREMENTS:
        source = reader.read(requirement.path)
        if source is None or requirement.symbol not in source:
            missing.append(requirement)
    return missing


def format_report(missing: list[Requirement]) -> str:
    if not missing:
        return (
            "Dolphin embedding surface candidates present: "
            f"checked {len(REQUIREMENTS)} symbols; semantic execution remains unverified"
        )
    lines = [
        f"Dolphin embedding contract incomplete: {len(missing)}/{len(REQUIREMENTS)} requirements missing"
    ]
    lines.extend(
        f"- {requirement.path}: {requirement.symbol} ({requirement.purpose})"
        for requirement in missing
    )
    return "\n".join(lines)


def self_test() -> None:
    complete: dict[Path, str] = {}
    for requirement in REQUIREMENTS:
        complete[requirement.path] = complete.get(requirement.path, "") + requirement.symbol + "\n"
    if missing_requirements(MappingReader(complete)):
        raise AssertionError("complete planted contract was reported missing")

    incomplete = dict(complete)
    incomplete[BRIDGE_HEADER] = incomplete[BRIDGE_HEADER].replace("ExecuteOriginalOnce", "")
    missing = missing_requirements(MappingReader(incomplete))
    if len(missing) != 1 or missing[0].symbol != "ExecuteOriginalOnce":
        raise AssertionError("incomplete planted contract did not identify ExecuteOriginalOnce")
