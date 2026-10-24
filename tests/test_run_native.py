"""Reject unsafe launcher combinations before touching private input files."""
import contextlib
import io
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import run_native


class NativeLaunchGuards(unittest.TestCase):
    def refuses_before_intake(self, arguments):
        with patch.object(run_native, "fingerprint") as fingerprint:
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as caught:
                run_native.main(arguments)
            self.assertEqual(caught.exception.code, 2)
            fingerprint.assert_not_called()

    def test_firmware_modes_require_strict_iop(self):
        for mode in ("--ee-firmware-init", "--ee-firmware-entry"):
            with self.subTest(mode=mode):
                self.refuses_before_intake(["--input-complete", mode])

    def test_firmware_modes_cannot_be_combined(self):
        self.refuses_before_intake(["--input-complete", "--iop-bootstrap",
                                   "--ee-firmware-init", "--ee-firmware-entry"])

    def test_entry_requires_completed_input(self):
        self.refuses_before_intake(["--iop-bootstrap", "--ee-firmware-entry"])

    def test_video_events_require_firmware_game_entry(self):
        for arguments in (["--input-complete", "--iop-video-events"],
                          ["--input-complete", "--iop-bootstrap", "--ee-firmware-init", "--iop-video-events"]):
            with self.subTest(arguments=arguments):
                self.refuses_before_intake(arguments)

    def test_virtual_pad_requires_iop_video_events(self):
        self.refuses_before_intake(["--input-complete", "--iop-bootstrap", "--ee-firmware-entry", "--virtual-pad"])

    def test_nondefault_runtime_budget_requires_entry(self):
        for preset in ("512", "1024"):
            with self.subTest(preset=preset):
                self.refuses_before_intake(["--input-complete", "--iop-runtime-millions", preset])

    def test_write_watch_requires_firmware_entry(self):
        self.refuses_before_intake(["--input-complete", "--ee-write-watch", "0x11fe70"])

    def test_write_watch_address_is_aligned_and_bounded(self):
        for address in ("0x11fe71", str(32 * 1024 * 1024 - 7), str(32 * 1024 * 1024)):
            with self.subTest(address=address):
                self.refuses_before_intake(["--input-complete", "--iop-bootstrap", "--ee-firmware-entry",
                                            "--ee-write-watch", address])

    def test_write_watch_arm_value_requires_watch_and_is_32_bit(self):
        self.refuses_before_intake(["--input-complete", "--iop-bootstrap", "--ee-firmware-entry",
                                    "--ee-write-watch-arm-value", "1"])
        self.refuses_before_intake(["--input-complete", "--iop-bootstrap", "--ee-firmware-entry",
                                    "--ee-write-watch", "0x11fe70", "--ee-write-watch-arm-value", "0x100000000"])


if __name__ == "__main__":
    unittest.main()
