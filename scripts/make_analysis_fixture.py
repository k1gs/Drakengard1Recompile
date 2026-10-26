"""Original synthetic ELFs for Ghidra pointer and shared-epilogue regressions."""
import argparse
import struct
from intake import ROOT
from kernel_image import translation_elf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['pointers', 'shared-epilogue', 'long-nop', 'delay-slot-exit',
                                        'call-slot-exit'])
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
    elif args.kind == 'shared-epilogue':
        # beq zero,zero,0x1010; nop; eight-byte gap; return42.
        image = translation_elf([(0x1000, struct.pack('<4I', 0x10000003, 0, 0xFFFFFFFF, 0xFFFFFFFF)
                                 + return42)], 0x1000)
    elif args.kind == 'long-nop':
        # A real fallthrough through 64 original NOPs, beyond automatic cutoff.
        image = translation_elf([(0x1000, struct.pack('<I', 0x2402002a) + bytes(64 * 4) +
                                  struct.pack('<2I', 0x03e00008, 0))], 0x1000)
    elif args.kind == 'call-slot-exit':
        # A split caller with an alternate entry into a jump's delay slot,
        # followed by a missing word and a real call outside its analysis bound.
        words = (0x03e08021, 0x18800003, 0x2484ffff, 0, 0x08000401,
                 0x24630001, 0x2402000a, 0x0c000800, 0x24630002,
                 0x0200f821, 0x03e00008, 0x24420001)
        callee = (0x24420014, 0x03e00008, 0x24630004)
        image = translation_elf([(0x1000, struct.pack('<12I', *words)),
                                 (0x2000, struct.pack('<3I', *callee))], 0x1000)
    else:
        # Exit branches directly into a backward jump's delay slot. That slot
        # must then fall through into the return, rather than end the function.
        words = (0x18800004, 0x2484ffff, 0, 0, 0x08000400,
                 0x24630001, 0x2402002a, 0x03e00008, 0x24420001)
        image = translation_elf([(0x1000, struct.pack('<9I', *words))], 0x1000)
    directory = ROOT / 'work' / f'analysis-{args.kind}-001'
    directory.mkdir(exist_ok=False)
    path = directory / f'{args.kind}.elf'
    path.write_bytes(image)
    print(path)


if __name__ == '__main__':
    main()
