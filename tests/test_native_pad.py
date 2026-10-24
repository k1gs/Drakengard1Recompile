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
