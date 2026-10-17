"""Own synthetic snapshot components; never save real firmware or state in tests."""
import contextlib
import io
from pathlib import Path
import struct
import sys
import unittest
import zipfile
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from reference_state import hardware_seed, main


def fixture(value=7, version=b'v2.8.2', hardware_size=65536):
    output = io.BytesIO()
    hardware = bytearray(hardware_size)
    struct.pack_into('<I', hardware, 0x1450, value)
    struct.pack_into('<I', hardware, 0x1578, 1)
    with zipfile.ZipFile(output, 'w') as archive:
        archive.writestr('PCSX2 Savestate Version.id', struct.pack('<I', 0x9a590000) + version.ljust(32, b'\0'))
        archive.writestr('iopHwRegs.bin', hardware)
    return output.getvalue()


class ReferenceStateTests(unittest.TestCase):
    def test_exact_passive_register_without_active_device_state(self):
        seed, metadata = hardware_seed(fixture(0x12345678))
        self.assertEqual(seed, b'D1HW001\0' + struct.pack('<IIIII', 2, 0x1f801450, 0x12345678, 0x1f801578, 1))
        self.assertEqual(metadata['hw_icfg'], 0x12345678)

    def test_rejects_unknown_layouts_and_wrong_sizes(self):
        for data in (fixture(version=b'v2.9.0'), fixture(hardware_size=65540), b'not an archive'):
            with self.subTest(size=len(data)), self.assertRaises((ValueError, zipfile.BadZipFile)):
                hardware_seed(data)

    def test_opt_in_and_private_output_before_read(self):
        args = ['--state', 'missing.p2s', '--expected-sha256', '0' * 64, '--output', 'public-output']
        with patch.object(Path, 'read_bytes', side_effect=AssertionError('input accessed')):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                main(args)
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(['--input-complete'] + args), 2)


if __name__ == '__main__':
    unittest.main()
