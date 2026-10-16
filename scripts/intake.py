"""Read-only PS2 input metadata. Never opens input without explicit completion opt-in."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_BOOT = "SLES_523.22"


def fingerprint(path):
    path = Path(path)
    before = path.stat()
    if not path.is_file() or before.st_size == 0:
        raise ValueError("Input must be a nonempty regular file")
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns, before.st_ino) != (
            after.st_size, after.st_mtime_ns, after.st_ino):
        raise ValueError("Input changed during hashing; wait for completion")
    return {"size": after.st_size, "sha256": digest.hexdigest()}


def inspect_elf(path):
    path = Path(path)
    metadata = fingerprint(path)
    size = metadata["size"]
    with path.open("rb") as stream:
        header = stream.read(52)
        if len(header) != 52 or header[:7] != b"\x7fELF\x01\x01\x01":
            raise ValueError("Expected ELF32 little-endian version 1")
        values = struct.unpack("<HHIIIIIHHHHHH", header[16:])
        kind, machine, version, entry, phoff, _, flags, ehsize, phsize, phnum, _, _, _ = values
        if (kind, machine, version, ehsize) != (2, 8, 1, 52):
            raise ValueError("Expected executable MIPS ELF with standard header")
        if phsize != 32 or not phnum or phoff < 52 or phoff + phnum * phsize > size:
            raise ValueError("Invalid program header table")
        segments = []
        for index in range(phnum):
            stream.seek(phoff + index * phsize)
            ptype, offset, vaddr, paddr, filesz, memsz, pflags, align = struct.unpack(
                "<IIIIIIII", stream.read(32))
            if ptype != 1:
                continue
            if filesz > memsz or offset + filesz > size or vaddr + memsz > 2**32:
                raise ValueError("Invalid PT_LOAD bounds")
            if align not in (0, 1) and (align & (align - 1) or vaddr % align != offset % align):
                raise ValueError("Invalid PT_LOAD alignment")
            segments.append({"offset": offset, "vaddr": vaddr, "paddr": paddr,
                             "filesz": filesz, "memsz": memsz, "flags": pflags,
                             "align": align})
    if not any(s["flags"] & 1 and s["vaddr"] <= entry < s["vaddr"] + s["filesz"]
               for s in segments):
        raise ValueError("Entry is not inside file-backed executable PT_LOAD")
    if fingerprint(path) != metadata:
        raise ValueError("ELF changed during inspection")
    return {**metadata, "entry": entry, "flags": flags, "load_segments": segments}


def boot_path(root, cnf):
    matches = re.findall(r"^\s*BOOT2\s*=\s*cdrom0:[\\/]*(.+?)\s*$", cnf,
                         re.IGNORECASE | re.MULTILINE)
    if len(matches) != 1:
        raise ValueError("Expected exactly one cdrom0 BOOT2 in SYSTEM.CNF")
    name = re.sub(r";\d+$", "", matches[0]).replace("\\", "/")
    relative = PurePosixPath(name)
    if relative.is_absolute() or any(p in ("..", ".") or ":" in p for p in name.split("/")):
        raise ValueError("Unsafe BOOT2 path")
    if relative.name.upper() != EXPECTED_BOOT:
        raise ValueError(f"Wrong boot serial: expected {EXPECTED_BOOT}, got {relative.name}")
    # Resolve ISO's case-insensitive names on Linux too, rejecting ambiguity.
    target = root.resolve()
    for part in relative.parts:
        options = [p for p in target.iterdir() if p.name.casefold() == part.casefold()]
        if len(options) != 1:
            raise ValueError(f"Missing or ambiguous BOOT2 component: {part}")
        target = options[0].resolve()
        if not target.is_relative_to(root.resolve()):
            raise ValueError("BOOT2 escapes extracted tree")
    return target


def inspect_tree(root):
    root = Path(root).resolve()
    if not root.is_dir():
        raise ValueError("Extracted input must be a directory")
    candidates = [p for p in root.iterdir() if p.name.upper() == "SYSTEM.CNF"]
    if len(candidates) != 1:
        raise ValueError("Expected one SYSTEM.CNF at extracted root")
    cnf_path = candidates[0]
    if cnf_path.is_symlink() or cnf_path.stat().st_size > 65536:
        raise ValueError("Unsafe or oversized SYSTEM.CNF")
    cnf_meta = fingerprint(cnf_path)
    cnf = cnf_path.read_text(encoding="ascii")
    if fingerprint(cnf_path) != cnf_meta:
        raise ValueError("SYSTEM.CNF changed during inspection")
    elf_path = boot_path(root, cnf)
    inventory = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError("Symlinks are not accepted in extracted input")
        if path.is_file():
            inventory.append({"path": path.relative_to(root).as_posix(),
                              "size": path.stat().st_size})
    return {"expected_serial": "SLES-52322", "revision_verified": False,
            "system_cnf": cnf_meta, "boot_path": elf_path.relative_to(root).as_posix(),
            "elf": inspect_elf(elf_path), "inventory": inventory}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("image", "tree"))
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--input-complete", action="store_true")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    # Deliberately before stat, resolve or any access to the game input.
    if not args.input_complete:
        parser.error("Confirm completed input with --input-complete before inspection")
    try:
        output = args.output.resolve()
        if not output.is_relative_to((ROOT / "work").resolve()):
            raise ValueError("Reports must be written inside this project's ignored work/")
        if output.exists():
            raise ValueError("Report already exists; use a new filename")
        if args.mode == "image":
            if args.input.suffix.lower() != ".iso":
                raise ValueError("Image mode accepts .iso only (fingerprint, not ISO validation)")
            report = fingerprint(args.input)
        else:
            report = inspect_tree(args.input)
        report.update({"schema_version": 1, "mode": args.mode,
                       "input": str(args.input.resolve()),
                       "created_utc": datetime.now(timezone.utc).isoformat(),
                       "note": "Metadata only; not proof of authenticity or runtime support"})
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2, ensure_ascii=False)
            stream.write("\n")
        print(f"Wrote local report: {output}")
        return 0
    except (OSError, ValueError, struct.error) as error:
        print(f"Intake refused: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
