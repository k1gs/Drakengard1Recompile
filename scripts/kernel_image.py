"""Prepare a private translation image for an observed Europe kernel-code copy.

This image is analysis input only. It is never loaded instead of the original ELF.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from intake import ROOT, inspect_elf, fingerprint
from lift import ELF_SHA256

KERNEL_IMAGE_SHA256 = '11f5a7f8dfa209f87716e0ce6c70bfc24fa7fc5076b113e911666084298d302e'


def translation_elf(regions, entry):
    """Build ELF32/MIPS with one executable section per explicitly located region."""
    if not regions or len(regions) > 16:
        raise ValueError("Expected 1..16 regions")
    names = bytearray(b"\0.shstrtab\0")
    entries = []
    previous_end = 0
    for address, code in sorted(regions):
        if address % 4 or not code or len(code) % 4 or address < previous_end or address + len(code) > 2**32:
            raise ValueError("Invalid aligned, disjoint code region")
        previous_end = address + len(code)
        name_offset = len(names)
        names.extend(f".text_{address:08x}".encode() + b"\0")
        entries.append((address, code, name_offset))
    if entry % 4 or not any(address <= entry < address + len(code) for address, code, _ in entries):
        raise ValueError("Entry outside code")
    data = bytearray((52 + 32 * len(entries) + 255) & ~255)
    sections = [bytes(40)]
    for i, (address, code, name_offset) in enumerate(entries):
        offset = len(data)
        struct.pack_into('<8I', data, 52 + i * 32, 1, offset, address, address,
                         len(code), len(code), 5, 4)
        sections.append(struct.pack('<10I', name_offset, 1, 6, address, offset,
                                    len(code), 0, 0, 4, 0))
        data.extend(code)
    names_offset = len(data)
    data.extend(names)
    data.extend(bytes((-len(data)) % 4))
    section_offset = len(data)
    sections.append(struct.pack('<10I', 1, 3, 0, 0, names_offset, len(names), 0, 0, 1, 0))
    data.extend(b''.join(sections))
    data[:16] = b'\x7fELF\x01\x01\x01' + bytes(9)
    struct.pack_into('<HHIIIIIHHHHHH', data, 16, 2, 8, 1, entry, 52, section_offset,
                     0x20924001, 52, 32, len(entries), 40, len(sections), len(sections) - 1)
    return bytes(data)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-complete', action='store_true')
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error('--input-complete required')
    elf = ROOT / 'work/extracted-001/SLES_523.22'
    metadata = inspect_elf(elf)
    if metadata['sha256'] != ELF_SHA256:
        raise ValueError('Wrong Europe ELF')
    original = elf.read_bytes()
    if hashlib.sha256(original).hexdigest() != ELF_SHA256:
        raise ValueError('ELF changed during read')
    copies = [(0x0051BFA0, 0x80076000, 0x740), (0x0051C6E0, 0x00082000, 0x28),
              (0x0051C770, 0x80075000, 0x330)]
    regions = []
    for source, destination, size in copies:
        matches = [s for s in metadata['load_segments']
                   if s['vaddr'] <= source and source + size <= s['vaddr'] + s['filesz']]
        if len(matches) != 1:
            raise ValueError('Copy source not uniquely file backed')
        segment = matches[0]
        offset = segment['offset'] + source - segment['vaddr']
        regions.append((destination, original[offset:offset + size]))
    translated = translation_elf(regions, 0x80076000)
    if hashlib.sha256(translated).hexdigest() != KERNEL_IMAGE_SHA256:
        raise ValueError('Translation image differs from verified checkpoint')
    output = ROOT / 'work/kernel-image-002'
    output.mkdir(exist_ok=False)
    image = output / 'kernel.elf'
    image.write_bytes(translated)
    (output / 'provenance.json').write_text(json.dumps({
        'source_sha256': ELF_SHA256, 'image': fingerprint(image),
        'copies': copies, 'evidence': 'fn_00328748/fn_0032A560; native-run-005/007 prefix matches',
        'note': 'Private translation input; never replace the boot ELF'
    }, indent=2), encoding='utf-8')
    (output / 'config.toml').write_text('[general]\n'
        f'input = "{image.as_posix()}"\n'
        f'ghidra_output = "{(output / "functions-002.csv").as_posix()}"\n'
        f'output = "{(ROOT / "generated/kernel-004").as_posix()}"\n'
        'single_file_output = false\nlow_memory_mode = true\noutput_worker_threads = 1\n'
        'patch_syscalls = false\npatch_cop0 = false\npatch_cache = false\n'
        'stubs = []\nskip = []\n', encoding='utf-8')
    print(image)


if __name__ == '__main__':
    main()
