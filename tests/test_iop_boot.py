"""Synthetic boot configuration/version selection contracts, without firmware."""
from pathlib import Path
import contextlib
import io
import struct
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from prepare_iop_boot import boot_order, select_modules, main


class IopBootTests(unittest.TestCase):
    def test_hex_start_and_order_are_explicit(self):
        self.assertEqual(boot_order(b'# test\n@800\nSYSMEM\nLOADCORE\nSIFCMD\n'),
                         (0x800, ['SYSMEM', 'LOADCORE', 'SIFCMD']))
        for config in [b'SYSMEM\nLOADCORE', b'@801\nSYSMEM\nLOADCORE',
                       b'@800\nLOADCORE\nSYSMEM', b'@800\nSYSMEM\nLOADCORE\n!include x',
                       b'@800\nSYSMEM\nLOADCORE\nSYSMEM', b'@800\n@900\nSYSMEM\nLOADCORE']:
            with self.subTest(config=config), self.assertRaises(ValueError):
                boot_order(config)

    def test_versions_choose_newest_and_later_image_on_ties(self):
        def image(revision):
            data = struct.pack('<HBB', revision, 0, 2)
            entry = {'name': 'SIFCMD', 'extinfo_offset': 0, 'extinfo_size': 4, 'elf': {}}
            return data, [{'name': 'RESET'}, {'name': 'ROMDIR'}, {'name': 'EXTINFO', 'offset': 0}, entry]
        images = [image(0x201), image(0x200)]
        self.assertEqual(select_modules(images, ['SIFCMD'])[0][:2], (0x201, 0))
        images[1] = image(0x201)
        self.assertEqual(select_modules(images, ['SIFCMD'])[0][:2], (0x201, 1))
        with self.assertRaises(ValueError):
            select_modules(images, ['MISSING'])
        images[1] = (struct.pack('<HBB', 0x202, 4, 2), images[1][1])
        with self.assertRaises(ValueError):
            select_modules(images, ['SIFCMD'])

    def test_boot_bundle_requires_opt_in_and_private_output_before_read(self):
        args = ['--base-rom', 'missing.bin', '--base-romdir-offset', '0',
                '--update', 'missing.img', '--output', 'public-output']
        with patch.object(Path, 'read_bytes', side_effect=AssertionError('input accessed')):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                main(args)
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(['--input-complete'] + args), 2)


if __name__ == '__main__':
    unittest.main()
