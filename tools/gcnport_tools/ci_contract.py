"""Mechanical contract for asset-free native hosted verification."""

from __future__ import annotations

import re


RUNNERS = {
    "ubuntu-26.04": ("linux", "x64"),
    "ubuntu-26.04-arm": ("linux", "arm64"),
    "windows-2022": ("windows", "x64"),
    "macos-15-intel": ("macos", "x64"),
    "macos-15": ("macos", "arm64"),
}
ACTION_PINS = (
    "actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1",
    "actions/setup-python@5fda3b95a4ea91299a34e894583c3862153e4b97",
    "astral-sh/setup-uv@20cfd1bf945f4377ade1205e4dbc17946fc9a30d",
)


def inspect_workflow(source: str) -> list[str]:
    errors: list[str] = []
    for runner, (system, architecture) in RUNNERS.items():
        entry = re.compile(
            rf"- runner: {re.escape(runner)}\s+"
            rf"target_os: {system}\s+"
            rf"target_arch: {architecture}(?:\s|$)"
        )
        if entry.search(source) is None:
            errors.append(f"missing native matrix entry: {runner} -> {system}/{architecture}")
    actions = tuple(re.findall(r"^\s*uses:\s*(\S+)\s*$", source, flags=re.MULTILINE))
    if actions != ACTION_PINS:
        errors.append(
            "workflow actions must be exactly the three approved immutable pins: "
            + ", ".join(ACTION_PINS)
        )
    required = (
        "permissions:\n  contents: read",
        "fetch-depth: 0",
        "submodules: recursive",
        "persist-credentials: false",
        'python-version: "3.12"',
        "timeout-minutes:",
        "uv run --frozen python tools/verify.py --runtime",
        "--expected-os",
        "--expected-arch",
    )
    for token in required:
        if token not in source:
            errors.append(f"missing hosted verification contract: {token}")
    for token in ("continue-on-error", "android", "pull_request_target"):
        if token in source.lower():
            errors.append(f"forbidden hosted verification token: {token}")
    return errors


def self_test() -> None:
    complete = "\n".join(
        [
            *(
                f"- runner: {runner}\n  target_os: {target[0]}\n  target_arch: {target[1]}"
                for runner, target in RUNNERS.items()
            ),
            *(f"uses: {action}" for action in ACTION_PINS),
            "permissions:\n  contents: read",
            "fetch-depth: 0",
            "submodules: recursive",
            "persist-credentials: false",
            'python-version: "3.12"',
            "timeout-minutes:",
            "uv run --frozen python tools/verify.py --runtime --expected-os x --expected-arch y",
        ]
    )
    if inspect_workflow(complete):
        raise AssertionError("complete hosted-CI fixture was rejected")
    errors = inspect_workflow(complete.replace(ACTION_PINS[0], "actions/checkout@v4"))
    if not any("approved immutable pins" in error for error in errors):
        raise AssertionError("mutable action fixture was not rejected")
    errors = inspect_workflow(
        complete.replace('python-version: "3.12"', 'python-version: "3.12.12"')
    )
    if not any("python-version" in error for error in errors):
        raise AssertionError("runner-specific Python patch fixture was not rejected")
