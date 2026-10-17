"""Export reviewed passive IOP register state from a completed PCSX2 2.8.2 snapshot."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import zipfile
from intake import ROOT


def hardware_seed(snapshot):
    if not 64 <= len(snapshot) <= 128 * 1024 * 1024:
        raise ValueError('Unsupported snapshot size')
    with zipfile.ZipFile(io.BytesIO(snapshot)) as archive:
        names = archive.namelist()
        for name in ('PCSX2 Savestate Version.id', 'iopHwRegs.bin'):
            if names.count(name) != 1:
                raise ValueError('Missing or ambiguous snapshot component')
        version_info = archive.getinfo('PCSX2 Savestate Version.id')
        hardware_info = archive.getinfo('iopHwRegs.bin')
        if version_info.file_size != 36 or hardware_info.file_size != 65536:
            raise ValueError('Unexpected snapshot component size')
        version = archive.read(version_info)
        if version[:4] != struct.pack('<I', 0x9a590000) or version[4:].rstrip(b'\0') != b'v2.8.2':
            raise ValueError('Snapshot layout/revision differs from pinned PCSX2 2.8.2')
        hardware = archive.read(hardware_info)
    # HW_ICFG is plain stored read/write state in the pinned reference.
    # Active DMA, IRQ, timers and device replies are deliberately not imported.
    value, = struct.unpack_from('<I', hardware, 0x1450)
    gate, = struct.unpack_from('<I', hardware, 0x1578)
    if gate not in (0, 1):
        raise ValueError('Unreviewed DMA-global gate bits')
    seed = b'D1HW001\0' + struct.pack('<IIIII', 2, 0x1f801450, value, 0x1f801578, gate)
    return seed, {'hw_icfg': value, 'dma_global_gate': gate,
                  'iop_hardware_sha256': hashlib.sha256(hardware).hexdigest()}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-complete', action='store_true')
    parser.add_argument('--state', type=Path, required=True)
    parser.add_argument('--expected-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error('--input-complete required before reading a snapshot')
    try:
        output = args.output.resolve(); work = (ROOT / 'work').resolve()
        if not output.is_relative_to(work) or output == work or output.exists():
            raise ValueError('Use a fresh private directory inside ignored work/')
        if args.state.stat().st_size > 128 * 1024 * 1024:
            raise ValueError('Snapshot exceeds supported bound')
        snapshot = args.state.read_bytes()
        digest = hashlib.sha256(snapshot).hexdigest()
        if digest != args.expected_sha256:
            raise ValueError('Snapshot hash differs from reviewed experiment')
        seed, metadata = hardware_seed(snapshot)
        metadata.update({'snapshot_sha256': digest, 'seed_sha256': hashlib.sha256(seed).hexdigest(),
                         'reference': 'PCSX2 2.8.2', 'note': 'Passive configuration only; no boot/device completion'})
        output.mkdir(parents=True, exist_ok=False)
        (output / 'hardware-state.bin').write_bytes(seed)
        (output / 'provenance.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
        print(json.dumps(metadata, indent=2))
        return 0
    except (OSError, ValueError, zipfile.BadZipFile, NotImplementedError) as error:
        print(f'Reference state refused: {error}', file=sys.stderr); return 2


if __name__ == '__main__':
    sys.exit(main())
