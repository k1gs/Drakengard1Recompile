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
from reference_state import hardware_seed, ee_dma_config, main


def fixture(value=7, version=b'v2.8.2', hardware_size=65536):
    output = io.BytesIO()
    hardware = bytearray(hardware_size)
    struct.pack_into('<I', hardware, 0x1450, value)
    struct.pack_into('<I', hardware, 0x1578, 1)
    with zipfile.ZipFile(output, 'w') as archive:
        archive.writestr('PCSX2 Savestate Version.id', struct.pack('<I', 0x9a590000) + version.ljust(32, b'\0'))
        archive.writestr('iopHwRegs.bin', hardware)
    return output.getvalue()


def ee_fixture(control=1, version=b'v2.8.2', size=65536, enable=0x1201, alias=0x1201):
    output = io.BytesIO()
    hardware = bytearray(b'\x55' * size)  # Active fields must not escape this fixture.
    struct.pack_into('<I', hardware, 0xe000, control)
    struct.pack_into('<I', hardware, 0xf520, enable)
    struct.pack_into('<I', hardware, 0xf590, alias)
    with zipfile.ZipFile(output, 'w') as archive:
        archive.writestr('PCSX2 Savestate Version.id', struct.pack('<I', 0x9a590000) + version.ljust(32, b'\0'))
        archive.writestr('eeHwRegs.bin', hardware)
    return output.getvalue()


class ReferenceStateTests(unittest.TestCase):
    def test_ee_profile_exports_only_initialized_master_gate(self):
        control, enable, digest = ee_dma_config(ee_fixture())
        self.assertEqual(control, 1)
        self.assertEqual(enable, 0x1201)
        self.assertEqual(len(digest), 64)

    def test_ee_profile_rejects_disabled_and_unreviewed_modes(self):
        for control in (0, 3, 0x10001):
            with self.subTest(control=control), self.assertRaises(ValueError):
                ee_dma_config(ee_fixture(control))
        for parameters in ({'enable': 0x11201}, {'alias': 0}, {'enable': 0}):
            with self.subTest(parameters=parameters), self.assertRaises(ValueError):
                ee_dma_config(ee_fixture(**parameters))

    def test_ee_profile_checks_snapshot_layout(self):
        for data in (fixture(), ee_fixture(version=b'v2.9.0'), ee_fixture(size=65540)):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                ee_dma_config(data)
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
