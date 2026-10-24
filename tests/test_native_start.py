"""Bounded no-card startup controller tests; no native process or OS input."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import native_pad
import native_start


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class NativeStartupController(unittest.TestCase):
    def make_run(self, folder, run=1, log=b"", opted_in=True):
        directory = Path(folder) / "work" / f"native-run-{run:03d}"
        directory.mkdir(parents=True)
        if opted_in:
            (directory / "pad-control.txt").write_text("D1PAD1 0 FFFF 0\n", encoding="ascii")
        if log:
            (directory / "runtime.log").write_bytes(log)
        return directory

    def test_rejects_missing_opt_in_and_ended_run_without_send(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.native_pad, "send") as send:
            directory = self.make_run(folder, opted_in=False)
            with self.assertRaises(ValueError):
                native_start.advance_startup(1, after_frame=0, timeout=1)
            send.assert_not_called()

            (directory / "pad-control.txt").write_text("D1PAD1 0 FFFF 0\n", encoding="ascii")
            (directory / "result.json").write_text(json.dumps({"exit_code": 6}), encoding="ascii")
            with self.assertRaises(ValueError):
                native_start.advance_startup(1, after_frame=0, timeout=1)
            send.assert_not_called()

    def test_already_at_ipu_sends_no_input(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.native_pad, "send") as send:
            self.make_run(folder, log=b"[diagnostic] frame=1800 pc=1\r\n[ee-ipu-command] 0x2c010000\r\n")
            self.assertTrue(native_start.advance_startup(1, after_frame=1800, timeout=1))
            send.assert_not_called()

    def test_crlf_utf8_offsets_reports_and_ipu_stop(self):
        clock = FakeClock()
        calls = []
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.time, "monotonic", side_effect=clock.monotonic), \
                patch.object(native_start.time, "sleep", side_effect=lambda seconds: self._sleep_and_finish(
                    clock, seconds, calls, Path(folder))), \
                patch.object(native_start.native_pad, "send", side_effect=lambda run, mask, hold:
                    self._send_report(Path(folder), calls, run, mask, hold)) as send:
            directory = self.make_run(folder, log="запуск\r\n[diagnostic] frame=1800 pc=1\r\n".encode("utf-8"))
            self.assertTrue(native_start.advance_startup(1, after_frame=1800, timeout=5, attempts=3))
            self.assertEqual([entry[1] for entry in calls if entry[0] == "send"], [0xff7f, 0xbfff])
            self.assertEqual(send.call_count, 2)
            self.assertIn(b"[ee-ipu-command] 0x2c010000", directory.joinpath("runtime.log").read_bytes())

    @staticmethod
    def _send_report(root, calls, run, mask, hold):
        calls.append(("send", mask, hold))
        directory = root / "work" / f"native-run-{run:03d}"
        with directory.joinpath("runtime.log").open("ab") as stream:
            stream.write(f"[sio2-pad-input-report] buttons=0x{mask:04x}\r\n".encode("ascii"))
            stream.write(b"[sio2-pad-input-report] buttons=0xffff\r\n")
        return {"sequence": len([entry for entry in calls if entry[0] == "send"])}

    @staticmethod
    def _sleep_and_finish(clock, seconds, calls, root):
        clock.now += seconds
        # The second (Cross) release is accepted before the next pause sees
        # the decoder marker, proving no third button is sent.
        if len([entry for entry in calls if entry[0] == "send"]) == 2 and clock.now >= 0.45:
            directory = root / "work" / "native-run-001"
            log = directory.joinpath("runtime.log")
            if b"[ee-ipu-command]" not in log.read_bytes():
                with log.open("ab") as stream:
                    stream.write(b"[ee-ipu-command] 0x2c010000\r\n")

    def test_missing_reports_and_timeout_are_bounded(self):
        clock = FakeClock()
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.time, "monotonic", side_effect=clock.monotonic), \
                patch.object(native_start.time, "sleep", side_effect=clock.sleep), \
                patch.object(native_start.native_pad, "send", return_value={"sequence": 1}) as send:
            self.make_run(folder, log=b"[diagnostic] frame=1800 pc=1\r\n")
            with self.assertRaisesRegex(ValueError, "did not report left"):
                native_start.advance_startup(1, after_frame=1800, timeout=20, attempts=1)
            send.assert_called_once()

        clock = FakeClock()
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.native_pad, "ROOT", Path(folder)), \
                patch.object(native_start.time, "monotonic", side_effect=clock.monotonic), \
                patch.object(native_start.time, "sleep", side_effect=clock.sleep), \
                patch.object(native_start.native_pad, "send") as send:
            self.make_run(folder)
            with self.assertRaisesRegex(ValueError, "time limit"):
                native_start.advance_startup(1, after_frame=1800, timeout=1)
            send.assert_not_called()


if __name__ == "__main__":
    unittest.main()
