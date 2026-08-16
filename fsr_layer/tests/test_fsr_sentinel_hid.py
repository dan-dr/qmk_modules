#!/usr/bin/env python3
"""Host-compile and run the FSR Sentinel Raw HID protocol test."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from paths import MODULE, TESTS


class FsrSentinelHidTest(unittest.TestCase):
    def test_protocol_and_bounds(self) -> None:
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "host C compiler is required")
        with tempfile.TemporaryDirectory(prefix="fsr-hid-") as directory:
            executable = Path(directory) / "hid-test"
            subprocess.run(
                [
                    str(compiler),
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-DFSR_ENABLE",
                    "-DFSR_SENTINEL_NVM_DISABLE",
                    f"-I{MODULE}",
                    f"-I{TESTS}",
                    str(MODULE / "fsr_sentinel.c"),
                    str(MODULE / "fsr_atlas_phase.c"),
                    str(MODULE / "fsr_sentinel_runtime.c"),
                    str(MODULE / "fsr_sentinel_hid.c"),
                    str(TESTS / "fsr_sentinel_hid_test.c"),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            result = subprocess.run(
                [str(executable)], text=True, capture_output=True, check=True
            )
        self.assertEqual(result.stdout, "fsr_sentinel_hid_test: ok\n")


if __name__ == "__main__":
    unittest.main()
