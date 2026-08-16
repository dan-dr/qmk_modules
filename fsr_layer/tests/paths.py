"""Resolve module and optional userspace paths in both checkout layouts."""

from __future__ import annotations

import os
from pathlib import Path


MODULE = Path(__file__).resolve().parents[1]
TESTS = MODULE / "tests"


def find_userspace() -> Path | None:
    """Find the evidence-bearing userspace without coupling portable tests to it."""
    configured = os.environ.get("QMK_USERSPACE")
    if configured:
        return Path(configured).expanduser().resolve()

    candidates = (
        MODULE.parents[2],
        MODULE.parents[1] / "qmk_userspace",
    )
    for candidate in candidates:
        if (candidate / "qmk.json").is_file():
            return candidate
    return None
