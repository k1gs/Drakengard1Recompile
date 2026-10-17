"""Inspect a completed IOPRP ROMFS image; derived bytes stay under ignored work/."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
from intake import ROOT


def inspect_romfs(image, table_offset=0):
    """Bounded ROMFS at an explicit ROMDIR offset; no scanning or execution."""
    if not 64 <= len(image) <= 16 * 1024 * 1024:
        raise ValueError('Unsupported ROMFS image size')
    if table_offset < 0 or table_offset % 16 or table_offset > len(image) - 64:
        raise ValueError('Invalid explicit ROMDIR offset')
    entries = []
    names = set()
    table_size = None
    terminated = False
    offset = 0
    ext_offset = 0
    for index in range(min((len(image) - table_offset) // 16, 4096)):
        position = table_offset + index * 16
        if table_size is not None and position >= table_offset + table_size:
            break
        raw = image[position:position + 16]
        if raw == bytes(16):
            if table_size is None:
                raise ValueError('ROMFS terminator before ROMDIR size')
            if any(image[position:table_offset + table_size]):
                raise ValueError('Nonzero bytes after ROMDIR terminator')
            terminated = True
            break
        name_bytes, ext_size, size = struct.unpack('<10sHI', raw)
        name_bytes, separator, padding = name_bytes.partition(b'\0')
        if separator and any(padding):
            raise ValueError('Nonzero ROMFS name padding')
        name = name_bytes.decode('ascii')
        if not re.fullmatch(r'[A-Z0-9_.-]{1,10}', name) or name in ('.', '..') or (name in names and name != '-'):
            raise ValueError('Invalid or duplicate ROMFS identifier')
        names.add(name)
        if index < 3 and name != ('RESET', 'ROMDIR', 'EXTINFO')[index]:
            raise ValueError('Required ROMFS sections missing or out of order')
        if index == 0 and ((size + 15) & ~15) != table_offset:
            raise ValueError('RESET extent does not locate the specified ROMDIR')
        if index == 1:
            table_size = size
            if size < 64 or size % 16 or size > len(image) - table_offset or size > 4096 * 16:
                raise ValueError('Invalid ROMDIR table size')
        if ext_size % 4:
            raise ValueError('Unaligned EXTINFO record size')
        if offset > len(image) or size > len(image) - offset:
            raise ValueError('ROMFS file extent exceeds input')
        data = image[offset:offset + size]
        entry = {'name': name, 'offset': offset, 'size': size,
                 'extinfo_offset': ext_offset, 'extinfo_size': ext_size,
                 'sha256': hashlib.sha256(data).hexdigest()}
        if data.startswith(b'\x7fELF'):
            if len(data) < 52 or data[4:7] != b'\x01\x01\x01':
                raise ValueError('Unsupported/truncated ELF in ROMFS')
            elf_type, machine, version, address = struct.unpack_from('<HHII', data, 16)
            if machine != 8 or version != 1:
                raise ValueError('Non-MIPS ELF in ROMFS')
            entry['elf'] = {'type': elf_type, 'machine': machine, 'entry': address}
        entries.append(entry)
        offset += (size + 15) & ~15
        ext_offset += ext_size
    if not terminated or len(entries) < 3:
        raise ValueError('Missing bounded ROMDIR terminator')
    if entries[1]['offset'] != table_offset or entries[2]['offset'] != table_offset + table_size:
        raise ValueError('ROMFS section placement mismatch')
    if ext_offset != entries[2]['size']:
        raise ValueError('EXTINFO record lengths do not cover EXTINFO section')
    return entries


def version(image, entries, entry):
    """UDNL compares EXTINFO type-2 values; equal versions prefer later images."""
    extinfo = entries[2]['offset'] + entry['extinfo_offset']
    end = extinfo + entry['extinfo_size']
    result = 0
    seen = False
    while extinfo < end:
        value, length, kind = struct.unpack_from('<HBB', image, extinfo)
        if length % 4 or extinfo + 4 + length > end:
            raise ValueError('Malformed EXTINFO field extent')
        if kind == 2:
            if seen:
                raise ValueError('Ambiguous duplicate module version field')
            result, seen = value, True
        extinfo += 4 + length
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-complete', action='store_true')
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--extract', action='store_true')
    parser.add_argument('--romdir-offset', type=lambda value: int(value, 0), default=0)
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error('--input-complete required before inspecting input')
    try:
        output = args.output.resolve()
        work = (ROOT / 'work').resolve()
        if not output.is_relative_to(work) or output == work:
            raise ValueError('Output must be a new directory inside ignored work/')
        if output.exists():
            raise ValueError('Output already exists; use a fresh experiment directory')
        image_path = args.image.resolve()
        if image_path.stat().st_size > 16 * 1024 * 1024:
            raise ValueError('ROMFS input exceeds supported size')
        image = image_path.read_bytes()
        entries = inspect_romfs(image, args.romdir_offset)
        output.mkdir(parents=True, exist_ok=False)
        if args.extract:
            for entry in entries:
                if entry['name'] == '-':
                    continue  # Repeated anonymous ROM padding extents have no file identity.
                (output / entry['name']).write_bytes(image[entry['offset']:entry['offset'] + entry['size']])
        manifest = {'input_sha256': hashlib.sha256(image).hexdigest(), 'size': len(image),
                    'romdir_offset': args.romdir_offset,
                    'entries': entries, 'extracted': args.extract,
                    'note': 'Static inventory only; no module execution or reboot success established'}
        (output / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        print(f"ROMFS: {len(entries)} entries; metadata saved in {output}")
        return 0
    except (OSError, UnicodeError, ValueError, struct.error) as error:
        print(f'ROMFS refused: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
