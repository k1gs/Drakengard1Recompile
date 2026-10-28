"""The host controller writer must stay bounded and release its exclusive lock."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import native_pad


class HostControllerWriter(unittest.TestCase):
    def test_atomic_commands_release_and_sequence(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)):
            run = Path(folder) / "work/native-run-001"
            run.mkdir(parents=True)
            target = run / "pad-control.txt"
            target.write_text("D1PAD1 0 FFFF 0\n", encoding="ascii")
            self.assertEqual(native_pad.send(1, 0xff7f, 500)["sequence"], 1)
            self.assertEqual(target.read_text(), "D1PAD1 1 FF7F 500\n")
            self.assertEqual(native_pad.send(1, 0xffff, 0)["sequence"], 2)
            self.assertFalse((run / "pad-control.lock").exists())
            events = [json.loads(line) for line in (run / "pad-control-events.jsonl").read_text().splitlines()]
            self.assertEqual([event["buttons"] for event in events], ["FF7F", "FFFF"])

    def test_does_not_create_an_opt_in_input_or_steal_a_lock(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)):
            run = Path(folder) / "work/native-run-001"
            run.mkdir(parents=True)
            with self.assertRaises(FileNotFoundError):
                native_pad.send(1, 0xff7f, 500)
            self.assertFalse((run / "pad-control.lock").exists())
            lock = run / "pad-control.lock"
            lock.write_text("other writer")
            with self.assertRaises(FileExistsError):
                native_pad.send(1, 0xff7f, 500)
            self.assertEqual(lock.read_text(), "other writer")

    def test_rejects_unbounded_commands(self):
        for arguments in ((0, 0xffff, 0), (1, 0x10000, 1), (1, 0xff7f, 0), (1, 0xff7f, 10001)):
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                native_pad.send(*arguments)

    def test_refuses_an_ended_run_without_recording_an_input(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)):
            run = Path(folder) / "work/native-run-001"
            run.mkdir(parents=True)
            target = run / "pad-control.txt"
            target.write_text("D1PAD1 7 FFFF 0\n", encoding="ascii")
            (run / "result.json").write_text("{}")
            with self.assertRaisesRegex(ValueError, "has ended"):
                native_pad.send(1, 0xffff, 1000, (127, 127, 127, 0))
            self.assertEqual(target.read_text(), "D1PAD1 7 FFFF 0\n")
            self.assertFalse((run / "pad-control-events.jsonl").exists())
            self.assertFalse((run / "pad-control.lock").exists())

    def test_analog_commands_and_digital_release_share_sequence(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(native_pad, "ROOT", Path(folder)):
            run=Path(folder)/"work/native-run-001"
            run.mkdir(parents=True)
            target=run/"pad-control.txt"
            target.write_text("D1PAD1 0 FFFF 0\n",encoding="ascii")
            event=native_pad.send(1,0xffff,3000,(0,255,42,127))
            self.assertEqual(event["axes_rx_ry_lx_ly"],[0,255,42,127])
            self.assertEqual(target.read_text(),"D1PAD2 1 FFFF 3000 00FF2A7F\n")
            self.assertEqual(native_pad.send(1,0xffff,0)["sequence"],2)
            self.assertEqual(target.read_text(),"D1PAD1 2 FFFF 0\n")
            self.assertFalse((run/"pad-control.lock").exists())

    def test_rejects_invalid_axes_before_touching_files(self):
        for axes in [(0,0,0),(-1,127,127,127),(256,127,127,127),(1.0,127,127,127)]:
            with self.subTest(axes=axes),self.assertRaises(ValueError):
                native_pad.send(1,0xffff,100,axes)
        with self.assertRaises(ValueError):
            native_pad.send(1,0xffff,0,(127,127,127,0))
