"""Process orchestration shared by gcnport tooling entry points."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def run(command: list[str], root: Path) -> None:
    subprocess.run(command, cwd=root, check=True)


def capture(command: list[str], root: Path) -> str:
    result = subprocess.run(command, cwd=root, check=True, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    return result.stdout
