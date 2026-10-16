"""Prepare private compiler units and exact sparse bindings for a kernel lift."""
import csv
from pathlib import Path
import re
import sys
from intake import ROOT, inspect_elf
from lift import ELF_SHA256, validate_map
from kernel_image import KERNEL_IMAGE_SHA256
import json


def write_changed(path, text):
    if not path.is_file() or path.read_text(encoding='utf-8') != text:
        path.write_text(text, encoding='utf-8')


def prepare(output):
    source = ROOT / 'generated/kernel-004'
    image_dir = ROOT / 'work/kernel-image-002'
    image = image_dir / 'kernel.elf'
    provenance = json.loads((image_dir / 'provenance.json').read_text())
    metadata = inspect_elf(image)
    if (provenance['source_sha256'] != ELF_SHA256 or
            provenance['image']['sha256'] != metadata['sha256'] or
            metadata['sha256'] != KERNEL_IMAGE_SHA256):
        raise ValueError('Kernel provenance mismatch')
    validate_map(image_dir / 'functions-002.csv', metadata)
    output = output.resolve()
    if not output.is_relative_to(ROOT / 'build'):
        raise ValueError('Compiler units must remain private in build/')
    output.mkdir(parents=True, exist_ok=True)
    write_changed(output / 'kernel_functions.h',
                  (ROOT / 'native/runner/kernel_functions.h').read_text(encoding='utf-8'))
    for path in source.glob('*'):
        if path.name == 'register_functions.cpp' or path.suffix not in ('.cpp', '.h'):
            continue
        name = path.name.replace('ps2_recompiled_', 'kernel_recompiled_')
        text = path.read_text().replace('ps2_recompiled_functions.h', 'kernel_recompiled_functions.h')
        text = text.replace('ps2_recompiled_stubs.h', 'kernel_recompiled_stubs.h')
        if path.suffix == '.cpp':
            text = '#include "kernel_functions.h"\n' + text
            text, count = re.subn(r'(void fn_\w+\(uint8_t\* rdram, R5900Context\* ctx, PS2Runtime \*runtime\) \{)',
                r'\1\n    verifyKernelInstructions(rdram, ctx->pc);', text)
            if count != 1:
                raise ValueError('Generated function signature contract changed')
        write_changed(output / name, text)
    registration = (source / 'register_functions.cpp').read_text()
    base_match = re.search(r'g_ps2RecompiledFunctionTableBase = (0x[0-9a-fA-F]+)u;', registration)
    if not base_match:
        raise ValueError('Generated registration contract changed')
    base = int(base_match[1], 16)
    entries = re.findall(r'g_ps2RecompiledFunctionTable\[(\d+)\] = (fn_[A-Za-z0-9_]+);', registration)
    if not entries:
        raise ValueError('Empty kernel registration')
    content = ['const KernelEntry kernelEntries[] = {']
    for slot, function in sorted(entries, key=lambda item: int(item[0])):
        content.append(f'{{0x{base + int(slot) * 4:08x}u, {function}}},')
    content.append('};')
    regions = []
    image_bytes = image.read_bytes()
    for index, row in enumerate(csv.DictReader((image_dir / 'functions-002.csv').open())):
        start, end = int(row['Start'], 0), int(row['End'], 0)
        segment = next(s for s in metadata['load_segments']
                       if s['vaddr'] <= start and end <= s['vaddr'] + s['filesz'])
        offset = segment['offset'] + start - segment['vaddr']
        code = image_bytes[offset:offset + end - start]
        name = f'kernelCode{index}'
        content.append(f'const uint8_t {name}[] = {{' + ','.join(str(byte) for byte in code) + '};')
        regions.append(f'{{0x{start:08x}u, {name}, sizeof({name})}},')
    content.extend(['const KernelCode kernelCode[] = {', *regions, '};'])
    content.append('const KernelRegion kernelRegions[] = {')
    for segment in metadata['load_segments']:
        content.append(f'{{0x{segment["vaddr"]:08x}u, 0x{segment["vaddr"] + segment["filesz"]:08x}u}},')
    content.append('};')
    write_changed(output / 'kernel_bindings.h', '\n'.join(content))
    print(f'Prepared {len(entries)} exact kernel entries with {len(regions)} code ranges')


if __name__ == '__main__':
    prepare(Path(sys.argv[1]))
