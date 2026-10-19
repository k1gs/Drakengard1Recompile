import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from prepare_firmware import verify_sif_syscall_table


class FirmwareTableTests(unittest.TestCase):
    def test_each_binding_requires_its_own_exact_slot(self):
        # Synthetic table only; no firmware bytes are distributed.
        entries = {0x6b: 0x80006380, 0x76: 0x80006958, 0x77: 0x800067a0, 0x78: 0x80006348}
        table = bytearray(0x15100)
        for number, address in entries.items():
            slot = 0x14e80 + number * 4
            table[slot:slot + 4] = address.to_bytes(4, 'little')
        verify_sif_syscall_table(table)
        for number, address in entries.items():
            with self.subTest(number=number):
                wrong = bytearray(table)
                slot = 0x14e80 + number * 4
                wrong[slot:slot + 4] = (address + 4).to_bytes(4, 'little')
                with self.assertRaises(ValueError):
                    verify_sif_syscall_table(wrong)
                with self.assertRaises(ValueError):
                    verify_sif_syscall_table(table[:slot + 3])


if __name__ == '__main__':
    unittest.main()
