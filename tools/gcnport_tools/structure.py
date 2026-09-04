"""Mechanical first-party structure and ownership checks."""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}
SOURCE_LIMIT = 1_200
FORBIDDEN_PRODUCT_TEXT = {
    "getenv(": "environment reads belong to the configuration owner",
    "fprintf(stderr": "product diagnostics belong to the configured logger",
    "std::cerr": "product diagnostics belong to the configured logger",
    "printf(": "product diagnostics belong to the configured logger",
}


def inspect_sources(files: Mapping[Path, str]) -> list[str]:
    errors: list[str] = []
    for path, source in sorted(files.items(), key=lambda item: str(item[0])):
        line_count = len(source.splitlines())
        if line_count > SOURCE_LIMIT:
            errors.append(f"{path}: {line_count} lines exceeds {SOURCE_LIMIT}")
        if path.parts[0] not in {"include", "src"}:
            continue
        lowered = source.lower()
        for token, explanation in FORBIDDEN_PRODUCT_TEXT.items():
            if token.lower() in lowered:
                errors.append(f"{path}: forbidden {token!r}: {explanation}")
    return errors


def collect_sources(root: Path) -> dict[Path, str]:
    result: dict[Path, str] = {}
    for owner in ("include", "src", "tests"):
        owner_root = root / owner
        if not owner_root.is_dir():
            continue
        for path in owner_root.rglob("*"):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                result[path.relative_to(root)] = path.read_text(encoding="utf-8")
    return result


def inspect_tool_sources(files: Mapping[Path, str]) -> list[str]:
    errors: list[str] = []
    for relative, source in sorted(files.items(), key=lambda item: str(item[0])):
        path = Path(relative)
        if path.suffix == ".sh":
            errors.append(f"{relative}: project automation must be Python, not shell")
        if path.suffix != ".py":
            continue
        try:
            compile(source, str(relative), "exec")
        except SyntaxError as error:
            errors.append(f"{relative}:{error.lineno}: invalid Python: {error.msg}")
    return errors


def inspect_tools(root: Path) -> list[str]:
    tools = root / "tools"
    files = {
        path.relative_to(root): path.read_text(encoding="utf-8")
        for path in tools.rglob("*")
        if path.is_file() and path.suffix in {".py", ".sh"}
    }
    return inspect_tool_sources(files)


def self_test() -> None:
    clean = {Path("src/clean.cpp"): "void f() {}\n"}
    if inspect_sources(clean):
        raise AssertionError("clean fixture was rejected")
    planted = {Path("src/bad.cpp"): "auto* value = getenv(\"BAD\");\n"}
    errors = inspect_sources(planted)
    if len(errors) != 1 or "configuration owner" not in errors[0]:
        raise AssertionError("planted environment read was not rejected")
    tool_errors = inspect_tool_sources(
        {
            Path("tools/bad.sh"): "#!/bin/sh\n",
            Path("tools/broken.py"): "def incomplete(\n",
        }
    )
    if len(tool_errors) != 2:
        raise AssertionError("planted shell and invalid-Python tools were not rejected")
