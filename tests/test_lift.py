"""Synthetic validation for the exact-input lift guard, without installed toolchain."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import lift
import run_native
from intake import inspect_elf
from make_smoke_fixture import elf_bytes


class LiftTests(unittest.TestCase):
    def test_smoke_fixture_is_valid_executable(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "return42.elf"
            path.write_bytes(elf_bytes())
            info = inspect_elf(path)
            self.assertEqual(info["entry"], 0x1000)
            self.assertEqual(info["load_segments"][0]["filesz"], 12)

    def test_no_access_without_completion_opt_in(self):
        with patch.object(lift, "inspect_elf", side_effect=AssertionError("touched input")):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                lift.main([])

    def test_wrong_revision_does_not_launch_tools(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "return42.elf"
            path.write_bytes(elf_bytes())
            with patch.object(lift.subprocess, "check_output", side_effect=AssertionError("launched tool")):
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(lift.main(["--input-complete", "--input", str(path)]), 2)

    def test_run_id_cannot_escape_local_output(self):
        with patch.object(lift, "inspect_elf", side_effect=AssertionError("touched input")):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                lift.main(["--input-complete", "--run-id", "../../public"])

    def test_native_does_not_touch_input_without_opt_in(self):
        with patch.object(run_native, "fingerprint", side_effect=AssertionError("touched input")):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                run_native.main([])

    def test_native_rejects_wrong_elf_before_starting_process(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf = root / "work/extracted-001/SLES_523.22"
            elf.parent.mkdir(parents=True)
            elf.write_bytes(elf_bytes())
            with patch.object(run_native, "ROOT", root), patch.object(
                run_native.subprocess, "run", side_effect=AssertionError("launched wrong game")
            ):
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(run_native.main(["--input-complete"]), 2)

    def test_map_rejects_bounding_interval_with_hole(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf = root / "fixture.elf"
            elf.write_bytes(elf_bytes())
            info = inspect_elf(elf)
            path = root / "map.csv"
            path.write_text("Name,Start,End,Size\nf,0x1000,0x100c,8\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                lift.validate_map(path, info)
            path.write_text("Name,Start,End,Size\nf,0x1000,0x1004,4\ng,0x1008,0x100c,4\n", encoding="utf-8")
            self.assertEqual(lift.validate_map(path, info), 2)
            path.write_text("Name,Start,End,Size\nf,0x1000,0x1008,8\ng,0x1004,0x100c,8\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                lift.validate_map(path, info)


if __name__ == "__main__":
    unittest.main()
