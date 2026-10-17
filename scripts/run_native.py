"""Bounded diagnostic native launch for the exact verified Europe ELF."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from intake import ROOT, fingerprint
from lift import ELF_SHA256

ISO_SHA256 = "73d3a3e7261ecfb134efbcf84c1b47278817e927dc62b5982f9bf95802be4706"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-complete", action="store_true")
    parser.add_argument("--seconds", type=int, default=20, choices=range(1, 61))
    parser.add_argument("--run", type=int, default=1)
    parser.add_argument("--iop-bootstrap", action="store_true", help="Use the verified local strict IOP bootstrap experiment")
    parser.add_argument("--ee-firmware-init", action="store_true", help="Execute reviewed EE firmware initialization before game entry")
    parser.add_argument("--disc", type=Path, default=ROOT / "game/Drakengard (Europe) (En,Fr,De,Es,It).iso")
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error("--input-complete is required")
    if args.ee_firmware_init and not args.iop_bootstrap:
        parser.error("--ee-firmware-init requires --iop-bootstrap")
    if not 1 <= args.run <= 999:
        parser.error("run must be 1..999")
    try:
        elf = ROOT / "work/extracted-001/SLES_523.22"
        if fingerprint(elf)["sha256"] != ELF_SHA256:
            raise ValueError("Wrong boot ELF SHA-256")
        disc = args.disc.resolve()
        if not disc.is_file():
            raise ValueError("Missing completed ISO; pass --disc explicitly")
        exe = ROOT / "build/native/ps2xRuntime/Release/ps2EntryRunner.exe"
        executable = fingerprint(exe)
        run = ROOT / "work" / f"native-run-{args.run:03d}"
        run.mkdir(parents=True, exist_ok=False)
        result = {"elf_sha256": ELF_SHA256, "disc_expected_sha256": ISO_SHA256, "executable": executable,
                  "timeout_seconds": args.seconds, "timed_out": False,
                  "note": "Diagnostic execution only; no playable-port claim"}
        command = [str(exe), str(elf), str(disc)]
        if args.iop_bootstrap:
            result["iop_inputs"] = {}
            for name, relative in (
                ("bundle", "work/iop-boot-plan-001/boot-plan.bin"),
                ("hardware", "work/iop-hardware-002/hardware-state.bin"),
                ("rom", "work/pcsx2-reference/bios/Europe.bin"),
                ("mecha", "work/pcsx2-reference/bios/Europe.mec"),
            ):
                path = ROOT / relative
                metadata = fingerprint(path)
                result["iop_inputs"][name] = metadata
                command.extend((str(path), metadata["sha256"]))
        if args.ee_firmware_init:
            command.append("--ee-firmware-init")
            result["ee_firmware_init_experiment"] = True
        with (run / "runtime.log").open("x", encoding="utf-8") as log:
            try:
                child = subprocess.run(command, cwd=elf.parent,
                                       stdout=log, stderr=subprocess.STDOUT, timeout=args.seconds)
                result["exit_code"] = child.returncode
            except subprocess.TimeoutExpired:
                result.update({"exit_code": None, "timed_out": True})
        result["disc_verified_by_native"] = "Verified Europe ISO SHA-256" in (run / "runtime.log").read_text(encoding="utf-8", errors="replace")
        (run / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, indent=2))
        return 124 if result["timed_out"] else result["exit_code"]
    except (OSError, ValueError) as error:
        print(f"Native launch refused: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
