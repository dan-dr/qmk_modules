#!/usr/bin/env python3
"""Compare FSR Sentinel v1 with the frozen Run7 C reference."""

from __future__ import annotations

import csv
import hashlib
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from paths import MODULE, find_userspace

USERSPACE = find_userspace()
ANALYSIS = (
    USERSPACE / "fsr-recordings/2026-08-03-video-run7/analysis"
    if USERSPACE is not None
    else None
)
REFERENCE = ANALYSIS / "reference" if ANALYSIS is not None else None
SAMPLES = ANALYSIS / "samples.csv" if ANALYSIS is not None else None
SAMPLES_SHA256 = "b3c794530c7abfb0a6d87ced75d129fa0eb4425100322cef8276e671683e1cb7"
REFERENCE_C_SHA256 = "74801b2b57a74ec72dda96df64bb0fa51047838494895ea29b9db452a1bfb7c3"
REFERENCE_H_SHA256 = "62a675f1e2d1d35b237161cd001bdc60e502b8bc046021abceda7e3368f49710"


class FsrSentinelParityTest(unittest.TestCase):
    def test_all_canonical_run7_rows_match_frozen_reference(self) -> None:
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "host C compiler is required")
        if (
            SAMPLES is None
            or REFERENCE is None
            or not SAMPLES.exists()
            or not REFERENCE.exists()
        ):
            self.skipTest("ignored Run7 evidence tree is not available")
        self.assertEqual(
            hashlib.sha256(SAMPLES.read_bytes()).hexdigest(), SAMPLES_SHA256
        )
        self.assertEqual(
            hashlib.sha256(
                (REFERENCE / "fsr_detector_reference.c").read_bytes()
            ).hexdigest(),
            REFERENCE_C_SHA256,
        )
        self.assertEqual(
            hashlib.sha256(
                (REFERENCE / "fsr_detector_reference.h").read_bytes()
            ).hexdigest(),
            REFERENCE_H_SHA256,
        )

        with SAMPLES.open(newline="", encoding="utf-8") as handle:
            rows = list(csv.DictReader(handle))
        self.assertEqual(len(rows), 4426)
        replay_input = "".join(
            f'{row["reading"]},{row["t_ms"]},{row["scan"]}\n' for row in rows
        )

        with tempfile.TemporaryDirectory(prefix="fsr-sentinel-parity-") as directory:
            executable = Path(directory) / "parity"
            subprocess.run(
                [
                    str(compiler),
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    f"-I{REFERENCE}",
                    str(MODULE / "fsr_sentinel.c"),
                    str(REFERENCE / "fsr_detector_reference.c"),
                    str(MODULE / "tests/fsr_sentinel_parity_runner.c"),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            result = subprocess.run(
                [str(executable)],
                input=replay_input,
                text=True,
                capture_output=True,
                check=True,
            )
        self.assertEqual(result.stdout, "fsr_sentinel_parity: 4426 rows ok\n")


if __name__ == "__main__":
    unittest.main()
