"""Original synthetic ROMFS; no copyrighted module or firmware input."""
import contextlib
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import romdir


def fixture():
    table = b''.join(struct.pack('<10sHI', name, ext, size) for name, ext, size in
                     [(b'RESET', 4, 0), (b'ROMDIR', 4, 80), (b'EXTINFO', 0, 12),
                      (b'TEST', 4, 3)]) + bytes(16)
    return table + bytes(16) + b'abc'


class RomdirTests(unittest.TestCase):
    def test_offsets_sizes_and_hashes_follow_actual_bytes(self):
        image = fixture()
        entries = romdir.inspect_romfs(image)
        self.assertEqual([entry['name'] for entry in entries], ['RESET', 'ROMDIR', 'EXTINFO', 'TEST'])
        self.assertEqual(entries[3]['offset'], 96)
        self.assertEqual(entries[3]['size'], 3)
        self.assertEqual(entries[3]['extinfo_offset'], 8)
        self.assertEqual(entries[3]['sha256'], romdir.hashlib.sha256(b'abc').hexdigest())

    def test_rejects_missing_sections_terminators_extents_and_bad_names(self):
        cases = [fixture()[:-1], fixture()[:63]]
        for offset, value in [(0, b'BAD'), (16 + 12, struct.pack('<I', 81)),
                              (32 + 12, struct.pack('<I', 8)), (48, b'../TEST'),
                              (48, b'ROMDIR'), (48 + 10, b'\x03\0'),
                              (64, b'X'), (12, struct.pack('<I', 16))]:
            image = bytearray(fixture()); image[offset:offset + len(value)] = value
            cases.append(image)
        for image in cases:
            with self.subTest(size=len(image)), self.assertRaises(ValueError):
                romdir.inspect_romfs(image)

    def test_explicit_nonzero_romdir_requires_matching_reset_extent(self):
        image = bytearray(bytes(16) + fixture())
        struct.pack_into('<I', image, 16 + 12, 16)
        entries = romdir.inspect_romfs(image, 16)
        self.assertEqual(entries[1]['offset'], 16)
        self.assertEqual(entries[3]['offset'], 112)
        for offset in (-16, 1, 32, len(image)):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                romdir.inspect_romfs(image, offset)

    def test_anonymous_padding_can_repeat_without_aliasing_named_files(self):
        table = b''.join(struct.pack('<10sHI', name, ext, size) for name, ext, size in
                         [(b'RESET', 4, 0), (b'ROMDIR', 4, 96), (b'EXTINFO', 0, 12),
                          (b'-', 4, 3), (b'-', 0, 4)]) + bytes(16)
        image = table + bytes(16) + b'abc' + bytes(13) + b'defg'
        entries = romdir.inspect_romfs(image)
        self.assertEqual([(e['offset'], e['size']) for e in entries if e['name'] == '-'],
                         [(112, 3), (128, 4)])

    def test_opt_in_and_output_boundary_before_input_access(self):
        with patch.object(Path, 'read_bytes', side_effect=AssertionError('input accessed')):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                romdir.main(['--image', 'missing.img', '--output', 'work/test'])
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(romdir.main(['--input-complete', '--image', 'missing.img',
                                             '--output', 'public-output']), 2)

    def test_private_extract_is_exact_and_never_overwrites(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(romdir, 'ROOT', Path(directory)):
            image = Path(directory) / 'input.img'; image.write_bytes(fixture())
            output = Path(directory) / 'work/experiment'
            args = ['--input-complete', '--image', str(image), '--output', str(output), '--extract']
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(romdir.main(args), 0)
            self.assertEqual((output / 'TEST').read_bytes(), b'abc')
            self.assertEqual(image.read_bytes(), fixture())
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(romdir.main(args), 2)


if __name__ == '__main__':
    unittest.main()
