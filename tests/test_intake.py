"""Synthetic-only fixtures. No game bytes or BIOS required."""
import contextlib
import hashlib
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import intake
from check_public_tree import allowed_path


def synthetic_elf():
    ident = b"\x7fELF\x01\x01\x01" + bytes(9)
    header = struct.pack("<HHIIIIIHHHHHH", 2, 8, 1, 0x1000, 52, 0, 0,
                         52, 32, 1, 0, 0, 0)
    segment = struct.pack("<IIIIIIII", 1, 84, 0x1000, 0x1000, 4, 16, 5, 4)
    return ident + header + segment + bytes(4)


class IntakeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.elf = self.root / "SLES_523.22"
        self.elf.write_bytes(synthetic_elf())
        (self.root / "SYSTEM.CNF").write_text("BOOT2 = cdrom0:\\SLES_523.22;1\nVER = 1.00\n",
                                                    encoding="ascii")

    def tearDown(self):
        self.temp.cleanup()

    def test_fingerprint_and_no_mutation(self):
        before = self.elf.read_bytes()
        result = intake.inspect_tree(self.root)
        self.assertEqual(result["elf"]["sha256"], hashlib.sha256(before).hexdigest())
        self.assertEqual(result["elf"]["entry"], 0x1000)
        self.assertEqual(result["elf"]["load_segments"][0]["memsz"], 16)
        self.assertEqual(self.elf.read_bytes(), before)
        self.assertFalse(result["revision_verified"])

    def test_wrong_machine_endian_and_truncation(self):
        for offset, value in ((18, 3), (5, 2)):
            data = bytearray(synthetic_elf())
            data[offset] = value
            self.elf.write_bytes(data)
            with self.assertRaises(ValueError):
                intake.inspect_elf(self.elf)
        self.elf.write_bytes(synthetic_elf()[:60])
        with self.assertRaises(ValueError):
            intake.inspect_elf(self.elf)

    def test_segment_and_entry_bounds(self):
        for offset, value in ((52 + 16, 1000), (52 + 20, 1), (24, 0x2000),
                              (52 + 24, 4), (52 + 28, 3)):
            data = bytearray(synthetic_elf())
            struct.pack_into("<I", data, offset, value)
            self.elf.write_bytes(data)
            with self.assertRaises(ValueError):
                intake.inspect_elf(self.elf)

    def test_boot_path_wrong_region_traversal_duplicate(self):
        for cnf in ("BOOT2 = cdrom0:\\SLUS_207.32;1", "BOOT2 = cdrom0:\\..\\SLES_523.22;1",
                    "BOOT2 = cdrom0:\\C:\\SLES_523.22;1", "no boot",
                    "BOOT2 = cdrom0:\\SLES_523.22;1\nBOOT2 = cdrom0:\\SLES_523.22;1"):
            with self.assertRaises(ValueError):
                intake.boot_path(self.root, cnf)

    def test_case_insensitive_boot(self):
        self.assertEqual(intake.boot_path(self.root, "boot2 = cdrom0:\\sles_523.22;1"),
                         self.elf.resolve())

    def test_opt_in_before_any_input_access(self):
        with patch.object(intake, "fingerprint", side_effect=AssertionError("input touched")):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                intake.main(["image", "--input", "missing.iso", "--output", "work/x.json"])

    def test_cli_output_and_no_overwrite(self):
        with patch.object(intake, "ROOT", self.root):
            image = self.root / "synthetic.iso"
            image.write_bytes(b"synthetic fingerprint fixture, not an ISO")
            output = self.root / "work" / "report.json"
            args = ["image", "--input-complete", "--input", str(image), "--output", str(output)]
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(intake.main(args), 0)
                before = output.read_bytes()
                self.assertEqual(intake.main(args), 2)
                self.assertEqual(output.read_bytes(), before)
                self.assertEqual(intake.main(args[:-1] + [str(self.root / "public.json")]), 2)

    def test_public_allowlist(self):
        for name in ("README.md", "scripts/intake.py", "docs/ROADMAP.md", ".github/workflows/ci.yml"):
            self.assertTrue(allowed_path(name), name)
        for name in ("private/game.iso", "generated/lift.cpp", "docs/game.elf", "work/report.json", ".env"):
            self.assertFalse(allowed_path(name), name)


if __name__ == "__main__":
    unittest.main()
