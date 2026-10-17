"""Prepare private units for the reviewed BIOS EE SIF initialization experiment."""
import csv
import hashlib
from pathlib import Path
import re
import sys
import json
from intake import ROOT, inspect_elf
from lift import validate_map
from prepare_kernel import write_changed

BIOS_SHA256 = 'ee7ae4cc152588b7da1dab17494a321ceafafaf799769baea4e8a28afa5044b1'
KERNEL_SHA256 = '7caa05e1a4e1a6b81ed38016039e6d865567faf5af2b842afff55781da7d5a21'
IMAGE_SHA256 = '9884ddf3c1e80a8fd9b89d83b741b3b64ec509c0edf773bd889048eaf3d55dd8'


def prepare(output):
    image_dir = ROOT / 'work/ee-bios-kernel-001'
    image = image_dir / 'sif.elf'
    metadata = inspect_elf(image)
    if metadata['sha256'] != IMAGE_SHA256 or metadata['entry'] != 0x80006240:
        raise ValueError('Firmware translation image differs from reviewed experiment')
    table = image_dir / 'sif-functions-002.csv'
    validate_map(table, metadata)
    rows = list(csv.DictReader(table.open()))
    if [(int(r['Start'], 0), int(r['End'], 0)) for r in rows] != [
        (0x80005f00, 0x80005fdc), (0x80006240, 0x80006348), (0x80006348, 0x8000637c),
        (0x800063b0, 0x80006414), (0x80006b90, 0x80006ba4),
    ]:
        raise ValueError('Firmware code coverage changed')
    rom = (ROOT / 'work/pcsx2-reference/bios/Europe.bin').read_bytes()
    manifest = json.loads((ROOT / 'work/bios-romdir-001/manifest.json').read_text())
    if hashlib.sha256(rom).hexdigest() != BIOS_SHA256 or manifest['input_sha256'] != BIOS_SHA256:
        raise ValueError('Firmware source BIOS differs')
    records = [r for r in manifest['entries'] if r['name'] == 'KERNEL']
    if len(records) != 1:
        raise ValueError('Ambiguous firmware source extent')
    record = records[0]
    kernel = rom[record['offset']:record['offset'] + record['size']]
    if len(kernel) != 93544 or hashlib.sha256(kernel).hexdigest() != KERNEL_SHA256:
        raise ValueError('Firmware source extent differs')
    image_bytes = image.read_bytes()
    segment = metadata['load_segments'][0]
    if image_bytes[segment['offset']:segment['offset'] + len(kernel)] != kernel:
        raise ValueError('Firmware translation bytes do not match source ROM')
    output = output.resolve()
    if not output.is_relative_to(ROOT / 'build'):
        raise ValueError('Firmware compiler units must stay inside private build/')
    output.mkdir(parents=True, exist_ok=True)
    write_changed(output / 'firmware_functions.h', (ROOT / 'native/runner/firmware_functions.h').read_text())
    source = ROOT / 'generated/firmware-003'
    for path in source.glob('*'):
        if path.name == 'register_functions.cpp' or path.suffix not in ('.cpp', '.h'):
            continue
        name = path.name.replace('ps2_recompiled_', 'firmware_recompiled_')
        text = path.read_text().replace('ps2_recompiled_', 'firmware_recompiled_')
        text = text.replace('PS2_RECOMPILED_', 'FIRMWARE_RECOMPILED_')
        if path.suffix == '.cpp':
            text = '#include "firmware_functions.h"\n' + text
            text, count = re.subn(r'(void fn_\w+\(uint8_t\* rdram, R5900Context\* ctx, PS2Runtime \*runtime\) \{)',
                r'\1\n    verifyFirmwareInstructions(rdram, ctx->pc);', text)
            if count != 1:
                raise ValueError('Generated firmware function signature changed')
        write_changed(output / name, text)
    registration = (source / 'register_functions.cpp').read_text()
    base_match = re.search(r'g_ps2RecompiledFunctionTableBase = (0x[0-9a-fA-F]+)u;', registration)
    entries = re.findall(r'g_ps2RecompiledFunctionTable\[(\d+)\] = (fn_[A-Za-z0-9_]+);', registration)
    if not base_match or not entries:
        raise ValueError('Missing firmware registration')
    base = int(base_match[1], 16)
    content = ['const FirmwareEntry firmwareEntries[] = {']
    for slot, name in sorted(entries, key=lambda pair: int(pair[0])):
        content.append(f'{{0x{base + int(slot) * 4:08x}u, {name}}},')
    content.append('};')
    ranges = []
    for i, row in enumerate(rows):
        start, end = int(row['Start'], 0), int(row['End'], 0)
        code = kernel[start - 0x80000000:end - 0x80000000]
        content.append(f'const uint8_t firmwareCode{i}[] = {{' + ','.join(map(str, code)) + '};')
        ranges.append(f'{{0x{start:08x}u, firmwareCode{i}, sizeof(firmwareCode{i})}},')
    content.extend(['const FirmwareCode firmwareCode[] = {', *ranges, '};'])
    content.append(f'constexpr uint32_t FirmwareRomOffset = {record["offset"]}u, FirmwareSize = {len(kernel)}u;')
    write_changed(output / 'firmware_bindings.h', '\n'.join(content))
    print(f'Prepared {len(entries)} exact firmware entries and {sum(int(r["Size"]) for r in rows)} instruction bytes')


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] != '--input-complete':
        raise SystemExit('Pass --input-complete and a private build output directory')
    prepare(Path(sys.argv[2]))
