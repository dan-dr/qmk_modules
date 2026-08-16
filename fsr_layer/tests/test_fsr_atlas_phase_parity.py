#!/usr/bin/env python3
"""Prove Atlas Phase firmware output matches the frozen clean-slate model."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from paths import MODULE, TESTS, find_userspace

USERSPACE = find_userspace()
REFERENCE = (
    USERSPACE
    / "fsr-recordings/2026-08-07-scan-215949/analysis/clean-slate/src"
    if USERSPACE is not None
    else None
)


def waveform(start: int = 0) -> str:
    rows: list[tuple[int, int, int, int]] = []
    now = start

    def add(value: int, motion: int = 0, dt: int = 1) -> None:
        nonlocal now
        rows.append((value, now & 0xFFFFFFFF, motion, 0))
        now += dt

    for _ in range(30):
        add(1000)
    for value in range(1000, 1151, 10):
        add(value)
    for index in range(180):
        add(1150 + (index % 7) - 3, 4 if index in (30, 90) else 0)
    for value in range(1150, 999, -10):
        add(value)
    for _ in range(20):
        add(1000)
    add(1070)
    add(1000, 4)
    add(1100, 4)
    for _ in range(80):
        add(1000)
    add(1150)
    now += 200
    add(1150)
    for _ in range(30):
        add(1000)
    return "".join(f"{value},{t_ms},{mx},{my}\n" for value, t_ms, mx, my in rows)


class FsrAtlasPhaseParityTest(unittest.TestCase):
    def test_frontends_bounds_motion_dwell_gap_and_wrap_match_reference(self) -> None:
        cc = shutil.which("cc")
        cxx = shutil.which("c++")
        self.assertIsNotNone(cc, "host C compiler is required")
        self.assertIsNotNone(cxx, "host C++ compiler is required")
        if REFERENCE is None or not REFERENCE.is_dir():
            self.skipTest("frozen Atlas reference is not available")
        cases = (
            (0, 1, 1, 100, 1, 10, 20, 1, 10000, 8, 50, 50, 100, 50, 0, 0, 40),
            (1, 5, 3, 80, 1, 25, 30, 2, 2000, 17, 35, 45, 80, 20, 6, 9, 20),
            (2, 7, 5, 160, 3, 40, 25, 2, 4000, 31, 40, 55, 90, 30, 10, 12, 25),
            (3, 8, 2, 80, 1, 30, 15, 1, 3000, 20, 30, 35, 70, 15, 4, 8, 18),
            (4, 40, 7, 320, 3, 60, 35, 4, 5000, 45, 45, 60, 110, 40, 14, 21, 30),
            # Canonicalization boundaries from the frozen search contract.
            (0, 1000, 20, 20, 0, 0, 0, 0, 1, 1280, 1, 1000, 1, -500, 1000, 1000, 1000),
            (1, 16, 20, 1000, 1, 500, 500, 1000, 10000, 1279, 1000, 1, 1500, 1499, 0, 0, 1000),
            (2, 1000, 1, 20, 2, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1000, 1),
            (3, 1000, 20, 1000, 3, 500, 500, 1000, 10000, 1280, 1000, 1000, 1500, -500, 1000, 1000, 1000),
            (4, 1, 1, 20, 3, 0, 0, 0, 1, 1, 1, 1, 1, -500, 0, 0, 1),
        )
        with tempfile.TemporaryDirectory(prefix="fsr-atlas-parity-") as directory:
            firmware = Path(directory) / "firmware"
            reference = Path(directory) / "reference"
            subprocess.run(
                [
                    str(cc), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(MODULE / "fsr_atlas_phase.c"),
                    str(TESTS / "fsr_atlas_phase_runner.c"),
                    "-o", str(firmware),
                ],
                check=True,
            )
            subprocess.run(
                [
                    str(cxx), "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    f"-I{REFERENCE}", str(REFERENCE / "detectors.cpp"),
                    str(TESTS / "fsr_atlas_phase_reference_runner.cpp"),
                    "-o", str(reference),
                ],
                check=True,
            )
            for params in cases:
                arguments = [str(value) for value in params]
                for samples in (waveform(), waveform(0xFFFFFFFF - 100)):
                    expected = subprocess.run(
                        [str(reference), *arguments], input=samples, text=True,
                        capture_output=True, check=True,
                    ).stdout
                    actual = subprocess.run(
                        [str(firmware), *arguments], input=samples, text=True,
                        capture_output=True, check=True,
                    ).stdout
                    self.assertEqual(actual, expected, f"params={params}")

    def test_rolling_alone_enters_without_press(self) -> None:
        cc = shutil.which("cc")
        self.assertIsNotNone(cc, "host C compiler is required")
        idle_then_roll = "".join(
            f"1000,{t},0,0\n" for t in range(40)
        ) + "".join(
            f"1000,{t},20,0\n" for t in range(40, 90)
        )
        defaults = [0, 1, 1, 100, 0, 50, 10, 5, 1500, 8, 45, 40, 70, 40, 0, 5, 40]
        with tempfile.TemporaryDirectory(prefix="fsr-atlas-roll-") as directory:
            firmware = Path(directory) / "firmware"
            subprocess.run(
                [
                    str(cc), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(MODULE / "fsr_atlas_phase.c"),
                    str(TESTS / "fsr_atlas_phase_runner.c"),
                    "-o", str(firmware),
                ],
                check=True,
            )

            def touched(mode: int) -> str:
                params = list(defaults)
                params[4] = mode
                return subprocess.run(
                    [str(firmware), *[str(value) for value in params]],
                    input=idle_then_roll, text=True, capture_output=True, check=True,
                ).stdout

            press_and_roll = touched(1).strip().splitlines()
            rolling_alone = touched(5).strip().splitlines()
            self.assertTrue(all(row == "0" for row in press_and_roll),
                            "mode 1 must still need a press")
            self.assertIn("1", rolling_alone,
                          "mode 5 must enter from rolling with no press")


if __name__ == "__main__":
    unittest.main()
