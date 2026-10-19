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


if __name__ == "__main__":
    unittest.main()
