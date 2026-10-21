"""Bounded diagnostic native launch for the exact verified Europe ELF."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from intake import ROOT, fingerprint
from lift import ELF_SHA256

ISO_SHA256 = "73d3a3e7261ecfb134efbcf84c1b47278817e927dc62b5982f9bf95802be4706"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-complete", action="store_true")
    parser.add_argument("--seconds", type=int, default=20, choices=range(1, 301))
    parser.add_argument("--run", type=int, default=1)
    parser.add_argument("--iop-bootstrap", action="store_true", help="Use the verified local strict IOP bootstrap experiment")
    firmware = parser.add_mutually_exclusive_group()
    firmware.add_argument("--ee-firmware-init", action="store_true", help="Run the isolated reviewed firmware component")
    firmware.add_argument("--ee-firmware-entry", action="store_true", help="Enter the game after guest-owned IOP BOOTEND; experimental")
    parser.add_argument("--iop-video-events", action="store_true", help="Forward shared EE VBlank events to strict IOP; experimental, SIO2 remains incomplete")
    parser.add_argument("--disc", type=Path, default=ROOT / "game/Drakengard (Europe) (En,Fr,De,Es,It).iso")
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error("--input-complete is required")
    if (args.ee_firmware_init or args.ee_firmware_entry) and not args.iop_bootstrap:
        parser.error("Firmware modes require --iop-bootstrap")
    if not 1 <= args.run <= 999:
        parser.error("run must be 1..999")
    if args.iop_video_events and not args.ee_firmware_entry:
        parser.error("IOP video events require --ee-firmware-entry")
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
        elif args.ee_firmware_entry:
            command.append("--ee-firmware-entry")
            result["ee_firmware_entry_experiment"] = True
        environment = os.environ.copy()
        environment.pop("DRAKENGARD_FRAME_OUTPUT", None)
        environment.pop("DRAKENGARD_EE_RAM_OUTPUT", None)
        environment.pop("DRAKENGARD_IOP_VIDEO_EVENTS", None)
        result["iop_video_events_experiment"] = args.iop_video_events
        if args.iop_video_events:
            environment["DRAKENGARD_IOP_VIDEO_EVENTS"] = "1"
        frame = run / "first-guest-frame.ppm"
        if args.ee_firmware_entry:
            environment["DRAKENGARD_FRAME_OUTPUT"] = str(frame)
            environment["DRAKENGARD_EE_RAM_OUTPUT"] = str(run / "ee-progress-ram.bin")
        with (run / "runtime.log").open("x", encoding="utf-8") as log:
            try:
                child = subprocess.run(command, cwd=elf.parent,
                                       stdout=log, stderr=subprocess.STDOUT, timeout=args.seconds, env=environment)
                result["exit_code"] = child.returncode
            except subprocess.TimeoutExpired:
                result.update({"exit_code": None, "timed_out": True})
        result["disc_verified_by_native"] = "Verified Europe ISO SHA-256" in (run / "runtime.log").read_text(encoding="utf-8", errors="replace")
        result["guest_frame_candidate"] = frame.name if frame.is_file() else None
        followup = run / "review-guest-frame.ppm"
        result["guest_frame_followup"] = followup.name if followup.is_file() else None
        (run / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, indent=2))
        return 124 if result["timed_out"] else result["exit_code"]
    except (OSError, ValueError) as error:
        print(f"Native launch refused: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
