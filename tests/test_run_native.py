"""Reject unsafe launcher combinations before touching private input files."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import run_native


class NativeLaunchGuards(unittest.TestCase):
    def test_gs_metrics_are_opt_in_and_confined_to_fresh_run(self):
        for enabled in (False, True):
            with self.subTest(enabled=enabled), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                disc = root / "synthetic.iso"
                disc.write_bytes(b"synthetic launcher fixture; never executed")
                (root / "work").mkdir()
                def fingerprint(path):
                    return {"size": 1, "sha256": run_native.ELF_SHA256 if path.name == "SLES_523.22" else "0" * 64}
                with patch.object(run_native, "ROOT", root), patch.object(run_native, "fingerprint", side_effect=fingerprint), \
                     patch.object(run_native.subprocess, "run", return_value=SimpleNamespace(returncode=0)) as child, \
                     patch.dict(run_native.os.environ, {"DRAKENGARD_GS_METRICS_OUTPUT": "unrelated-existing-file"}), \
                     contextlib.redirect_stdout(io.StringIO()):
                    args = ["--input-complete", "--seconds", "1", "--disc", str(disc)]
                    if enabled:
                        args.append("--gs-metrics")
                    self.assertEqual(run_native.main(args), 0)
                environment = child.call_args.kwargs["env"]
                if enabled:
                    self.assertEqual(environment["DRAKENGARD_GS_METRICS_OUTPUT"], str(root / "work/native-run-001/gs-metrics.jsonl"))
                else:
                    self.assertNotIn("DRAKENGARD_GS_METRICS_OUTPUT", environment)
                result = json.loads((root / "work/native-run-001/result.json").read_text())
                self.assertEqual(result["gs_metrics_experiment"], enabled)

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

    def test_pad_control_requires_opt_in_virtual_pad(self):
        self.refuses_before_intake(["--input-complete", "--pad-control"])

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
