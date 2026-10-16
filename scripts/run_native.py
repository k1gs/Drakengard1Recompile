"""Bounded diagnostic native launch for the exact verified Europe ELF."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from intake import ROOT, fingerprint
from lift import ELF_SHA256


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-complete", action="store_true")
    parser.add_argument("--seconds", type=int, default=20, choices=range(1, 61))
    parser.add_argument("--run", type=int, default=1)
    args = parser.parse_args(argv)
    if not args.input_complete:
        parser.error("--input-complete is required")
    if not 1 <= args.run <= 999:
        parser.error("run must be 1..999")
    try:
        elf = ROOT / "work/extracted-001/SLES_523.22"
        if fingerprint(elf)["sha256"] != ELF_SHA256:
            raise ValueError("Wrong boot ELF SHA-256")
        exe = ROOT / "build/native/ps2xRuntime/Release/ps2EntryRunner.exe"
        executable = fingerprint(exe)
        run = ROOT / "work" / f"native-run-{args.run:03d}"
        run.mkdir(parents=True, exist_ok=False)
        result = {"elf_sha256": ELF_SHA256, "executable": executable,
                  "timeout_seconds": args.seconds, "timed_out": False,
                  "note": "Diagnostic execution only; no playable-port claim"}
        with (run / "runtime.log").open("x", encoding="utf-8") as log:
            try:
                child = subprocess.run([str(exe), str(elf)], cwd=elf.parent,
                                       stdout=log, stderr=subprocess.STDOUT, timeout=args.seconds)
                result["exit_code"] = child.returncode
            except subprocess.TimeoutExpired:
                result.update({"exit_code": None, "timed_out": True})
        (run / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, indent=2))
        return 124 if result["timed_out"] else result["exit_code"]
    except (OSError, ValueError) as error:
        print(f"Native launch refused: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
