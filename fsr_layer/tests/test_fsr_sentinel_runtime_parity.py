#!/usr/bin/env python3
"""Prove v2/v3 firmware integer behavior matches the frozen lab models."""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from paths import MODULE, find_userspace


class FsrSentinelRuntimeParityTest(unittest.TestCase):
    def test_successors_match_lab_on_real_and_synthetic_rows(self) -> None:
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "host C compiler is required")
        userspace = find_userspace()
        if userspace is None:
            self.skipTest("qmk_userspace checkout is not available")
        lab = userspace / "tools/fsr-sentinel-lab"
        if not lab.is_dir():
            self.skipTest("FSR Sentinel lab is not available")
        sys.path.insert(0, str(lab))
        from common import load_new_samples, load_run7_samples
        from detectors import V2Detector, V2Params, V3Detector, V3Params
        from synthetic import locked as synthetic_locked

        samples = load_new_samples() + load_run7_samples() + synthetic_locked()
        replay_input = "".join(
            f"{row.reading},{row.t_ms},{row.sequence},{row.motion_x},{row.motion_y}\n"
            for row in samples
        )

        cases = (
            (2, V2Detector(V2Params())),
            (3, V3Detector(V3Params())),
        )
        with tempfile.TemporaryDirectory(prefix="fsr-runtime-parity-") as directory:
            executable = Path(directory) / "runtime-parity"
            subprocess.run(
                [
                    str(compiler),
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-DFSR_SENTINEL_NVM_DISABLE",
                    f"-I{MODULE}",
                    str(MODULE / "fsr_sentinel.c"),
                    str(MODULE / "fsr_atlas_phase.c"),
                    str(MODULE / "fsr_sentinel_runtime.c"),
                    str(MODULE / "tests/fsr_sentinel_runtime_runner.c"),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            for algorithm, detector in cases:
                expected = []
                for sample in samples:
                    touched = detector.step(sample)
                    expected.append(
                        f"{int(touched)},{detector.state},"
                        f"{detector.center_q8 >> 8},"
                        f"{detector.touch_evidence_ms},"
                        f"{detector.release_evidence_ms}\n"
                    )
                result = subprocess.run(
                    [str(executable), str(algorithm)],
                    input=replay_input,
                    text=True,
                    capture_output=True,
                    check=True,
                )
                self.assertEqual(result.stdout, "".join(expected))


if __name__ == "__main__":
    unittest.main()
