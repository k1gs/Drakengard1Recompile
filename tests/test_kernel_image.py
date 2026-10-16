"""Synthetic relocation-image checks; no game bytes or installed reverse tools."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import kernel_image
from intake import inspect_elf
from make_smoke_fixture import elf_bytes


class KernelImageTests(unittest.TestCase):
    def test_regions_keep_virtual_addresses_and_bytes(self):
        regions = [(0x80001000, bytes.fromhex('2a0002240800e00300000000')),
                   (0x82000, bytes.fromhex('01000224'))]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'translation.elf'
            image = kernel_image.translation_elf(regions, 0x80001000)
            path.write_bytes(image)
            metadata = inspect_elf(path)
            self.assertEqual(metadata['entry'], 0x80001000)
            self.assertEqual(len(metadata['load_segments']), 2)
            for segment, (address, code) in zip(metadata['load_segments'], sorted(regions)):
                self.assertEqual(segment['vaddr'], address)
                self.assertEqual(segment['filesz'], len(code))
                self.assertEqual(segment['flags'], 5)
                self.assertEqual(image[segment['offset']:segment['offset'] + len(code)], code)

    def test_rejects_overlap_unaligned_or_unmapped_entry(self):
        for regions, entry in [([], 0), ([(0x1001, bytes(4))], 0x1001),
                               ([(0x1000, bytes(8)), (0x1004, bytes(4))], 0x1000),
                               ([(0x1000, bytes(4))], 0x2000)]:
            with self.subTest(regions=regions), self.assertRaises(ValueError):
                kernel_image.translation_elf(regions, entry)

    def test_no_input_access_without_opt_in(self):
        with patch.object(kernel_image, 'inspect_elf', side_effect=AssertionError('touched input')):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                kernel_image.main([])

    def test_wrong_elf_cannot_create_translation_image(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf = root / 'work/extracted-001/SLES_523.22'
            elf.parent.mkdir(parents=True)
            elf.write_bytes(elf_bytes())
            with patch.object(kernel_image, 'ROOT', root), self.assertRaises(ValueError):
                kernel_image.main(['--input-complete'])
            self.assertFalse((root / 'work/kernel-image-001').exists())


if __name__ == '__main__':
    unittest.main()
