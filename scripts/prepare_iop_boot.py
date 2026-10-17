"""Prepare a private, bounded two-image UDNL bootstrap experiment; never signal boot success."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
from intake import ROOT
from romdir import inspect_romfs, version


def boot_order(config):
    start = None
    names = []
    for line in config.decode('ascii').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if line.startswith('@'):
            if start is not None or not re.fullmatch(r'@[0-9a-fA-F]{1,8}', line):
                raise ValueError('Invalid/duplicate boot start address')
            start = int(line[1:], 16)
        elif re.fullmatch(r'[A-Z0-9_.-]{1,10}', line) and line not in ('.', '..', '-'):
            if line in names:
                raise ValueError('Duplicate boot module')
            names.append(line)
        else:
            raise ValueError('Unsupported boot directive or module arguments')
    if start is None or start % 16 or start > 0x100000 or names[:2] != ['SYSMEM', 'LOADCORE']:
        raise ValueError('Unsupported boot layout')
    if not 2 <= len(names) <= 128:
        raise ValueError('Unsupported module count')
    return start, names


def select_modules(images, names):
    selected = []
    for name in names:
        candidates = []
        for index, (image, entries) in enumerate(images):
            for entry in entries:
                if entry['name'] == name:
                    candidates.append((version(image, entries, entry), index, entry))
        if not candidates:
            raise ValueError(f'Missing requested module: {name}')
        revision, index, entry = max(candidates, key=lambda item: item[:2])
        if 'elf' not in entry:
            raise ValueError(f'Requested module is not a supported ELF: {name}')
        selected.append((revision, index, entry))
    return selected


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-complete', action='store_true')
    parser.add_argument('--base-rom', type=Path, required=True)
    parser.add_argument('--base-romdir-offset', type=lambda value: int(value, 0), required=True)
    parser.add_argument('--update', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error('--input-complete required before inspecting inputs')
    try:
        output = args.output.resolve(); work = (ROOT / 'work').resolve()
        if not output.is_relative_to(work) or output == work or output.exists():
            raise ValueError('Use a new output directory inside ignored work/')
        images = []
        for path, offset in [(args.base_rom, args.base_romdir_offset), (args.update, 0)]:
            if path.stat().st_size > 16 * 1024 * 1024:
                raise ValueError('Input exceeds ROMFS bound')
            data = path.read_bytes()
            images.append((data, inspect_romfs(data, offset)))
        configs = [(index, entry) for index, (_, entries) in enumerate(images)
                   for entry in entries if entry['name'] == 'IOPBTCONF']
        if not configs:
            raise ValueError('No actual IOPBTCONF found')
        config_index, config = configs[-1]
        start, names = boot_order(images[config_index][0][config['offset']:config['offset'] + config['size']])
        selected = select_modules(images, names)
        bundle = bytearray(b'D1IOP01\0' + struct.pack('<II', start, len(selected)))
        metadata = []
        for revision, index, entry in selected:
            data = images[index][0][entry['offset']:entry['offset'] + entry['size']]
            bundle.extend(struct.pack('<10sHI', entry['name'].encode('ascii'), 0, len(data)))
            bundle.extend(data)
            metadata.append({'name': entry['name'], 'source_index': index, 'version': revision,
                             'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
        if len(bundle) > 512 * 1024:
            raise ValueError('Bootstrap source pool exceeds supported bound')
        output.mkdir(parents=True, exist_ok=False)
        (output / 'boot-plan.bin').write_bytes(bundle)
        (output / 'manifest.json').write_text(json.dumps({
            'sources_sha256': [hashlib.sha256(image).hexdigest() for image, _ in images],
            'start': start, 'config_source_index': config_index, 'modules': metadata,
            'bundle_sha256': hashlib.sha256(bundle).hexdigest(),
            'note': 'Private experimental boot inputs only; no reboot/menu success established'
        }, indent=2), encoding='utf-8')
        print(f'Private boot plan: {len(selected)} modules; start={start:#x}; {len(bundle)} bytes')
        return 0
    except (OSError, UnicodeError, ValueError, struct.error) as error:
        print(f'IOP boot plan refused: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
