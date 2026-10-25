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


def parse_int0(text):
    try:
        return int(text, 0)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected an integer accepted by int(..., 0)") from error


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
    parser.add_argument("--virtual-pad", action="store_true", help="Emulate DualShock2 port1 from keyboard/gamepad input; requires --iop-video-events")
    parser.add_argument("--pad-control", action="store_true", help="Use an explicit bounded host controller command file instead of window/keyboard input")
    parser.add_argument("--gs-metrics", action="store_true", help="Record bounded GS production/state diagnostics; adds observation overhead, not a game FPS counter")
    parser.add_argument("--ee-write-watch", type=parse_int0, default=None,
                        help="Watch one aligned 8-byte EE RAM slot; requires --ee-firmware-entry")
    parser.add_argument("--ee-write-watch-arm-value", type=parse_int0, default=None,
                        help="Optional 32-bit arm value for --ee-write-watch")
    parser.add_argument("--iop-runtime-millions", type=int, choices=(256, 512, 1024), default=256,
                        help="Finite post-reboot IOP budget in millions; larger presets require --ee-firmware-entry")
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
    if args.virtual_pad and not args.iop_video_events:
        parser.error("Virtual pad requires --iop-video-events")
    if args.pad_control and not args.virtual_pad:
        parser.error("Pad control requires --virtual-pad")
    if args.iop_runtime_millions != 256 and not args.ee_firmware_entry:
        parser.error("Nondefault IOP runtime budgets require --ee-firmware-entry")
    if args.ee_write_watch is not None:
        if not args.ee_firmware_entry:
            parser.error("EE write watch requires --ee-firmware-entry")
        if args.ee_write_watch < 0 or args.ee_write_watch > 32 * 1024 * 1024 - 8 or args.ee_write_watch % 8:
            parser.error("EE write watch address must be 8-byte aligned and within the 32 MiB EE RAM")
    if args.ee_write_watch_arm_value is not None:
        if args.ee_write_watch is None:
            parser.error("EE write watch arm value requires --ee-write-watch")
        if not 0 <= args.ee_write_watch_arm_value <= 0xFFFFFFFF:
            parser.error("EE write watch arm value must be a 32-bit unsigned integer")
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
                  "iop_runtime_budget_millions": args.iop_runtime_millions,
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
        environment.pop("DRAKENGARD_EE_FAILURE_OUTPUT", None)
        environment.pop("DRAKENGARD_EE_WRITE_WATCH", None)
        environment.pop("DRAKENGARD_EE_WRITE_WATCH_ARM_VALUE", None)
        environment.pop("DRAKENGARD_IOP_VIDEO_EVENTS", None)
        environment.pop("DRAKENGARD_VIRTUAL_PAD", None)
        environment.pop("DRAKENGARD_PAD_CONTROL", None)
        environment.pop("DRAKENGARD_GS_COLOR_OUTPUT", None)
        environment.pop("DRAKENGARD_GS_METRICS_OUTPUT", None)
        result["gs_metrics_experiment"] = args.gs_metrics
        if args.gs_metrics:
            environment["DRAKENGARD_GS_METRICS_OUTPUT"] = str(run / "gs-metrics.jsonl")
        environment.pop("DRAKENGARD_IOP_RUNTIME_BUDGET", None)
        result["virtual_pad_experiment"] = args.virtual_pad
        result["pad_control_experiment"] = args.pad_control
        if args.pad_control:
            control = run / "pad-control.txt"
            with control.open("x", encoding="ascii", newline="\n") as initial:
                initial.write("D1PAD1 0 FFFF 0\n")
            environment["DRAKENGARD_PAD_CONTROL"] = str(control)
        result["ee_write_watch"] = (f"0x{args.ee_write_watch:x}"
                                     if args.ee_write_watch is not None else None)
        result["ee_write_watch_arm_value"] = (f"0x{args.ee_write_watch_arm_value:08x}"
                                               if args.ee_write_watch_arm_value is not None else None)
        if args.ee_write_watch is not None:
            environment["DRAKENGARD_EE_WRITE_WATCH"] = f"0x{args.ee_write_watch:x}"
        if args.ee_write_watch_arm_value is not None:
            environment["DRAKENGARD_EE_WRITE_WATCH_ARM_VALUE"] = f"0x{args.ee_write_watch_arm_value:08x}"
        if args.virtual_pad:
            environment["DRAKENGARD_VIRTUAL_PAD"] = "1"
        result["iop_video_events_experiment"] = args.iop_video_events
        if args.iop_video_events:
            environment["DRAKENGARD_IOP_VIDEO_EVENTS"] = "1"
        if args.iop_runtime_millions != 256:
            environment["DRAKENGARD_IOP_RUNTIME_BUDGET"] = str(args.iop_runtime_millions)
        frame = run / "first-guest-frame.ppm"
        if args.ee_firmware_entry:
            environment["DRAKENGARD_FRAME_OUTPUT"] = str(frame)
            environment["DRAKENGARD_EE_RAM_OUTPUT"] = str(run / "ee-progress-ram.bin")
            environment["DRAKENGARD_EE_FAILURE_OUTPUT"] = str(run / "ee-invalid-transfer-ram.bin")
            environment["DRAKENGARD_GS_COLOR_OUTPUT"] = str(run / "gs-colors.txt")
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
        final_frame = run / "last-guest-frame.ppm"
        result["guest_frame_final"] = final_frame.name if final_frame.is_file() else None
        (run / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, indent=2))
        return 124 if result["timed_out"] else result["exit_code"]
    except (OSError, ValueError) as error:
        print(f"Native launch refused: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
