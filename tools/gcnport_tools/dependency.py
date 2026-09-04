"""Validate gcnport's exact maintained Dolphin dependency checkout."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
import subprocess


@dataclass(frozen=True)
class DependencySpecification:
    revision: str
    repository: str


@dataclass(frozen=True)
class DependencyStatus:
    revision: str
    origin: str

    def errors(self, specification: DependencySpecification) -> list[str]:
        errors: list[str] = []
        if self.revision != specification.revision:
            errors.append(
                f"revision mismatch: expected {specification.revision}, found {self.revision}"
            )
        if self.origin != specification.repository:
            errors.append(
                f"origin mismatch: expected {specification.repository}, found {self.origin}"
            )
        return errors


def load_specification(manifest: Path) -> DependencySpecification:
    try:
        document = json.loads(manifest.read_text(encoding="utf-8"))
        dolphin = document["dolphin"]
        specification = DependencySpecification(
            revision=str(dolphin["revision"]),
            repository=str(dolphin["repository"]),
        )
    except (OSError, json.JSONDecodeError, KeyError, TypeError) as error:
        raise ValueError(f"invalid dependency manifest {manifest}: {error}") from error
    if not specification.revision or not specification.repository:
        raise ValueError(f"invalid dependency manifest {manifest}: empty Dolphin field")
    return specification


def git_output(checkout: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=checkout,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def inspect_checkout(checkout: Path) -> DependencyStatus:
    return DependencyStatus(
        revision=git_output(checkout, "rev-parse", "HEAD"),
        origin=git_output(checkout, "remote", "get-url", "origin"),
    )


def self_test() -> None:
    specification = DependencySpecification("expected-revision", "https://example.test/dolphin.git")
    if DependencyStatus(specification.revision, specification.repository).errors(specification):
        raise AssertionError("matching dependency fixture was rejected")
    errors = DependencyStatus("wrong", specification.repository).errors(specification)
    if len(errors) != 1 or "revision mismatch" not in errors[0]:
        raise AssertionError("mismatched dependency fixture was not rejected")
