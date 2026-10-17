"""Original synthetic ELFs for Ghidra pointer and shared-epilogue regressions."""
import argparse
import struct
from intake import ROOT
from kernel_image import translation_elf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['pointers', 'shared-epilogue'])
    args = parser.parse_args()
    return42 = bytes.fromhex('2a0002240800e00300000000')
    if args.kind == 'pointers':
        image = bytearray(translation_elf([
            (0x1000, return42), (0x2000, return42),
            (0x3000, struct.pack('<5I', 0x2000, 0x3000, 0x2001, 0x12345678, 0x1004))], 0x1000))
        # The pointer table is writable data, never executable code.
        struct.pack_into('<I', image, 52 + 2 * 32 + 24, 6)
        sections = struct.unpack_from('<I', image, 32)[0]
        struct.pack_into('<I', image, sections + 3 * 40 + 8, 3)
    else:
        # beq zero,zero,0x1010; nop; eight-byte gap; return42.
        image = translation_elf([(0x1000, struct.pack('<4I', 0x10000003, 0, 0xFFFFFFFF, 0xFFFFFFFF)
                                 + return42)], 0x1000)
    directory = ROOT / 'work' / f'analysis-{args.kind}-001'
    directory.mkdir(exist_ok=False)
    path = directory / f'{args.kind}.elf'
    path.write_bytes(image)
    print(path)


if __name__ == '__main__':
    main()
