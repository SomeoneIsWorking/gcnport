"""Process orchestration shared by gcnport tooling entry points."""

from __future__ import annotations

import subprocess
from pathlib import Path


def run(command: list[str], root: Path) -> None:
    subprocess.run(command, cwd=root, check=True)
